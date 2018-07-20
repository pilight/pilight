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
#include "async.h"

typedef struct lua_thread_t {
	struct plua_metatable_t *table;
	struct plua_module_t *module;
	char *callback;
	lua_State *L;
	uv_work_t *work_req;
} lua_thread_t;

typedef struct lua_timer_t {
	struct plua_metatable_t *table;
	struct plua_module_t *module;
	uv_timer_t *timer_req;
	lua_State *L;
	int running;
	int timeout;
	int repeat;
	char *callback;
} lua_timer_t;

static void timer_callback(uv_timer_t *req);
static void thread_callback(uv_work_t *req);
static int plua_async_timer_start(lua_State *L);

static void thread_free(uv_work_t *req, int status) {
	struct lua_thread_t *lua_thread = req->data;

	if(lua_thread != NULL) {
		plua_gc_unreg(lua_thread->L, lua_thread);
		if(lua_thread->table != NULL) {
			plua_metatable_free(lua_thread->table);
		}
		if(lua_thread->work_req != NULL) {
			FREE(lua_thread->work_req);
		}
		if(lua_thread->callback != NULL) {
			FREE(lua_thread->callback);
		}
		FREE(lua_thread);
	}
}

static int plua_async_thread_trigger(lua_State *L) {
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "thread.trigger requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(thread == NULL) {
		luaL_error(L, "internal error: thread object not passed");
		return 0;
	}

	thread->L = L;

	if(thread->callback == NULL) {
		if(thread->module != NULL) {
			luaL_error(L, "%s: thread callback has not been set", thread->module->file);
		} else {
			luaL_error(L, "thread callback has not been set");
		}
		return 0;
	}

	if(uv_queue_work(uv_default_loop(), thread->work_req, "lua thread", thread_callback, thread_free) < 0) {
		plua_ret_false(L);
		return 0;
	}
	plua_gc_unreg(L, thread);

	plua_ret_true(L);
	return 1;
}

