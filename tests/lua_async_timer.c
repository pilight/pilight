/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/lua/lua.h"
#include "../libs/pilight/lua/lualibrary.h"

static int run = 0;
static int test = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static int plua_print(lua_State* L) {
	if(test == 1) {
		switch(run) {
			case 0:
			case 1:
			case 2: 
			case 3: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "table", lua_tostring(L, -1));
				run++;
			} break;
			case 4: {
				CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
				CuAssertIntEquals(gtc, 5, lua_tonumber(L, -1));
				run++;
			} break;
			case 5:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "main", lua_tostring(L, -1));
				run++;
			break;
			case 6: {
				CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
				CuAssertIntEquals(gtc, 4, lua_tonumber(L, -1));
				run++;
			} break;
			case 7:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "main1", lua_tostring(L, -1));
				run++;
			break;
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "boolean", lua_tostring(L, -1));
				run++;
			} break;
			case 16:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "timer", lua_tostring(L, -1));
				run++;
			break;
			case 17:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "boolean", lua_tostring(L, -1));
				run++;
			break;
			case 18:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "timer1", lua_tostring(L, -1));
				run++;
				uv_stop(uv_default_loop());
			break;
		}
	} else {
		switch(run) {
			case 0:
			case 1: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "table", lua_tostring(L, -1));
				run++;
			} break;
			case 2:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "main", lua_tostring(L, -1));
				run++;
			break;
			case 3:
				CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
				run++;
			break;
			case 4:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "timer", lua_tostring(L, -1));
				run++;
			break;
		}
	}

	return 0;
}

static int call(struct lua_State *L, char *file, char *func) {
	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		return -1;
	}

	if(lua_pcall(L, 0, 0, 0) == LUA_ERRRUN) {
		if(lua_type(L, -1) == LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
			lua_pop(L, 1);
			return -1;
		}
	}
	lua_pop(L, 1);

	return 1;
}

static void plua_overwrite_print(void) {
	struct lua_state_t *state[NRLUASTATES];
	struct lua_State *L = NULL;
	int i = 0;

	for(i=0;i<NRLUASTATES;i++) {
		state[i] = plua_get_free_state();

		if(state[i] == NULL) {
			return;
		}
		if((L = state[i]->L) == NULL) {
			uv_mutex_unlock(&state[i]->lock);
			return;
		}

		lua_getglobal(L, "_G");
		lua_pushcfunction(L, plua_print);
		lua_setfield(L, -2, "print");
		lua_pop(L, 1);
	}
	for(i=0;i<NRLUASTATES;i++) {
		uv_mutex_unlock(&state[i]->lock);
	}
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void stop(uv_timer_t *req) {
	uv_stop(uv_default_loop());
}

static void test_lua_async_timer_missing_parameters(CuTest *tc) {
	struct lua_state_t *state = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();
	
	plua_init();
	plua_overwrite_print();

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.callback();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.setCallback(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.setRepeat(-1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.setRepeat('a');"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.setTimeout(-1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.setTimeout('a');"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.start();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.start(\"timer\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local timer = pilight.async.timer(); timer.stop(\"timer\");"));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_mutex_unlock(&state->lock);

	plua_gc();
	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_async_timer(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	memset(name, 0, 255);

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 1;

	gtc = tc;
	memtrack();
	
	plua_init();
	plua_overwrite_print();

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);	

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_async_timer.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%stimer.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("timer", UNITTEST));
	
	uv_mutex_unlock(&state->lock);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "timer");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("timer", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	uv_mutex_unlock(&state->lock);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_gc();
	CuAssertIntEquals(tc, 19, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_async_timer_prematurely_stopped(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	memset(name, 0, 255);

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 1;

	gtc = tc;
	memtrack();
	
	plua_init();
	plua_overwrite_print();

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);	

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_async_timer.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%stimer.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("timer", UNITTEST));
	
	uv_mutex_unlock(&state->lock);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);	
	
	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "timer");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("timer", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	uv_mutex_unlock(&state->lock);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_gc();
	CuAssertIntEquals(tc, 18, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_async_timer_nonexisting_callback(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 2;

	gtc = tc;
	memtrack();
	
	plua_init();
	plua_overwrite_print();

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);	

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_async_timer.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%stimer1.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("timer", UNITTEST));

	uv_mutex_unlock(&state->lock);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "timer");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("timer", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, -1, call(L, file, "run"));

	lua_pop(L, -1);

	uv_mutex_unlock(&state->lock);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_gc();
	CuAssertIntEquals(tc, 3, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_async_timer(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_async_timer_missing_parameters);
	SUITE_ADD_TEST(suite, test_lua_async_timer);
	SUITE_ADD_TEST(suite, test_lua_async_timer_prematurely_stopped);
	SUITE_ADD_TEST(suite, test_lua_async_timer_nonexisting_callback);

	return suite;
}
