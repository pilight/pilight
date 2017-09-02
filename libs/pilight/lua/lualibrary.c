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

#ifndef _WIN32
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"

static int plua_toboolean(struct lua_State *L) {
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string, number, or boolean expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_isstring(L, 1) || lua_isnumber(L, 1) || lua_isboolean(L, 1)),
		1, buf);

	switch(lua_type(L, 1)) {
		case LUA_TSTRING: {
			const char *str = lua_tostring(L, 1);
			if(strcmp(str, "0") == 0 || strlen(str) == 0) {
				lua_pop(L, -1);
				lua_pushboolean(L, 0);
			} else {
				lua_pop(L, -1);
				lua_pushboolean(L, 1);
			}
		} break;
		case LUA_TNUMBER: {
			int num = (int)lua_tonumber(L, 1);
			lua_pop(L, -1);
			lua_pushboolean(L, num);
		} break;
		case LUA_TBOOLEAN: {
			int b = lua_toboolean(L, 1);
			lua_pop(L, -1);
			lua_pushboolean(L, b);
		} break;
	}

	return 1;
}

static int plua_tonumber(struct lua_State *L) {
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string, number, or boolean expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_isstring(L, 1) || lua_isnumber(L, 1) || lua_isboolean(L, 1)),
		1, buf);

	switch(lua_type(L, 1)) {
		case LUA_TSTRING: {
			const char *str = lua_tostring(L, 1);
			float num = atof(str);
			lua_pop(L, -1);
			lua_pushnumber(L, num);
		} break;
		case LUA_TNUMBER: {
			float num = lua_tonumber(L, 1);
			lua_pop(L, -1);
			lua_pushnumber(L, num);
		} break;
		case LUA_TBOOLEAN: {
			int b = lua_toboolean(L, 1);
			lua_pop(L, -1);
			if(b == 0) {
				lua_pushnumber(L, 0);
			} else {
				lua_pushnumber(L, 1);
			}
		} break;
	}

	return 1;
}

static int plua_tostring(struct lua_State *L) {
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string, number, or boolean expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_isstring(L, 1) || lua_isnumber(L, 1) || lua_isboolean(L, 1)),
		1, buf);

	switch(lua_type(L, 1)) {
		case LUA_TSTRING: {
			const char *str = lua_tostring(L, 1);
			lua_pop(L, -1);
			lua_pushstring(L, str);
		} break;
		case LUA_TNUMBER: {
			float num = lua_tonumber(L, 1);
			lua_pop(L, -1);

			char *tmp = NULL;
			int len = snprintf(NULL, 0, "%.6f", num);
			if((tmp = MALLOC(len+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			memset(tmp, 0, len+1);
			sprintf(tmp, "%.6f", num);
			lua_pushstring(L, tmp);
			FREE(tmp);
		} break;
		case LUA_TBOOLEAN: {
			int b = lua_toboolean(L, 1);
			lua_pop(L, -1);
			if(b == 0) {
				lua_pushstring(L, "0");
			} else {
				lua_pushstring(L, "1");
			}
		} break;
	}

	return 1;
}

static const luaL_Reg pilightlib[] = {
  {"toboolean", plua_toboolean},
  {"tonumber", plua_tonumber},
  {"tostring", plua_tostring},
  {NULL, NULL}
};

void plua_register_library(struct lua_State *L) {
	/*
	 * Register pilight lua library
	 */
	lua_newtable(L);
	luaL_setfuncs(L, pilightlib, 0);
	lua_pushvalue(L, -1);
	lua_setglobal(L, "pilight");
}