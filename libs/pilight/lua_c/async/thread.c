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

static void thread_callback(uv_work_t *req);

#ifdef PILIGHT_UNITTEST
extern void thread_free(uv_work_t *req, int status);
#else
static void thread_free(uv_work_t *req, int status) {
	struct lua_thread_t *lua_thread = req->data;

	if(lua_thread != NULL) {
		int x = 0;
		if((x = atomic_dec(lua_thread->ref)) == 0) {
			if(lua_thread->table != NULL) {
				plua_metatable_free(lua_thread->table);
			}
			if(lua_thread->work_req != NULL) {
				FREE(lua_thread->work_req);
			}
			if(lua_thread->callback != NULL) {
				FREE(lua_thread->callback);
			}
			lua_thread->running = 0;
			if(lua_thread->sigterm == 1) {
				lua_thread->gc = NULL;
			}
			plua_gc_unreg(NULL, lua_thread);
			FREE(lua_thread);
			lua_thread = NULL;
		}
		assert(x >= 0);
	}
}
#endif

static void plua_async_thread_gc(void *ptr) {
	struct lua_thread_t *lua_thread = ptr;

	thread_free(lua_thread->work_req, -99);
}

#ifdef PILIGHT_UNITTEST
extern void plua_async_thread_global_gc(void *ptr);
#else
static void plua_async_thread_global_gc(void *ptr) {
	struct lua_thread_t *lua_thread = ptr;
	atomic_inc(lua_thread->ref);
	lua_thread->sigterm = 1;
	plua_async_thread_gc(ptr);
}
#endif

static int plua_async_thread_trigger(lua_State *L) {
	/*
	 * Make sure we execute in the main thread
	 */
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "thread.trigger requires 0 arguments, %d given", lua_gettop(L));
	}

	if(thread == NULL) {
		pluaL_error(L, "internal error: thread object not passed");
	}

	thread->L = L;

	if(thread->callback == NULL) {
		if(thread->module != NULL) {
			pluaL_error(L, "%s: thread callback has not been set", thread->module->file);
		} else {
			pluaL_error(L, "thread callback has not been set");
		}
	}

	if(thread->running == 0) {
		atomic_inc(thread->ref);
	}

	thread->running = 1;

	uv_queue_work_s(thread->work_req, "lua thread", 0, thread_callback, thread_free);

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_thread_set_data(lua_State *L) {
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_set_userdata(L, (struct plua_interface_t *)thread, "thread");
}

static int plua_async_thread_get_data(lua_State *L) {
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_get_userdata(L, (struct plua_interface_t *)thread, "thread");
}

static int plua_async_thread_set_callback(lua_State *L) {
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "thread.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(thread == NULL) {
		pluaL_error(L, "internal error: thread object not passed");
	}

	if(thread->module == NULL) {
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
	plua_namespace(thread->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		pluaL_error(L, "cannot find %s lua module", thread->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		pluaL_error(L, "%s: thead callback %s does not exist", thread->module->file, func);
	}

	if(thread->callback != NULL) {
		FREE(thread->callback);
	}

	if((thread->callback = STRDUP((char *)func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_remove(L, -1);
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_thread_call(lua_State *L) {
	struct lua_mqtt_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(thread == NULL) {
		logprintf(LOG_ERR, "internal error: thread object not passed");
		return 0;
	}

	lua_pushlightuserdata(L, thread);

	return 1;
}

static void plua_async_thread_object(lua_State *L, struct lua_thread_t *thread) {
	lua_newtable(L);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, thread);
	lua_pushcclosure(L, plua_async_thread_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "trigger");
	lua_pushlightuserdata(L, thread);
	lua_pushcclosure(L, plua_async_thread_trigger, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, thread);
	lua_pushcclosure(L, plua_async_thread_get_data, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, thread);
	lua_pushcclosure(L, plua_async_thread_set_data, 1);
	lua_settable(L, -3);

	lua_newtable(L);

	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, thread);
	lua_pushcclosure(L, plua_async_thread_call, 1);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);
}

static void thread_callback(uv_work_t *req) {
	struct lua_thread_t *thread = req->data;
	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the thread is triggered
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = thread->module;

	logprintf(LOG_DEBUG, "lua thread on state #%d", state->idx);

	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		pluaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, thread->callback);
	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		pluaL_error(state->L, "%s: thread callback %s does not exist", state->module->file, thread->callback);
	}

	plua_async_thread_object(state->L, thread);

	assert(plua_check_stack(state->L, 3, PLUA_TTABLE, PLUA_TFUNCTION, PLUA_TTABLE) == 0);
	if(plua_pcall(state->L, state->module->file, 1, 0) == -1) {
		assert(plua_check_stack(state->L, 0) == 0);
		plua_clear_state(state);
		return;
	}

	lua_remove(state->L, -1);

	assert(plua_check_stack(state->L, 0) == 0);

	plua_clear_state(state);
}

int plua_async_thread(struct lua_State *L) {
	struct lua_thread_t *lua_thread = NULL;

	if(lua_gettop(L) < 0 || lua_gettop(L) > 1) {
		pluaL_error(L, "thread requires 0 or 1 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(lua_gettop(L) == 1) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "lightuserdata expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TLIGHTUSERDATA),
			1, buf);

		lua_thread = lua_touserdata(L, -1);
		lua_remove(L, -1);

		if(lua_thread->type != PLUA_THREAD) {
			luaL_error(L, "thread object required but %s object passed", plua_interface_to_string(lua_thread->type));
		}
		atomic_inc(lua_thread->ref);
	} else {
		struct lua_state_t *state = plua_get_current_state(L);
		if(state == NULL) {
			assert(plua_check_stack(L, 0) == 0);
			return 0;
		}

		uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
		if(tp_work_req == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		lua_thread = MALLOC(sizeof(struct lua_thread_t));
		if(lua_thread == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(lua_thread, 0, sizeof(struct lua_thread_t));

		lua_thread->type = PLUA_THREAD;
		lua_thread->ref = 1;

		plua_metatable_init(&lua_thread->table);

		lua_thread->module = state->module;
		lua_thread->work_req = tp_work_req;
		lua_thread->gc = plua_async_thread_gc;
		lua_thread->running = 0;
		tp_work_req->data = lua_thread;
		lua_thread->L = L;

		plua_gc_reg(NULL, lua_thread, plua_async_thread_global_gc);
	}
	plua_gc_reg(L, lua_thread, plua_async_thread_gc);

	plua_async_thread_object(L, lua_thread);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}