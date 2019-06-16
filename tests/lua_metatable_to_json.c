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
#include "../libs/pilight/lua_c/table.h"
#include "../libs/pilight/lua_c/lualibrary.h"

static CuTest *gtc = NULL;
static int run = 0;
static int test = 0;

static void test_metatable_to_json(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	struct lua_state_t *state = NULL;

	plua_init();
	plua_pause_coverage(1);

	state = plua_get_free_state();
	CuAssertPtrNotNull(gtc, state);

	luaL_loadstring(state->L, " \
		local table = pilight.table(); \
		table[1] = 'a'; \
		table['c'] = 1; \
		table['b'] = true; \
		table['a'] = {}; \
		table['a'][1] = {}; \
		table['a'][2] = {}; \
		table['a'][1]['c'] = {}; \
		table['a'][2]['c'] = {}; \
		table['a'][1]['c'][1] = {}; \
		table['a'][1]['c'][2] = {}; \
		table['a'][2]['c'][1] = {}; \
		table['a'][2]['c'][2] = {}; \
		table['a'][1]['c'][1]['e'] = 'a'; \
		table['a'][1]['c'][2]['e'] = 'b'; \
		table['a'][2]['c'][1]['e'] = 'c'; \
		table['a'][2]['c'][2]['e'] = 'd'; \
		return table(); \
	");

	plua_pcall(state->L, __FILE__, 0, 1);

	struct plua_metatable_t *table = NULL;
	struct JsonNode *json = NULL;

	CuAssertIntEquals(tc, LUA_TLIGHTUSERDATA, lua_type(state->L, -1));

	table = (void *)lua_topointer(state->L, -1);
	plua_metatable_to_json(table, &json);
	char *out = json_stringify(json, NULL);
	CuAssertStrEquals(tc, "{\"1\":\"a\",\"c\":1,\"b\":true,\"a\":[{\"c\":[{\"e\":\"a\"},{\"e\":\"b\"}]},{\"c\":[{\"e\":\"c\"},{\"e\":\"d\"}]}]}", out);
	json_free(out);
	json_delete(json);

	lua_pop(state->L, -1);
	plua_clear_state(state);

	run = 0;
	test = 0;

	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_metatable_to_json(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_metatable_to_json);

	return suite;
}
