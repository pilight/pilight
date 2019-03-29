/*
	Copyright (C) 2013 - 2016 CurlyMo

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

#include <wiringx.h>

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "lua.h"
#include "table.h"
#include "../core/log.h"

struct lua_wiringx_t;

typedef struct lua_wiringx_gpio_t {
	struct lua_wiringx_t *parent;

	int gpio;
	int mode;

	struct {
		unsigned long first;
		unsigned long second;
	} timestamp;

	struct {
		int rbuffer[2][1024];
		int idx;
		int rptr;
	} data;

	uv_poll_t *poll_req;
	uv_timer_t *timer_req;
	char *callback;
} lua_wiringx_gpio_t;

typedef struct lua_wiringx_t {
	PLUA_INTERFACE_FIELDS

	struct lua_wiringx_gpio_t **gpio;

	char *platform;
	int nrgpio;
} lua_wiringx_t;

static struct lua_wiringx_t **wiringx = NULL;
static int nrinits = 0;
static int gc_regged = 0;

static void plua_wiringx_object(lua_State *L, struct lua_wiringx_t *wiringx);

static struct lua_wiringx_gpio_t *plua_wiringx_get_gpio_struct(struct lua_wiringx_t *wiringx, int gpio) {
	int i = 0, match = 0;
	for(i=0;i<wiringx->nrgpio;i++) {
		if(wiringx->gpio[i]->gpio == gpio) {
			match = 1;
			break;
		}
	}

	if(match == 0) {
		if((wiringx->gpio = REALLOC(wiringx->gpio, sizeof(struct lua_wiringx_gpio_t *)*(wiringx->nrgpio+1))) == NULL) {
			OUT_OF_MEMORY
		}
		if((wiringx->gpio[wiringx->nrgpio] = MALLOC(sizeof(struct lua_wiringx_gpio_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(wiringx->gpio[wiringx->nrgpio], 0, sizeof(struct lua_wiringx_gpio_t));
		wiringx->gpio[wiringx->nrgpio]->gpio = -1;
		i = wiringx->nrgpio++;
	}
	return wiringx->gpio[i];
}

static void plua_wiringx_gc(void *ptr) {
	struct lua_wiringx_t **data = ptr;
	int i = 0, x = 0;
	for(x=0;x<nrinits;x++) {
		for(i=0;i<data[x]->nrgpio;i++) {
			if(data[x]->gpio[i]->callback != NULL) {
				FREE(data[x]->gpio[i]->callback);
			}
			FREE(data[x]->gpio[i]);
		}
		if(data[x]->gpio != NULL) {
			FREE(data[x]->gpio);
		}
		FREE(data[x]->platform);
		plua_metatable_free(data[x]->table);
		FREE(data[x]);
	}
	FREE(data);
	wiringx = NULL;
	nrinits = 0;
	gc_regged = 0;
}

static int plua_wiringx_set_userdata(lua_State *L) {
	struct lua_wiringx_t *wiringx = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "wiringx.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(wiringx == NULL) {
		luaL_error(L, "internal error: wiringx object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		if(wiringx->table != (void *)lua_topointer(L, -1)) {
			plua_metatable_free(wiringx->table);
		}

		wiringx->table = (void *)lua_topointer(L, -1);
		if(wiringx->table->ref != NULL) {
			uv_sem_post(wiringx->table->ref);
		}
		plua_ret_true(L);

		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, wiringx->table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static int plua_wiringx_get_userdata(lua_State *L) {
	struct lua_wiringx_t *wiringx = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "wiringx.getUserdata requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(wiringx == NULL) {
		luaL_error(L, "internal error: wiringx object not passed");
		return 0;
	}

	plua_metatable__push(L, (struct plua_interface_t *)wiringx);

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_wiringx_digital_write(struct lua_State *L) {
	if(lua_gettop(L) < 2 || lua_gettop(L) > 3) {
		luaL_error(L, "wiringX digitalWrite requires 2 or 3 arguments, %d given", lua_gettop(L));
	}

	int mode = -1;
	int gpio = -1;
	int is_table = 0;
	struct plua_metatable_t *table = NULL;

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, 1) == LUA_TNUMBER) {
			gpio = lua_tonumber(L, -1);
			lua_remove(L, 1);
		}
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, 1) == LUA_TNUMBER) {
			mode = lua_tonumber(L, 1);
			lua_remove(L, 1);
		}
	}

	if(lua_gettop(L) == 1) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "userdata expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TLIGHTUSERDATA || lua_type(L, 1) == LUA_TTABLE),
			1, buf);

		if(lua_type(L, 1) == LUA_TLIGHTUSERDATA) {
			table = (void *)lua_topointer(L, 1);
			lua_remove(L, 1);
		} else if(lua_type(L, 1) == LUA_TTABLE) {
			is_table = 1;

			plua_metatable_init(&table);

			lua_pushnil(L);
			while(lua_next(L, -2) != 0) {
				plua_metatable_parse_set(L, table);
				lua_pop(L, 1);
			}
		}
	}

	if(mode != 0 && mode != 1) {
		if(is_table == 1) {
			plua_metatable_free(table);
		}
		luaL_error(L, "wiringX digitalWrite mode should be HIGH or LOW", lua_gettop(L));
	}

	if(wiringXPlatform() == NULL) {
		lua_pushnumber(L, 0);
	} else if(wiringXValidGPIO(gpio) == -1) {
		lua_pushnumber(L, 0);
	} else {
		if(table != NULL) {
			int i = 0, error = 0;
			for(i=0;i<table->nrvar;i++) {
				if(table->table[i].key.type_ != LUA_TNUMBER || table->table[i].val.type_ != LUA_TNUMBER) {
					luaL_error(L, "wiringX digitalWrite pulses should be a numeric array with numbers", lua_gettop(L));
				}
			}
			for(i=0;i<table->nrvar;i++) {
				if(digitalWrite(gpio, mode) == -1) {
					error = 1;
					break;
				}
				if(i < table->nrvar) {
					usleep(table->table[i].val.number_);
				}
				mode ^= 1;
			}
			if(error == 1) {
				lua_pushnumber(L, 0);	
			} else {
				lua_pushnumber(L, table->nrvar);
			}
		} else {
			if(digitalWrite(gpio, mode) == -1) {
				lua_pushnumber(L, 0);
			} else {
				lua_pushnumber(L, 1);
			}
		}
	}

	if(is_table == 1) {
		plua_metatable_free(table);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_wiringx_has_gpio(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "wiringX hasGPIO requires 1 argument, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TNUMBER),
		1, buf);

	int gpio = 0;
	if(lua_type(L, -1) == LUA_TNUMBER) {
		gpio = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	if(wiringXPlatform() == NULL) {
		lua_pushboolean(L, 0);
	} else if(wiringXValidGPIO(gpio) == 0) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_wiringx_pin_mode(struct lua_State *L) {
	struct lua_wiringx_t *wiringx = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 2) {
		luaL_error(L, "wiringX pinMode requires 2 arguments, %d given", lua_gettop(L));
	}
	int gpio = 0, mode = 0;

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, 1) == LUA_TNUMBER) {
			gpio = lua_tonumber(L, 1);
			lua_remove(L, 1);
		}
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, 1) == LUA_TNUMBER) {
			mode = lua_tonumber(L, 1);
			lua_remove(L, 1);
		}
	}

	struct lua_wiringx_gpio_t *tmp = plua_wiringx_get_gpio_struct(wiringx, gpio);

	if(tmp->timer_req != NULL) {
		uv_timer_stop(tmp->timer_req);
	}
	if(tmp->poll_req != NULL) {
		uv_poll_stop(tmp->poll_req);
	}
	if(tmp->callback != NULL) {
		FREE(tmp->callback);
	}

	tmp->gpio = gpio;
	tmp->mode = mode;
	tmp->parent = wiringx;

	if(wiringXPlatform() == NULL) {
		lua_pushboolean(L, 0);
	} else if(wiringXValidGPIO(gpio) == -1) {
		lua_pushboolean(L, 0);
	} else if(pinMode(gpio, mode) == -1) {
		lua_pushboolean(L, 0);
	} else {
		lua_pushboolean(L, 1);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_wiringx_poll_timer(uv_timer_t *req) {
	struct lua_wiringx_gpio_t *data = req->data;
	int nr = data->data.rptr, idx = data->data.idx;

	if(nr == 1) {
		return;
	}

	data->data.idx ^= 1;
	data->data.rptr = 1;

	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the wiringx callback is called
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = data->parent->module;

	logprintf(LOG_DEBUG, "lua wiringx on state #%d", state->idx);

	plua_namespace(data->parent->module, p);

	lua_getglobal(state->L, name);

	if(lua_type(state->L, -1) == LUA_TNIL) {
		/*
		 * FIXME shouldn't state be freed?
		 */
		luaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, data->callback);

	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		/*
		 * FIXME shouldn't state be freed?
		 */
		luaL_error(state->L, "%s: wiringx callback %s does not exist", state->module->file, data->callback);
	}

	plua_wiringx_object(state->L, data->parent);

	lua_pushnumber(state->L, nr);
	lua_createtable(state->L, nr, 0);

	int i = 0;
	for(i=0;i<nr;i++) {
		lua_pushnumber(state->L, i);
		lua_pushnumber(state->L, data->data.rbuffer[idx][i]);
		lua_settable(state->L, -3);
	}

	if(lua_pcall(state->L, 3, 0, 0) == LUA_ERRRUN) {
		if(lua_type(state->L, -1) == LUA_TNIL) {
			logprintf(LOG_ERR, "%s: syntax error", state->module->file);
			plua_clear_state(state);
			return;
		}
		if(lua_type(state->L, -1) == LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(state->L,  -1));
			lua_pop(state->L, -1);
			plua_clear_state(state);
			return;
		}
	}

	lua_remove(state->L, -1);
	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return;
}

