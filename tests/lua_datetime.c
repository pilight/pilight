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
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"

static struct {
	union {
		int number_;
		char *string_;
	} var;
	int type;
} lua_return[10];

static int nrvar = 0;
static CuTest *gtc = NULL;

static int plua_print(lua_State* L) {
	int nargs = lua_gettop(L);
	int i = 0;

	CuAssertIntEquals(gtc, nargs, nrvar);

	for(i=1; i <= nargs; ++i) {
		CuAssertIntEquals(gtc, lua_type(L, i), lua_return[i-1].type);

		switch(lua_type(L, i)) {
			case LUA_TSTRING:
				CuAssertStrEquals(gtc, lua_return[i-1].var.string_, lua_tostring(L, i));
			break;
			case LUA_TNUMBER:
				CuAssertIntEquals(gtc, lua_return[i-1].var.number_, lua_tonumber(L, i));
			break;
			case LUA_TBOOLEAN:
				CuAssertIntEquals(gtc, lua_return[i-1].var.number_, (int)lua_toboolean(L, i));
			break;
			case LUA_TTABLE: {
				struct JsonNode *json = json_mkobject();
				lua_pushnil(L);
				while(lua_next(L, -2)) {
					lua_pushvalue(L, -2);
					const char *key = lua_tostring(L, -1);
					const char *value = lua_tostring(L, -2);
					json_append_member(json, key, json_mkstring(value));
					lua_pop(L, 2);
				}
				lua_pop(L, 1);

				char *out = json_stringify(json, NULL);
				CuAssertStrEquals(gtc, lua_return[i-1].var.string_, out);
				json_free(out);
				json_delete(json);
			} break;
		}
	}
	return 0;
}

static void test_lua_datetime_strptime(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);

	struct lua_state_t *state = plua_get_free_state();

	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.datetime.strptime())"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.datetime.strptime('a', 1, true))"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.datetime.strptime('a', 1))"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.datetime.strptime(\"6 Dec 2001 12:33:45\", \"%H:%M:%S %d %b %Y\"))"));

	nrvar = 1;
	lua_return[0].var.string_ = STRDUP(
		"{\"day\":\"6\",\"weekday\":\"4\",\"sec\":\"45\",\"min\":\"33\",\"year\":\"2001\",\"month\":\"12\",\"hour\":\"12\"}");
	lua_return[0].type = LUA_TTABLE;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.datetime.strptime(\"6 Dec 2001 12:33:45\", \"%d %b %Y %H:%M:%S\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP(
		"{\"day\":\"16\",\"weekday\":\"0\",\"sec\":\"36\",\"min\":\"4\",\"year\":\"2016\",\"month\":\"10\",\"hour\":\"21\"}");
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.datetime.strptime(\"2016-10-16 21:04:36\", \"%Y-%m-%d %H:%M:%S\"))"));
	FREE(lua_return[0].var.string_);

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_datetime(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_datetime_strptime);

	return suite;
}
