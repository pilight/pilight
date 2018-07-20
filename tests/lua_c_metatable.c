/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/lua/lua.h"
#include "../libs/pilight/lua/lualibrary.h"

static CuTest *gtc = NULL;
static int run = 0;

static int plua_print(lua_State* L) {
	switch(run) {
		case 0:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "b", lua_tostring(L, -1));
			run++;
		break;
		case 1:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "c", lua_tostring(L, -1));
			run++;
		break;
		case 2:
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		break;
		case 3:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "d", lua_tostring(L, -1));
			run++;
		break;
		case 4:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "b", lua_tostring(L, -1));
			run++;
		break;
		case 5:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "c", lua_tostring(L, -1));
			run++;
		break;
		case 6:
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		break;
		case 7:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "d", lua_tostring(L, -1));
			run++;
		break;
		case 8:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
			run++;
		break;
		case 9:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 2, lua_tonumber(L, -1));
			run++;
		break;
		case 10:
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		break;
		case 11:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 3, lua_tonumber(L, -1));
			run++;
		break;
		case 12:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 2, lua_tonumber(L, -1));
			run++;
		break;
		case 13:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 4, lua_tonumber(L, -1));
			run++;
		break;
		case 14:
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		break;
		case 15:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 5, lua_tonumber(L, -1));
			run++;
		break;
		case 16:
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		break;
		case 17:
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		break;
		case 18:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 3, lua_tonumber(L, -1));
			run++;
		break;
		case 19:
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		break;
		case 20:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "d", lua_tostring(L, -1));
			run++;
		break;
		case 21:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 5, lua_tonumber(L, -1));
			run++;
		break;
		case 22:
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "c", lua_tostring(L, -1));
			run++;
		break;
		case 23:
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 2, lua_tonumber(L, -1));
			run++;
		break;
	}
	return 1;
}

static void test_lua_c_metatable(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	run = 0;

	struct lua_state_t *state = NULL;

	plua_init();
	plua_override_global("print", plua_print);

	state = plua_get_free_state();
	CuAssertPtrNotNull(gtc, state);
	CuAssertIntEquals(gtc, 0, luaL_dostring(state->L, " \
		local thread = pilight.async.thread(); \
		local data = thread.getUserdata(); \
		data['a'] = 'b'; \
		data['b'] = 'c'; \
		print(data['a']); \
		print(data['b']); \
		data['a'] = nil; \
		print(data['a']); \
		data['b'] = 'd'; \
		print(data['b']); \
		data[1] = 'b'; \
		data[2] = 'c'; \
		print(data[1]); \
		print(data[2]); \
		data[1] = nil; \
		print(data[1]); \
		data[2] = 'd'; \
		print(data[2]); \
		data['b'] = 1; \
		data['c'] = 2; \
		print(data['b']); \
		print(data['c']); \
		data['b'] = nil; \
		print(data['b']); \
		data['c'] = 3; \
		print(data['c']); \
		data[1] = 2; \
		data[3] = 4; \
		print(data[1]); \
		print(data[3]); \
		data[1] = nil; \
		print(data[1]); \
		data[3] = 5; \
		print(data[3]); \
		local thread1 = pilight.async.thread(); \
		local foo = 1;\
		thread1.setUserdata(data()); \
		local data1 = thread1.getUserdata(); \
		print(data1['a']); \
		print(data1['b']); \
		print(data1['c']); \
		print(data1[1]); \
		print(data1[2]); \
		print(data1[3]); \
	"));

	/*
	 * After the print are a few tests that
	 * check if old table are properly freed
	 */
	CuAssertIntEquals(gtc, 0, luaL_dostring(state->L, " \
		local thread = pilight.async.thread(); \
		local data = thread.getUserdata(); \
		local a = {}; \
		a['b'] = {}; \
		a['b'][1] = 'c'; \
		a['b']['d'] = 2; \
		thread.setUserdata(a); \
		print(data['b'][1]); \
		print(data['b']['d']); \
		data['b'] = nil; \
		data['b'] = {}; \
		data['b'] = 'a'; \
		data['b'] = {}; \
		data['b'] = {}; \
		data['b'] = 1; \
	"));

	uv_mutex_unlock(&state->lock);
	plua_gc();

	CuAssertIntEquals(tc, 24, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_c_metatable(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_c_metatable);

	return suite;
}