static int plua_async_thread_set_data(lua_State *L) {
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct plua_metatable_t *cpy = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "thread.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(thread == NULL) {
		luaL_error(L, "internal error: thread object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		cpy = (void *)lua_topointer(L, -1);
		lua_remove(L, -1);
		plua_metatable_clone(&cpy, &thread->table);

		plua_ret_true(L);
		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, thread->table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static int plua_async_thread_get_data(lua_State *L) {
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "thread.getUserdata requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(thread == NULL) {
		luaL_error(L, "internal error: thread object not passed");
		return 0;
	}

	lua_newtable(L);
	lua_newtable(L);

	lua_pushstring(L, "__index");
	lua_pushlightuserdata(L, thread->table);
	lua_pushcclosure(L, plua_metatable_get, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__newindex");
	lua_pushlightuserdata(L, thread->table);
	lua_pushcclosure(L, plua_metatable_set, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__gc");
	lua_pushlightuserdata(L, thread->table);
	lua_pushcclosure(L, plua_metatable_gc, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__pairs");
	lua_pushlightuserdata(L, thread->table);
	lua_pushcclosure(L, plua_metatable_pairs, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__next");
	lua_pushlightuserdata(L, thread->table);
	lua_pushcclosure(L, plua_metatable_next, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, thread->table);
	lua_pushcclosure(L, plua_metatable_call, 1);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_async_thread_set_callback(lua_State *L) {
	struct lua_thread_t *thread = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "thread.setCallback requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(thread == NULL) {
		luaL_error(L, "internal error: timer object not passed");
		return 0;
	}

	if(thread->module == NULL) {
		luaL_error(L, "internal error: lua state not properly initialized");
		return 0;
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
	switch(thread->module->type) {
		case UNITTEST: {
			sprintf(p, "unittest.%s", thread->module->name);
		} break;
		case FUNCTION: {
			sprintf(p, "function.%s", thread->module->name);
		} break;
		case OPERATOR: {
			sprintf(p, "operator.%s", thread->module->name);
		} break;
		case ACTION: {
			sprintf(p, "action.%s", thread->module->name);
		} break;
	}

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		luaL_error(L, "cannot find %s lua module", thread->module->name);
		return 0;
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		luaL_error(L, "%s: thead callback %s does not exist", thread->module->file, func);
		return 0;
	}

	if((thread->callback = STRDUP((char *)func)) == NULL) {
		OUT_OF_MEMORY
	}

	plua_ret_true(L);

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
}

static void plua_async_thread_gc(void *ptr) {
	struct lua_thread_t *lua_thread = ptr;
	if(lua_thread->table != NULL) {
		plua_metatable_free(lua_thread->table);
	}
	if(lua_thread->work_req != NULL) {
		FREE(lua_thread->work_req);
	}
	if(lua_thread->callback != NULL) {
		FREE(lua_thread->callback);
	}
	FREE(lua_thread);
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

	switch(state->module->type) {
		case UNITTEST: {
			sprintf(p, "unittest.%s", state->module->name);
		} break;
		case FUNCTION: {
			sprintf(p, "function.%s", state->module->name);
		} break;
		case OPERATOR: {
			sprintf(p, "operator.%s", state->module->name);
		} break;
		case ACTION: {
			sprintf(p, "action.%s", state->module->name);
		} break;
	}

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		luaL_error(state->L, "cannot find %s lua module", name);
		return;
	}

	lua_getfield(state->L, -1, thread->callback);
	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		luaL_error(state->L, "%s: thread callback %s does not exist", state->module->file, thread->callback);
		return;
	}

	plua_async_thread_object(state->L, thread);

	if(lua_pcall(state->L, 1, 0, 0) == LUA_ERRRUN) {
		if(lua_type(state->L, -1) == LUA_TNIL) {
			logprintf(LOG_ERR, "%s: syntax error", state->module->file);
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
}

int plua_async_thread(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		luaL_error(L, "thread requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
	if(tp_work_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	struct lua_thread_t *lua_thread = MALLOC(sizeof(struct lua_thread_t));
	if(lua_thread == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(lua_thread, 0, sizeof(struct lua_thread_t));

	if((lua_thread->table = MALLOC(sizeof(struct plua_metatable_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(lua_thread->table, 0, sizeof(struct plua_metatable_t));

	lua_thread->module = state->module;
	lua_thread->work_req = tp_work_req;
	tp_work_req->data = lua_thread;
	lua_thread->L = L;

	plua_async_thread_object(L, lua_thread);

	plua_gc_reg(L, lua_thread, plua_async_thread_gc);

	assert(lua_gettop(L) == 1);

	return 1;
}

void plua_async_timer_gc(void *ptr) {
	struct lua_timer_t *lua_timer = ptr;
	if(lua_timer != NULL) {
		if(lua_timer->callback != NULL) {
			FREE(lua_timer->callback);
		}
		if(lua_timer->table != NULL) {
			plua_metatable_free(lua_timer->table);
		}
		FREE(lua_timer);
		lua_timer = NULL;
	}
}

static void timer_stop(lua_State *L, struct lua_timer_t *timer) {
	if(timer != NULL) {
		timer->running = 0;
		plua_gc_unreg(NULL, timer);
		if(timer->timer_req != NULL) {
			uv_timer_stop(timer->timer_req);
		}
		plua_async_timer_gc(timer);
	}
}

static int plua_async_timer_stop(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "timer:stop requires 0 argument, %d given", lua_gettop(L));
	}

	if(timer == NULL) {
		luaL_error(L, "internal error: timer object not passed");
	}

	timer_stop(timer->L, timer);

	plua_ret_true(L);

	return 1;
}

static int plua_async_timer_set_timeout(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));
	uv_timer_t *timer_req = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "timer.setTimeout requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(timer == NULL) {
		luaL_error(L, "internal error: timer object not passed");
		return 0;
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
		luaL_error(L, "timer timeout should be larger than 0, %d given", timeout);
		return 0;
	}

	timer->timeout = timeout;

	if(timer->running == 1) {
		uv_timer_start(timer_req, timer_callback, timer->timeout, timer->timeout);
	}

	plua_ret_true(L);

	return 1;
}

static int plua_async_timer_set_repeat(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));
	uv_timer_t *timer_req = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "timer.setRepeat requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(timer == NULL) {
		luaL_error(L, "internal error: timer object not passed");
		return 0;
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
		luaL_error(L, "timer repeat should be larger than 0, %d given", repeat);
		return 0;
	}

	timer->repeat = repeat;

	if(timer->running == 1) {
		uv_timer_start(timer_req, timer_callback, timer->timeout, timer->repeat);
	}

	plua_ret_true(L);

	return 1;
}

static int plua_async_timer_set_callback(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "timer.setCallback requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(timer == NULL) {
		luaL_error(L, "internal error: timer object not passed");
		return 0;
	}

	if(timer->module == NULL) {
		luaL_error(L, "internal error: lua state not properly initialized");
		return 1;
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
	switch(timer->module->type) {
		case UNITTEST: {
			sprintf(p, "unittest.%s", timer->module->name);
		} break;
		case FUNCTION: {
			sprintf(p, "function.%s", timer->module->name);
		} break;
		case OPERATOR: {
			sprintf(p, "operator.%s", timer->module->name);
		} break;
		case ACTION: {
			sprintf(p, "action.%s", timer->module->name);
		} break;
	}

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		luaL_error(L, "cannot find %s lua module", timer->module->name);
		return 0;
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		luaL_error(L, "%s: timer callback %s does not exist", timer->module->file, func);
		return 0;
	}

	if((timer->callback = STRDUP((char *)func)) == NULL) {
		OUT_OF_MEMORY
	}

	plua_ret_true(L);
	return 1;
}

static int plua_async_timer_start(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));
	uv_timer_t *timer_req = NULL;

	if(lua_gettop(L) != 0) {
		luaL_error(L, "timer.start requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(timer == NULL) {
		luaL_error(L, "internal error: timer object not passed");
		return 0;
	}

	timer_req = timer->timer_req;

	if(timer->callback == NULL) {
		if(timer->module != NULL) {
			luaL_error(L, "%s: timer callback has not been set", timer->module->file);
		} else {
			luaL_error(L, "timer callback has not been set");
		}
		return 0;
	}
	if(timer->timeout == -1) {
		luaL_error(L, "%s: timer timeout has not been set", timer->module->file);
		return 0;
	}
	timer->running = 1;

	uv_timer_start(timer_req, timer_callback, timer->timeout, timer->repeat);

	plua_ret_true(L);

	return 1;
}

static int plua_async_timer_set_data(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct plua_metatable_t *cpy = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "timer.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(timer == NULL) {
		luaL_error(L, "internal error: timer object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		cpy = (void *)lua_topointer(L, -1);
		lua_remove(L, -1);
		plua_metatable_clone(&cpy, &timer->table);

		plua_ret_true(L);

		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, timer->table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static int plua_async_timer_get_data(lua_State *L) {
	struct lua_timer_t *timer = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "timer.getUserdata requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(timer == NULL) {
		luaL_error(L, "internal error: timer object not passed");
		return 0;
	}

	lua_newtable(L);
	lua_newtable(L);

	lua_pushstring(L, "__index");
	lua_pushlightuserdata(L, timer->table);
	lua_pushcclosure(L, plua_metatable_get, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__newindex");
	lua_pushlightuserdata(L, timer->table);
	lua_pushcclosure(L, plua_metatable_set, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__gc");
	lua_pushlightuserdata(L, timer->table);
	lua_pushcclosure(L, plua_metatable_gc, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__pairs");
	lua_pushlightuserdata(L, timer->table);
	lua_pushcclosure(L, plua_metatable_pairs, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__next");
	lua_pushlightuserdata(L, timer->table);
	lua_pushcclosure(L, plua_metatable_next, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, timer->table);
	lua_pushcclosure(L, plua_metatable_call, 1);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);

	assert(lua_gettop(L) == 1);

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
}

static void timer_callback(uv_timer_t *req) {
	struct lua_timer_t *timer = req->data;
	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the timer is triggered
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = timer->module;

	logprintf(LOG_DEBUG, "lua timer on state #%d", state->idx);

	switch(state->module->type) {
		case UNITTEST: {
			sprintf(p, "unittest.%s", state->module->name);
		} break;
		case FUNCTION: {
			sprintf(p, "function.%s", state->module->name);
		} break;
		case OPERATOR: {
			sprintf(p, "operator.%s", state->module->name);
		} break;
		case ACTION: {
			sprintf(p, "action.%s", state->module->name);
		} break;
	}

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		luaL_error(state->L, "cannot find %s lua module", name);
		return;
	}

	lua_getfield(state->L, -1, timer->callback);
	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		luaL_error(state->L, "%s: timer callback %s does not exist", state->module->file, timer->callback);
		return;
	}

	plua_async_timer_object(state->L, timer);

	if(lua_pcall(state->L, 1, 0, 0) == LUA_ERRRUN) {
		if(lua_type(state->L, -1) == LUA_TNIL) {
			logprintf(LOG_ERR, "%s: syntax error", state->module->file);
			return;
		}
		if(lua_type(state->L, -1) == LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(state->L,  -1));
			lua_pop(state->L, -1);
			plua_clear_state(state);
			return;
		}
	}

	lua_remove(state->L, 1);
	plua_clear_state(state);
}

int plua_async_timer(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		luaL_error(L, "timer requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	uv_timer_t *timer_req = MALLOC(sizeof(uv_timer_t));
	if(timer_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	struct lua_timer_t *lua_timer = MALLOC(sizeof(struct lua_timer_t));
	if(lua_timer == NULL) {
		OUT_OF_MEMORY
	}
	memset(lua_timer, '\0', sizeof(struct lua_timer_t));

	if((lua_timer->table = MALLOC(sizeof(struct plua_metatable_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(lua_timer->table, 0, sizeof(struct plua_metatable_t));

	lua_timer->module = state->module;
	lua_timer->timer_req = timer_req;
	lua_timer->timeout = -1;
	lua_timer->L = L;
	timer_req->data = lua_timer;

	uv_timer_init(uv_default_loop(), timer_req);
	plua_gc_reg(NULL, lua_timer, plua_async_timer_gc);

	plua_async_timer_object(L, lua_timer);

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
