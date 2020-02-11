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

#include "../../core/log.h"
#include "../async.h"
#include "../table.h"


static void timer_callback(uv_timer_t *req);
static int plua_async_timer_start(lua_State *L);

#ifdef PILIGHT_UNITTEST
extern void plua_async_timer_gc(void *ptr);
#else
static void plua_async_timer_gc(void *ptr) {
	struct lua_timer_t *lua_timer = ptr;

	if(lua_timer != NULL) {
		int x = 0;
		if((x = atomic_dec(lua_timer->ref)) == 0) {
			if(lua_timer->callback != NULL) {
				FREE(lua_timer->callback);
			}
			if(lua_timer->table != NULL) {
				plua_metatable_free(lua_timer->table);
			}
			plua_gc_unreg(NULL, lua_timer);
			if(lua_timer->sigterm == 1) {
				lua_timer->gc = NULL;
			}
			FREE(lua_timer);
			lua_timer = NULL;
		} else {
			lua_timer->running = 0;
		}
		assert(x >= 0);
	}
}
#endif

#ifdef PILIGHT_UNITTEST
extern void plua_async_timer_global_gc(void *ptr);
#else
static void plua_async_timer_global_gc(void *ptr) {
	struct lua_timer_t *lua_timer = ptr;
	lua_timer->sigterm = 1;
	plua_async_timer_gc(ptr);
}
#endif

static void timer_stop(lua_State *L, struct lua_timer_t *timer) {
	if(timer != NULL) {
		timer->running = 0;
		plua_gc_unreg(NULL, timer);
		plua_gc_unreg(L, timer);
		if(timer->timer_req != NULL) {
			uv_timer_stop(timer->timer_req);
		}
		plua_async_timer_gc(timer);
	}
}

static int plua_async_timer_stop(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "timer:stop requires 0 argument, %d given", lua_gettop(L));
	}

	if(timer == NULL) {
		pluaL_error(L, "internal error: timer object not passed");
	}

	timer_stop(L, timer);

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_timer_set_timeout(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));
	uv_timer_t *timer_req = NULL;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "timer.setTimeout requires 1 argument, %d given", lua_gettop(L));
	}

	if(timer == NULL) {
		pluaL_error(L, "internal error: timer object not passed");
	}

	timer_req = timer->timer_req;

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";
	int timeout = 0;

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TNUMBER),
		1, buf);

	if(lua_type(L, -1) == LUA_TNUMBER) {
		timeout = lua_tonumber(L, -1);
		lua_remove(L, -1);
	}

	if(timeout < 0) {
		pluaL_error(L, "timer timeout should be larger than 0, %d given", timeout);
	}

	timer->timeout = timeout;

	if(timer->running == 1) {
		uv_timer_start(timer_req, timer_callback, timer->timeout, timer->timeout);
	}

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_timer_set_repeat(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));
	uv_timer_t *timer_req = NULL;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "timer.setRepeat requires 1 argument, %d given", lua_gettop(L));
	}

	if(timer == NULL) {
		pluaL_error(L, "internal error: timer object not passed");
	}

	timer_req = timer->timer_req;

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";
	int repeat = 0;

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TNUMBER),
		1, buf);

	if(lua_type(L, -1) == LUA_TNUMBER) {
		repeat = lua_tonumber(L, -1);
		lua_remove(L, -1);
	}

	if(repeat < 0) {
		pluaL_error(L, "timer repeat should be larger than 0, %d given", repeat);
	}

	timer->repeat = repeat;

	if(timer->running == 1) {
		uv_timer_start(timer_req, timer_callback, timer->timeout, timer->repeat);
	}

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_timer_set_callback(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "timer.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(timer == NULL) {
		pluaL_error(L, "internal error: timer object not passed");
	}

	if(timer->module == NULL) {
		pluaL_error(L, "internal error: lua state not properly initialized");
	}

	const char *func = NULL;
	char buf[128] = { '\0' }, *p = buf, name[255] = { '\0' };
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	if(lua_type(L, -1) == LUA_TSTRING) {
		func = lua_tostring(L, -1);
		lua_remove(L, -1);
	}

	p = name;
	plua_namespace(timer->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		pluaL_error(L, "cannot find %s lua module", timer->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		pluaL_error(L, "%s: timer callback %s does not exist", timer->module->file, func);
	}

	if(timer->callback != NULL) {
		FREE(timer->callback);
	}

	if((timer->callback = STRDUP((char *)func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_remove(L, -1);
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_timer_start(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));
	uv_timer_t *timer_req = NULL;

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "timer.start requires 0 arguments, %d given", lua_gettop(L));
	}

	if(timer == NULL) {
		pluaL_error(L, "internal error: timer object not passed");
	}

	timer_req = timer->timer_req;

	if(timer->callback == NULL) {
		if(timer->module != NULL) {
			pluaL_error(L, "%s: timer callback has not been set", timer->module->file);
		} else {
			pluaL_error(L, "timer callback has not been set");
		}
	}
	if(timer->timeout == -1) {
		pluaL_error(L, "%s: timer timeout has not been set", timer->module->file);
	}

	if(timer->running == 0) {
		atomic_inc(timer->ref);
	}
	timer->running = 1;

	uv_timer_start(timer_req, timer_callback, timer->timeout, timer->repeat);

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_timer_set_data(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_set_userdata(L, (struct plua_interface_t *)timer, "timer");
}

static int plua_async_timer_get_data(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_get_userdata(L, (struct plua_interface_t *)timer, "timer");
}

static int plua_async_timer_call(lua_State *L) {
	struct lua_mqtt_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(timer == NULL) {
		logprintf(LOG_ERR, "internal error: timer object not passed");
		return 0;
	}

	lua_pushlightuserdata(L, timer);

	return 1;
}

static void plua_async_timer_object(lua_State *L, struct lua_timer_t *timer) {
	lua_newtable(L);

	lua_pushstring(L, "setTimeout");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_set_timeout, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setRepeat");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_set_repeat, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "start");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_start, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "stop");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_stop, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_get_data, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_set_data, 1);
	lua_settable(L, -3);

	lua_newtable(L);

	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, timer);
	lua_pushcclosure(L, plua_async_timer_call, 1);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);
}

