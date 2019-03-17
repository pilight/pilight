/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../libs/libuv/uv.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"

#include "alltests.h"

static CuTest *gtc = NULL;
static uv_async_t *async_close_req = NULL;
static uv_thread_t pth;
static int level = 0;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_stop(uv_default_loop());
}

static int plua_print(lua_State* L) {
	CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TNUMBER);
	CuAssertIntEquals(gtc, level++, lua_tonumber(L, -1));

	return 0;
}

static void test(void *param) {
	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	int ret = luaL_dostring(state->L, "\
		print(LOG_EMERG);\
		print(LOG_ALERT);\
		print(LOG_CRIT);\
		print(LOG_ERR);\
		print(LOG_WARNING);\
		print(LOG_NOTICE);\
		print(LOG_INFO);\
		print(LOG_DEBUG);\
		pilight.log(LOG_EMERG, \"lua emergency\");\
		pilight.log(LOG_ALERT, \"lua alert\");\
		pilight.log(LOG_CRIT, \"lua critical\");\
		pilight.log(LOG_ERR, \"lua test\");\
		pilight.log(LOG_WARNING, \"lua test\");\
		pilight.log(LOG_NOTICE, \"lua test\");\
		pilight.log(LOG_INFO, \"lua test\");\
		pilight.log(LOG_DEBUG, \"lua test\");\
	");
	plua_stack_dump(state->L);
	CuAssertIntEquals(gtc, 0, ret);

	plua_clear_state(state);

	plua_pause_coverage(0);
	plua_gc();

	uv_async_send(async_close_req);
	return;
}

static void test_lua_log(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	eventpool_init(EVENTPOOL_NO_THREADS);

	CuAssertTrue(tc, open("test.log", O_TRUNC | O_WRONLY | O_APPEND | O_CREAT, 0666) > 0);

	log_init();
	log_file_set("test.log");
	log_level_set(LOG_DEBUG);
	log_file_enable();
	log_shell_enable();

	uv_thread_create(&pth, test, NULL);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth);

	char *tmp = NULL, **array = NULL;
	int n = file_get_contents("test.log", &tmp), x = 0, i = 0;
	CuAssertIntEquals(tc, 0, n);
	x = explode(tmp, "\n", &array);
	CuAssertIntEquals(tc, 7, x);

	for(i=0;i<x;i++) {
		if(i == 0) {
			CuAssertPtrNotNull(tc, strstr(array[i], "lua emergency"));
		} else if(i == 1) {
			CuAssertPtrNotNull(tc, strstr(array[i], "lua alert"));
		} else if(i == 2) {
			CuAssertPtrNotNull(tc, strstr(array[i], "lua critical"));
		} else if(i == 3) {
			CuAssertPtrNotNull(tc, strstr(array[i], "ERROR: lua test"));
		} else if(i == 4) {
			CuAssertPtrNotNull(tc, strstr(array[i], "WARNING: lua test"));
		} else if(i == 5) {
			CuAssertPtrNotNull(tc, strstr(array[i], "NOTICE: lua test"));
		} else if(i == 6) {
			CuAssertPtrNotNull(tc, strstr(array[i], "INFO: lua test"));
		} else if(i == 7) {
			CuAssertPtrNotNull(tc, strstr(array[i], "DEBUG: lua test"));
		}
	}
	FREE(tmp);

	array_free(&array, x);

	log_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_log(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_log);

	return suite;
}