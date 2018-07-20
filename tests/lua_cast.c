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

static void test_lua_cast_toboolean(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	struct lua_state_t *state = plua_get_free_state();

	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.cast.toboolean())"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.cast.toboolean(1, 2))"));

	nrvar = 1;
	lua_return[0].var.number_ = 1;
	lua_return[0].type = LUA_TBOOLEAN;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean(1))"));

	lua_return[0].var.number_ = 1;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean(1.12345))"));

	lua_return[0].var.number_ = 0;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean(0))"));

	lua_return[0].var.number_ = 0;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean(\"\"))"));

	lua_return[0].var.number_ = 1;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean('2'))"));

	lua_return[0].var.number_ = 1;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean('foo'))"));

	lua_return[0].var.number_ = 0;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean('0'))"));

	lua_return[0].var.number_ = 1;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean('false'))"));

	lua_return[0].var.number_ = 1;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean('true'))"));

	lua_return[0].var.number_ = 0;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean(false))"));

	lua_return[0].var.number_ = 1;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.toboolean(true))"));

	uv_mutex_unlock(&state->lock);
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_cast_tostring(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	struct lua_state_t *state = plua_get_free_state();

	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.cast.tostring())"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.cast.tostring(1, 2))"));

	nrvar = 1;
	lua_return[0].var.string_ = STRDUP("1.000000");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(1))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("0.000000");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(0))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("1.123450");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(1.12345))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(\"\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("2");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(\"2\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("foo");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(\"foo\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("false");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(\"false\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("0");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(\"0\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("false");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(\"false\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("0");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(\"0\"))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("0");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(false))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("1");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(true))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("10.000000");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(10))"));
	FREE(lua_return[0].var.string_);

	lua_return[0].var.string_ = STRDUP("10.000000");
	lua_return[0].type = LUA_TSTRING;
	CuAssertPtrNotNull(tc, lua_return[0].var.string_);

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tostring(10.00000000))"));
	FREE(lua_return[0].var.string_);

	uv_mutex_unlock(&state->lock);
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_cast_tonumber(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	struct lua_state_t *state = plua_get_free_state();

	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.cast.tonumber())"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "print(pilight.cast.tonumber('1', '2'))"));

	nrvar = 1;
	lua_return[0].var.number_ = 1.000000;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(1))"));

	lua_return[0].var.number_ = 0.000000;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(0))"));

	lua_return[0].var.number_ = 1.123450;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(1.12345))"));

	lua_return[0].var.number_ = 0;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(\"\"))"));

	lua_return[0].var.number_ = 2;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(\"2\"))"));

	lua_return[0].var.number_ = 0;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(\"foo\"))"));

	lua_return[0].var.number_ = 0;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(\"false\"))"));

	lua_return[0].var.number_ = 0;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(\"0\"))"));

	lua_return[0].var.number_ = 0;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(\"false\"))"));

	lua_return[0].var.number_ = 0;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(\"0\"))"));

	lua_return[0].var.number_ = 0;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(false))"));

	lua_return[0].var.number_ = 1;
	lua_return[0].type = LUA_TNUMBER;

	CuAssertIntEquals(tc, 0, luaL_dostring(state->L, "print(pilight.cast.tonumber(true))"));

	uv_mutex_unlock(&state->lock);
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_cast(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_cast_toboolean);
	SUITE_ADD_TEST(suite, test_lua_cast_tostring);
	SUITE_ADD_TEST(suite, test_lua_cast_tonumber);

	return suite;
}