static void plua_wiringx_poll_cb(uv_poll_t *req, int status, int events) {
	struct lua_wiringx_gpio_t *data = req->data;

	int fd = req->io_watcher.fd;

#ifdef PILIGHT_UNITTEST
	if(events & UV_READABLE) {
#else
	if(events & UV_PRIORITIZED) {
#endif
		uint8_t c = 0;

		(void)read(fd, &c, 1);
		lseek(fd, 0, SEEK_SET);

		struct timeval tv;
		gettimeofday(&tv, NULL);

		data->timestamp.first = data->timestamp.second;
		data->timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

		int duration = (int)((int)data->timestamp.second-(int)data->timestamp.first);
		data->data.rbuffer[data->data.idx][data->data.rptr++] = duration;

		if(data->data.rptr > 1023) {
			logprintf(LOG_NOTICE, "433gpio pulse buffer full");
			data->data.rptr = 0;
		}
	}

	return;
}

static int plua_wiringx_isr(struct lua_State *L) {
	/* Make sure the pilight sender gets
	   the highest priority available */
#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = 80;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched);
#endif

	struct lua_wiringx_t *wiringx = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *func = NULL, name[255] = { '\0' };
	int gpio = -1, mode = -1, interval = 250;

	if(lua_gettop(L) != 3 && lua_gettop(L) != 4) {
		luaL_error(L, "wiringx.ISR requires 3 or 4 arguments, %d given", lua_gettop(L));
	}

	if(wiringx == NULL) {
		luaL_error(L, "internal error: wiringx object not passed");
	}

	if(wiringx->module == NULL) {
		luaL_error(L, "internal error: lua state not properly initialized");
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, 1) == LUA_TNUMBER) {
			gpio = lua_tonumber(L, 1);
			lua_remove(L, 1);
		}
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, 1) == LUA_TNUMBER) {
			mode = lua_tonumber(L, 1);
			lua_remove(L, 1);
		}
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TSTRING),
			1, buf);

		if(lua_type(L, 1) == LUA_TSTRING) {
			func = (void *)lua_tostring(L, 1);
			lua_remove(L, 1);

			p = name;
			plua_namespace(wiringx->module, p);

			lua_getglobal(L, name);
			if(lua_type(L, -1) == LUA_TNIL) {
				luaL_error(L, "cannot find %s lua module", wiringx->module->name);
			}

			lua_getfield(L, -1, func);
			if(lua_type(L, -1) != LUA_TFUNCTION) {
				luaL_error(L, "%s: wiringx callback \"%s\" does not exist", wiringx->module->file, func);
			}
			lua_remove(L, -1);
			lua_remove(L, -1);
		}

		if(lua_gettop(L) == 1) 	{
			char buf[128] = { '\0' }, *p = buf;
			char *error = "number expected, got %s";
			sprintf(p, error, lua_typename(L, lua_type(L, 1)));

			luaL_argcheck(L,
				(lua_type(L, 1) == LUA_TNUMBER),
				1, buf);

			if(lua_type(L, 1) == LUA_TNUMBER) {
				interval = lua_tonumber(L, 1);
				lua_remove(L, 1);
			}
		}

		if(gpio >= 0) {
			struct lua_wiringx_gpio_t *tmp = plua_wiringx_get_gpio_struct(wiringx, gpio);

			if(wiringXISR(gpio, mode) < 0) {
				/*
				 * FIXME
				 */
				// log
			}

			int fd = wiringXSelectableFd(gpio);

			if(tmp->timer_req != NULL) {
				uv_timer_stop(tmp->timer_req);
			} else {
				if((tmp->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				uv_timer_init(uv_default_loop(), tmp->timer_req);
			}

			if(tmp->poll_req != NULL) {
				uv_poll_stop(tmp->poll_req);
			} else {
				if((tmp->poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				uv_poll_init(uv_default_loop(), tmp->poll_req, fd);
			}

			tmp->gpio = gpio;
			tmp->mode = mode;
			tmp->parent = wiringx;

			tmp->poll_req->data = tmp;

#ifdef PILIGHT_UNITTEST
			uv_poll_start(tmp->poll_req, UV_READABLE, plua_wiringx_poll_cb);
#else
			uv_poll_start(tmp->poll_req, UV_PRIORITIZED, plua_wiringx_poll_cb);
#endif

			if(tmp->callback == NULL || (tmp->callback != NULL && strcmp(tmp->callback, func) != 0)) {
				if(tmp->callback != NULL) {
					FREE(tmp->callback);
				}
				if((tmp->callback = STRDUP(func)) == NULL) {
					OUT_OF_MEMORY
				}
			}
			tmp->timer_req->data = tmp;
			uv_timer_start(tmp->timer_req, (void (*)(uv_timer_t *))plua_wiringx_poll_timer, interval, interval);
		}
	}

	return 0;
}

static void plua_wiringx_object(lua_State *L, struct lua_wiringx_t *wiringx) {
	lua_newtable(L);

	lua_pushstring(L, "ISR");
	lua_pushlightuserdata(L, wiringx);
	lua_pushcclosure(L, plua_wiringx_isr, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "hasGPIO");
	lua_pushlightuserdata(L, wiringx);
	lua_pushcclosure(L, plua_wiringx_has_gpio, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "pinMode");
	lua_pushlightuserdata(L, wiringx);
	lua_pushcclosure(L, plua_wiringx_pin_mode, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "digitalWrite");
	lua_pushlightuserdata(L, wiringx);
	lua_pushcclosure(L, plua_wiringx_digital_write, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, wiringx);
	lua_pushcclosure(L, plua_wiringx_get_userdata, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, wiringx);
	lua_pushcclosure(L, plua_wiringx_set_userdata, 1);
	lua_settable(L, -3);
}

int plua_wiringx_setup(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "wiringX setup requires 1 argument, %d given", lua_gettop(L));
	}

#if !defined(__arm__) && !defined(__mips__) && !defined(PILIGHT_UNITTEST)
	lua_remove(L, -1);
	lua_pushboolean(L, 0);
#else

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TSTRING),
		1, buf);

	const char *platform = NULL;
	if(lua_type(L, -1) == LUA_TSTRING) {
		platform = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	int i = 0, match = 0;
	for(i=0;i<nrinits;i++) {
		if(strcmp(wiringx[i]->platform, platform) == 0) {
			match = 1;
			break;
		}
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	if(match == 0) {
		if(wiringXSetup((char *)platform, _logprintf) == 0) {
			if((wiringx = REALLOC(wiringx, sizeof(struct lua_wiringx_t *)*(nrinits+1))) == NULL) {
				OUT_OF_MEMORY
			}
			if((wiringx[nrinits] = MALLOC(sizeof(struct lua_wiringx_t))) == NULL) {
				OUT_OF_MEMORY
			}
			i = nrinits++;

			memset(wiringx[i], '\0', sizeof(struct lua_wiringx_t));

			if((wiringx[i]->platform = STRDUP((char *)platform)) == NULL) {
				OUT_OF_MEMORY
			}

			plua_metatable_init(&wiringx[i]->table);

			wiringx[i]->nrgpio = 0;

			wiringx[i]->module = state->module;
			wiringx[i]->L = L;

			if(gc_regged == 0) {
				gc_regged = 1;
				plua_gc_reg(NULL, wiringx, plua_wiringx_gc);
			}

			plua_wiringx_object(L, wiringx[i]);
		} else {
			lua_pushnil(L);
		}
	} else {
		wiringx[i]->module = state->module;
		plua_wiringx_object(L, wiringx[i]);
	}

	lua_assert(lua_gettop(L) == 1);
#endif

	return 1;
}