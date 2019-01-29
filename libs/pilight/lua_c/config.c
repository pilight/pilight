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

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "../core/log.h"
#include "../config/config.h"
#include "config/setting.h"
#include "config/hardware.h"
#include "config/device.h"
#include "async.h"

static int plua_config_set_data(lua_State *L) {
	// struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));
	// struct plua_metatable_t *cpy = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "config.setData requires 1 argument, %d given", lua_gettop(L));
	}

	// if(thread == NULL) {
		// luaL_error(L, "internal error: thread object not passed");
	// }

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		// cpy = (void *)lua_topointer(L, -1);
		// lua_remove(L, -1);
		// plua_metatable_clone(&cpy, &thread->table);

		// plua_ret_true(L);
		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		struct plua_metatable_t *table = config_get_metatable();

		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static int plua_config_get_data(lua_State *L) {
	// struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config.getData requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	// if(timer == NULL) {
		// luaL_error(L, "internal error: config object not passed");
		// return 0;
	// }

	struct plua_metatable_t *table = config_get_metatable();

	if(table == NULL) {
		lua_pushnil(L);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_metatable_push(L, table);

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_config_object(lua_State *L, void *foo) {
	lua_newtable(L);

	lua_pushstring(L, "getDevice");
	// lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_config_device, 0);
	lua_settable(L, -3);

	lua_pushstring(L, "getSetting");
	// lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_config_setting, 0);
	lua_settable(L, -3);

	lua_pushstring(L, "getHardware");
	// lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_config_hardware, 0);
	lua_settable(L, -3);

	// lua_pushstring(L, "setRepeat");
	// lua_pushlightuserdata(L, timer);
	// lua_pushcclosure(L, plua_async_timer_set_repeat, 1);
	// lua_settable(L, -3);

	// lua_pushstring(L, "setCallback");
	// lua_pushlightuserdata(L, timer);
	// lua_pushcclosure(L, plua_async_timer_set_callback, 1);
	// lua_settable(L, -3);

	// lua_pushstring(L, "start");
	// lua_pushlightuserdata(L, timer);
	// lua_pushcclosure(L, plua_async_timer_start, 1);
	// lua_settable(L, -3);

	// lua_pushstring(L, "stop");
	// lua_pushlightuserdata(L, timer);
	// lua_pushcclosure(L, plua_async_timer_stop, 1);
	// lua_settable(L, -3);

	lua_pushstring(L, "setData");
	// lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_config_set_data, 0);
	lua_settable(L, -3);

	lua_pushstring(L, "getData");
	// lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_config_get_data, 0);
	lua_settable(L, -3);
}

int plua_config(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		luaL_error(L, "config requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	plua_config_object(L, NULL);

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
