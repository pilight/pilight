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
static int check = 0;
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
			case 4: 
			case 5:
			case 10:
			case 11: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "boolean", lua_tostring(L, -1));
				run++;
			} break;
			case 6: {
				CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
				CuAssertIntEquals(gtc, 5, lua_tonumber(L, -1));
				run++;
			} break;
			case 7:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "main", lua_tostring(L, -1));
				run++;
			break;
			case 8: {
				CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
				CuAssertIntEquals(gtc, 4, lua_tonumber(L, -1));
				run++;
			} break;
			case 9:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "main1", lua_tostring(L, -1));
				run++;
			break;
			case 12:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				if(check == 1 || strcmp("thread1", lua_tostring(L, -1)) == 0) {
					CuAssertStrEquals(gtc, "thread1", lua_tostring(L, -1));
					check = 0;
				} else if(check == 0) {
					CuAssertStrEquals(gtc, "thread", lua_tostring(L, -1));
					check = 1;
				} else {
					CuAssertTrue(gtc, 0);
				}
				run++;
			break;
			case 13:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				if(check == 1 || strcmp("thread1", lua_tostring(L, -1)) == 0) {
					CuAssertStrEquals(gtc, "thread1", lua_tostring(L, -1));
					check = 0;
				} else if(check == 0) {
					CuAssertStrEquals(gtc, "thread", lua_tostring(L, -1));
					check = 1;
				} else {
					CuAssertTrue(gtc, 0);
				}
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
				CuAssertIntEquals(gtc, LUA_TFUNCTION, lua_type(L, -1));
				run++;
			break;
			case 4:
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
				CuAssertStrEquals(gtc, "thread", lua_tostring(L, -1));
				run++;
				uv_stop(uv_default_loop());
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
		logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
		lua_pop(L, 1);
		return -1;
	}

	lua_pop(L, 1);

	return 1;
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

static void test_lua_async_thread_missing_parameters(CuTest *tc) {
	struct lua_state_t *state = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local thread = pilight.async.thread(); thread.callback();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local thread = pilight.async.thread(); thread.setCallback(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local thread = pilight.async.thread(); thread.trigger();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local thread = pilight.async.thread(); thread.trigger(\"foo\");"));

	uv_mutex_unlock(&state->lock);

	plua_gc();
	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_async_thread(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 1;

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_async_thread.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%sthread.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("thread", UNITTEST));

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

	sprintf(name, "unittest.%s", "thread");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("thread", tmp->name) == 0) {
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
	CuAssertIntEquals(tc, 14, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_async_thread_nonexisting_callback(CuTest *tc) {
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
	plua_override_global("print", plua_print);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_async_thread.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%sthread1.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("thread", UNITTEST));

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

	sprintf(name, "unittest.%s", "thread");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("thread", tmp->name) == 0) {
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
	CuAssertIntEquals(tc, 2, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_async_thread(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_async_thread_missing_parameters);
	SUITE_ADD_TEST(suite, test_lua_async_thread);
	SUITE_ADD_TEST(suite, test_lua_async_thread_nonexisting_callback);

	return suite;
}
