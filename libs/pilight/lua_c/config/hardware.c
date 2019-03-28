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
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "../../core/log.h"
#include "../config.h"
#include "../table.h"
#include "hardware.h"

static void plua_config_hardware_gc(void *ptr) {
	struct plua_hardware_t *hw = ptr;
	if(hw->table != NULL) {
		plua_metatable_free(hw->table);
	}
	FREE(hw->name);
	FREE(hw);
}

static int plua_config_hardware_get_data(lua_State *L) {
	struct plua_hardware_t *hw = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "hardware.getUserdata requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(hw == NULL) {
		luaL_error(L, "internal error: hardware object not passed");
		return 0;
	}

	plua_metatable__push(L, (struct plua_interface_t *)hw);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_config_hardware_set_data(lua_State *L) {
	struct plua_hardware_t *hw = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "hardware.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(hw == NULL) {
		luaL_error(L, "internal error: hardware object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		if(hw->table != (void *)lua_topointer(L, -1)) {
			plua_metatable_free(hw->table);
		}
		hw->table = (void *)lua_topointer(L, -1);

		if(hw->table->ref != NULL) {
			uv_sem_post(hw->table->ref);
		}

		plua_ret_true(L);
		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, hw->table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static int plua_config_hardware_validate(lua_State *L) {
	struct plua_hardware_t *hw = (void *)lua_topointer(L, lua_upvalueindex(1));
	const char *setting = NULL;

	if(hw == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config hardware validate requires 0 arguments, %d given", lua_gettop(L));
	}

	char name[255], *p = name;
	memset(name, '\0', 255);

	sprintf(p, "hardware.%s", hw->name);
	lua_getglobal(L, name);

	if(lua_isnil(L, -1) != 0) {
		lua_remove(L, -1);
		assert(lua_gettop(L) == 0);
		return -1;
	}

	if(lua_istable(L, -1) != 0) {
		char *file = NULL;
		struct plua_module_t *tmp = plua_get_modules(), *oldmod = NULL;
		while(tmp) {
			if(strcmp(hw->name, tmp->name) == 0) {
				file = tmp->file;
				break;
			}
			tmp = tmp->next;
		}

		struct lua_state_t *state = plua_get_current_state(L);
		oldmod = state->module;
		state->module = tmp;

#if LUA_VERSION_NUM <= 502
		lua_getfield(L, -1, "validate");
		if(lua_type(L, -1) != LUA_TFUNCTION) {
#else
		if(lua_getfield(L, -1, "validate") == 0) {
#endif
			logprintf(LOG_ERR, "%s: validate function missing", file);
			state->module = oldmod;
			return 0;
		}
		lua_pushstring(L, setting);
		if(lua_pcall(L, 1, 1, 0) == LUA_ERRRUN) {
			if(lua_type(L, -1) == LUA_TNIL) {
				logprintf(LOG_ERR, "%s: syntax error", file);
				state->module = oldmod;
				lua_remove(L, 1);
				lua_pushboolean(L, 0);
				assert(lua_gettop(L) == 1);
				return 1;
			}
			if(lua_type(L, -1) == LUA_TSTRING) {
				logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
				lua_pop(L, 1);
				state->module = oldmod;
				lua_remove(L, 1);
				lua_pushboolean(L, 0);
				assert(lua_gettop(L) == 1);
				return 1;
			}
		}
		state->module = oldmod;

		lua_remove(L, 1);

		assert(lua_gettop(L) == 1);

		return 1;
	}
	lua_pushboolean(L, 0);
	assert(lua_gettop(L) == 1);
	return 1;
}

int plua_config_hardware(lua_State *L) {
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	const char *module = NULL;

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	if(lua_type(L, -1) == LUA_TSTRING) {
		module = lua_tostring(L, -1);
		lua_remove(L, -1);
	}

	char name[255];
	memset(name, '\0', 255);

	p = name;

	sprintf(p, "hardware.%s", module);
	lua_getglobal(L, name);

	if(lua_isnil(L, -1) != 0) {
		lua_remove(L, -1);
		assert(lua_gettop(L) == 0);
		return -1;
	}

	if(lua_istable(L, -1) != 0) {
		struct plua_hardware_t *hw = MALLOC(sizeof(struct plua_hardware_t));
		if(hw == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(hw, 0, sizeof(struct plua_hardware_t));

		lua_remove(L, -1);

		if((hw->name = STRDUP((char *)module)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		plua_metatable_init(&hw->table);

		lua_newtable(L);

		lua_pushstring(L, "validate");
		lua_pushlightuserdata(L, hw);
		lua_pushcclosure(L, plua_config_hardware_validate, 1);
		lua_settable(L, -3);

		lua_pushstring(L, "getUserdata");
		lua_pushlightuserdata(L, hw);
		lua_pushcclosure(L, plua_config_hardware_get_data, 1);
		lua_settable(L, -3);

		lua_pushstring(L, "setUserdata");
		lua_pushlightuserdata(L, hw);
		lua_pushcclosure(L, plua_config_hardware_set_data, 1);
		lua_settable(L, -3);

		plua_gc_reg(L, (void *)hw, plua_config_hardware_gc);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}