static void timer_callback(uv_timer_t *req) {
	struct lua_timer_t *timer = req->data;
	char name[512], *p = name;
	memset(name, '\0', 512);

	/*
	 * Only create a new state once the timer is triggered
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = timer->module;

	logprintf(LOG_DEBUG, "lua timer on state #%d", state->idx);

	p = name;
	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		pluaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, timer->callback);
	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		pluaL_error(state->L, "%s: timer callback %s does not exist", state->module->file, timer->callback);
	}

	plua_async_timer_object(state->L, timer);

	if(timer->repeat == 0) {
		plua_gc_reg(state->L, timer, plua_async_timer_gc);
	}

	assert(plua_check_stack(state->L, 3, PLUA_TTABLE, PLUA_TFUNCTION, PLUA_TTABLE) == 0);
	if(plua_pcall(state->L, state->module->file, 1, 0) == -1) {
		assert(plua_check_stack(state->L, 0) == 0);
		plua_clear_state(state);
		return;
	}

	lua_remove(state->L, 1);
	assert(plua_check_stack(state->L, 0) == 0);
	plua_clear_state(state);
}

int plua_async_timer(struct lua_State *L) {
	struct lua_timer_t *lua_timer = NULL;

	if(lua_gettop(L) < 0 || lua_gettop(L) > 1) {
		pluaL_error(L, "timer requires 0 or 1 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		assert(plua_check_stack(L, 0) == 0);
		return 0;
	}

	if(lua_gettop(L) == 1) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "lightuserdata expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TLIGHTUSERDATA),
			1, buf);

		lua_timer = lua_touserdata(L, -1);
		lua_remove(L, -1);

		if(lua_timer->type != PLUA_TIMER) {
			luaL_error(L, "timer object required but %s object passed", plua_interface_to_string(lua_timer->type));
		}
		atomic_inc(lua_timer->ref);
	} else {
		uv_timer_t *timer_req = MALLOC(sizeof(uv_timer_t));
		if(timer_req == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		lua_timer = MALLOC(sizeof(struct lua_timer_t));
		if(lua_timer == NULL) {
			OUT_OF_MEMORY
		}
		memset(lua_timer, '\0', sizeof(struct lua_timer_t));

		lua_timer->type = PLUA_TIMER;
		lua_timer->ref = 1;

		plua_metatable_init(&lua_timer->table);

		lua_timer->module = state->module;
		lua_timer->timer_req = timer_req;
		lua_timer->timeout = -1;
		lua_timer->gc = plua_async_timer_gc;
		lua_timer->L = L;
		timer_req->data = lua_timer;

		uv_timer_init(uv_default_loop(), timer_req);
		plua_gc_reg(NULL, lua_timer, plua_async_timer_global_gc);
	}
	plua_gc_reg(L, lua_timer, plua_async_timer_gc);

	plua_async_timer_object(L, lua_timer);

	assert(plua_check_stack(state->L, 1, PLUA_TTABLE) == 0);

	return 1;
}