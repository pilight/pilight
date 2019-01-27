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
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/table.h"
#include "../libs/pilight/lua_c/lualibrary.h"

static int run = 0;
static int test = 0;
static int check = 0;
static int check1 = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static int plua_print(lua_State* L) {
	switch((int)lua_tonumber(L, -2)) {
		case 0: {
			switch((int)lua_tonumber(L, -3)) {
				case 1: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 2: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
			}
		} break;
		case 10000: {
			switch((int)lua_tonumber(L, -3)) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
					run++;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
			}
		} break;
		case 10001: {
			switch((int)lua_tonumber(L, -3)) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
					run++;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					if(check == 0) {
						CuAssertIntEquals(gtc, 1, (int)lua_toboolean(L, -1));
					} else {
						CuAssertIntEquals(gtc, 0, (int)lua_toboolean(L, -1));
					}
					check = 1;
					run++;
				} break;
				case 2: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
			}
		} break;
		case 10002: {
			switch((int)lua_tonumber(L, -3)) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
					run++;
				} break;
				case 1: {
					switch(check1++) {
						case 0: {
							CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
							CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
						} break;
						case 1: {
							CuAssertTrue(gtc, (LUA_TBOOLEAN == lua_type(L, -1)) || (LUA_TNUMBER == lua_type(L, -1)));
							if(lua_type(L, -1) == LUA_TBOOLEAN) {
								CuAssertIntEquals(gtc, 0, lua_toboolean(L, -1));
							} else if(lua_type(L, -1) == LUA_TBOOLEAN) {
								CuAssertIntEquals(gtc, 2, lua_tonumber(L, -1));
							}
						} break;
						case 2:
						case 3: {
							CuAssertTrue(gtc, (LUA_TBOOLEAN == lua_type(L, -1)) || (LUA_TNUMBER == lua_type(L, -1)));
							if(lua_type(L, -1) == LUA_TBOOLEAN) {
								CuAssertIntEquals(gtc, 0, lua_toboolean(L, -1));
							} else if(lua_type(L, -1) == LUA_TBOOLEAN) {
								CuAssertIntEquals(gtc, 2, lua_tonumber(L, -1));
							}
						} break;
					}
					run++;
				} break;
				case 2: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
			}
		}
	}

	if((run >= 24) && (run <= 25)) {
		uv_stop(uv_default_loop());
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

static void test_lua_async_event_missing_parameters(CuTest *tc) {
	struct lua_state_t *state = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.callback();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.setCallback(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.register();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.register('a');"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.register({});"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.register(-1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.register(9999);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.unregister();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.unregister('a');"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.unregister({});"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.unregister(-1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.unregister(9999);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.trigger();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.trigger('a');"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local event = pilight.async.event(); event.trigger(1);"));

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	plua_gc();
	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void *eventpool_trigger_free(void *param) {
	struct plua_metatable_t *table = param;
	plua_metatable_free(table);
	return NULL;
}

static void *listener(int reason, void *param, void *userdata) {
	struct plua_metatable_t *table = param;
	int _bool = 0;

	CuAssertIntEquals(gtc, 10000, reason);
	CuAssertIntEquals(gtc, 0, plua_metatable_get_boolean(table, "status", &_bool));
	CuAssertIntEquals(gtc, 1, _bool);

	struct plua_metatable_t *table1 = MALLOC(sizeof(struct plua_metatable_t));
	memset(table1, 0, sizeof(struct plua_metatable_t));

	plua_metatable_set_number(table1, "status", 2);
	eventpool_trigger(10002, eventpool_trigger_free, table1);

	return NULL;
}

static void test_lua_async_event(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_async_event.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%sevent.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("event", UNITTEST));

	plua_clear_state(state);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 100, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(10000, listener, NULL);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "event");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("event", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	plua_clear_state(state);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_pause_coverage(0);
	plua_gc();

	eventpool_gc();

	CuAssertTrue(tc, (run >= 24) && (run <= 25));
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_async_event(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_async_event_missing_parameters);
	SUITE_ADD_TEST(suite, test_lua_async_event);

	return suite;
}
