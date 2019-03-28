/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#ifndef __FreeBSD__
	#include <linux/spi/spidev.h>
#endif

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include <wiringx.h>

#include "../../core/log.h"
#include "../../config/config.h"
#include "../table.h"
#include "../io.h"

typedef struct lua_spi_t {
	struct plua_metatable_t *table;
	struct plua_module_t *module;
	lua_State *L;

	int channel;
	int speed;
	int fd;

} lua_spi_t;

typedef struct spi_fd_list_t {
	struct lua_spi_t *spi;
	struct spi_fd_list_t *next;
} spi_fd_list_t;

#ifdef _WIN32
	static uv_mutex_t lock;
#else
	static pthread_mutex_t lock;
	static pthread_mutexattr_t lock_attr;
#endif
static struct spi_fd_list_t *spi_fd_list = NULL;
static int gc_regged = 0;
static int mutex_init = 0;

/*
 * Remove from list
 */
static void plua_io_spi_gc(void *ptr, int rm_from_list) {
	struct lua_spi_t *data = ptr;

	if(rm_from_list == 1) {
#ifdef _WIN32
		uv_mutex_lock(&lock);
#else
		pthread_mutex_lock(&lock);
#endif

		struct spi_fd_list_t *currP, *prevP;

		prevP = NULL;

		for(currP = spi_fd_list; currP != NULL; prevP = currP, currP = currP->next) {
			if(currP->spi == data) {
				if(prevP == NULL) {
					spi_fd_list = currP->next;
				} else {
					prevP->next = currP->next;
				}

				FREE(currP);
				break;
			}
		}
#ifdef _WIN32
		uv_mutex_unlock(&lock);
#else
		pthread_mutex_unlock(&lock);
#endif
	}

	plua_metatable_free(data->table);

	FREE(data);
}

static void plua_io_spi_global_gc(void *ptr) {
#ifdef _WIN32
	uv_mutex_lock(&lock);
#else
	pthread_mutex_lock(&lock);
#endif
	struct spi_fd_list_t *node = NULL;
	while(spi_fd_list) {
		node = spi_fd_list;

		plua_io_spi_gc(node->spi, 0);

		spi_fd_list = spi_fd_list->next;
		FREE(node);
	}
#ifdef _WIN32
	uv_mutex_unlock(&lock);
#else
	pthread_mutex_unlock(&lock);
#endif

	gc_regged = 0;
}

static int plua_io_spi_rw(struct lua_State *L) {
	struct lua_spi_t *spi = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "spi.rw requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(spi == NULL) {
		luaL_error(L, "internal error: spi object not passed");
	}

	uint8_t *tmp = NULL;
	int nr = 0;

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "table expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TTABLE),
			1, buf);

		if(lua_type(L, -1) == LUA_TTABLE) {
			lua_pushnil(L);
			while(lua_next(L, -2) != 0) {
				if(lua_type(L, -2) != LUA_TNUMBER || lua_type(L, -1) != LUA_TNUMBER) {
					lua_pop(L, 3);
					luaL_error(L, "spi.rw table should contain both nummeric keys and values", lua_gettop(L));
				}

				if((tmp = REALLOC(tmp, sizeof(uint8_t *)*(nr+1))) == NULL) {
					OUT_OF_MEMORY
				}
				tmp[nr] = (uint8_t)lua_tonumber(L, -1);
				nr++;

				lua_pop(L, 1);
			}
		}
		lua_remove(L, -1);
	}

	uint8_t *rxbuf = (void *)tmp;

	wiringXSPIDataRW(spi->channel, (unsigned char *)tmp, nr);

	if((rxbuf[0] & 0x80) == 0) { // read
		lua_pushnumber(L, tmp[1]);
		FREE(tmp);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	FREE(tmp);
	assert(lua_gettop(L) == 0);
	return 0;
}

static void plua_io_spi_object(lua_State *L, struct lua_spi_t *spi) {
	lua_newtable(L);

	lua_pushstring(L, "rw");
	lua_pushlightuserdata(L, spi);
	lua_pushcclosure(L, plua_io_spi_rw, 1);
	lua_settable(L, -3);
}

int plua_io_spi(struct lua_State *L) {
	if(lua_gettop(L) != 1 && lua_gettop(L) != 2) {
		luaL_error(L, "spi requires 1 or 2 arguments, %d given", lua_gettop(L));
		return 0;
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";
	int speed = 10000000, channel = -1;

	if(lua_gettop(L) == 2) {
		{
			sprintf(p, error, lua_typename(L, lua_type(L, -1)));

			luaL_argcheck(L,
				(lua_type(L, -1) == LUA_TNUMBER),
				1, buf);

			if(lua_type(L, -1) == LUA_TNUMBER) {
				speed = (int)lua_tonumber(L, -1);
				lua_remove(L, -1);
			}
		}
	}

	{
		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, -1) == LUA_TNUMBER) {
			channel = (int)lua_tonumber(L, -1);
			lua_remove(L, -1);
		}
	}

	if(mutex_init == 0) {
		mutex_init = 1;
#ifdef _WIN32
		uv_mutex_init(&lock);
#else
		pthread_mutexattr_init(&lock_attr);
		pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&lock, &lock_attr);
#endif
	}

#ifdef _WIN32
	uv_mutex_lock(&lock);
#else
	pthread_mutex_lock(&lock);
#endif
	int match = 0;
	struct spi_fd_list_t *node = spi_fd_list;
	while(node) {
		if(node->spi->channel == channel) {
			match = 1;
			break;
		}
		node = node->next;
	}
#ifdef _WIN32
	uv_mutex_unlock(&lock);
#else
	pthread_mutex_unlock(&lock);
#endif

	if(match == 1 && wiringXSPIGetFd(channel) == 0) {
		logprintf(LOG_ERR, "known SPI channel (%d) is unknown to wiringX", channel);
		lua_pushnil(L);

		lua_assert(lua_gettop(L) == 1);

		return 1;
	}

	if(match == 0) {
		if(wiringXSPISetup(channel, speed) == -1) {
			lua_pushnil(L);
			lua_assert(lua_gettop(L) == 1);
			return 1;
		} else {
			struct lua_state_t *state = plua_get_current_state(L);
			if(state == NULL) {
				return 0;
			}

			struct lua_spi_t *lua_spi = MALLOC(sizeof(struct lua_spi_t));
			if(lua_spi == NULL) {
				OUT_OF_MEMORY
			}
			memset(lua_spi, '\0', sizeof(struct lua_spi_t));

			plua_metatable_init(&lua_spi->table);

			lua_spi->channel = channel;
			lua_spi->speed = speed;
			lua_spi->fd = wiringXSPIGetFd(channel);

			lua_spi->module = state->module;
			lua_spi->L = L;

#ifdef _WIN32
			uv_mutex_lock(&lock);
#else
			pthread_mutex_lock(&lock);
#endif
			if((node = MALLOC(sizeof(struct spi_fd_list_t))) == NULL) {
				OUT_OF_MEMORY
			}
			node->spi = lua_spi;
			node->next = spi_fd_list;
			spi_fd_list = node;
#ifdef _WIN32
			uv_mutex_unlock(&lock);
#else
			pthread_mutex_unlock(&lock);
#endif

			plua_io_spi_object(L, lua_spi);
		}
	} else {
		plua_io_spi_object(L, node->spi);
	}

	if(gc_regged == 0) {
		gc_regged = 1;
		plua_gc_reg(NULL, NULL, plua_io_spi_global_gc);
	}

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
