/*
  Copyright (C) CurlyMo

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
#include "../libs/pilight/lua_c/lualibrary.h"

static CuTest *gtc = NULL;
static int run = 0;

static int plua_print(lua_State* L) {
	switch(run++) {
		case 0: {
			CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TTABLE);
		} break;
		case 1: {
			CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TTABLE);
		} break;
		case 2: {
			CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TBOOLEAN);
			CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
		} break;
		case 3: {
			CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TBOOLEAN);
			CuAssertIntEquals(gtc, 0, lua_toboolean(L, -1));
		} break;
		case 4: {
			CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TNUMBER);
			CuAssertIntEquals(gtc, 461, lua_tonumber(L, -1));
		} break;
		case 5: {
			CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TNUMBER);
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
		} break;
		case 6: {
			CuAssertIntEquals(gtc, lua_type(L, -1), LUA_TSTRING);
			const char *str = lua_tostring(L, -1);

			int ret = strcmp(str, "{\n" \
				"\t\"devices\": {},\n" \
				"\t\"gui\": {},\n" \
				"\t\"rules\": {},\n" \
				"\t\"settings\": {\n" \
					"\t\t\"log-level\": 6,\n" \
					"\t\t\"pid-file\": \"/var/run/pilight.pid\",\n" \
					"\t\t\"log-file\": \"/var/log/pilight.log\",\n" \
					"\t\t\"gpio-platform\": \"raspberrypi1b2\",\n" \
					"\t\t\"webserver-enable\": 1,\n" \
					"\t\t\"webserver-root\": \"/usr/local/share/pilight/\",\n" \
					"\t\t\"webserver-http-port\": 5001,\n" \
					"\t\t\"webserver-https-port\": 5002,\n" \
					"\t\t\"webserver-cache\": 1,\n" \
					"\t\t\"whitelist\": \"\"\n" \
					"\t},\n"\
				"\t\"hardware\": {\n" \
					"\t\t\"433gpio\": {\n" \
						"\t\t\t\"sender\": 0,\n" \
						"\t\t\t\"receiver\": 1\n" \
					"\t\t}\n" \
				"\t},\n" \
				"\t\"registry\": {}\n" \
			"}");
			CuAssertIntEquals(gtc, 0, ret);
		} break;
	}
	return 1;
}

static void test_lua_io_file(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	remove("lua_io_c.txt");

	int ret = luaL_dostring(state->L, "\
		local file = pilight.io.file(\"../res/config/config.json-default\"); \
		local file1 = pilight.io.file(\"lua_io_c.txt\"); \
		print(file); \
		print(file1); \
		print(file.exists()); \
		print(file1.exists()); \
		file.open(\"r\"); \
		print(file.seek(0, \"end\")); \
		print(file.seek(0, \"set\")); \
		local content = ''; \
		for line in file.readline() do \
			content = content .. line; \
		end \
		print(content); \
		file1.open(\"w+\"); \
		file1.write(content); \
		file.close(); \
		file1.close(); \
	");
	CuAssertIntEquals(tc, 0, ret);

	plua_clear_state(state);

	plua_pause_coverage(0);
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_io_dir(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	remove("lua_io_c.txt");

	int ret = luaL_dostring(state->L, "\
		local path = pilight.io.dir(\"/tmp\"); \
		local path1 = pilight.io.dir(\"/foo\"); \
		print(path); \
		print(path1); \
		print(path.exists());\
		print(path1.exists());\
		path.close(); \
	");
	CuAssertIntEquals(tc, 0, ret);

	plua_clear_state(state);

	plua_pause_coverage(0);
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}


CuSuite *suite_lua_io(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_io_file);
	SUITE_ADD_TEST(suite, test_lua_io_dir);

	return suite;
}
