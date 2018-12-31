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
#include <assert.h>

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "cast.h"

int plua_cast_toboolean(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "toboolean requires 1 arguments, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string, number, or boolean expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		((lua_type(L, 1) == LUA_TSTRING) || (lua_type(L, 1) == LUA_TNUMBER) || (lua_type(L, 1) == LUA_TBOOLEAN)),
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

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_cast_tonumber(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "tonumber requires 1 arguments, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string, number, or boolean expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		((lua_type(L, 1) == LUA_TSTRING) || (lua_type(L, 1) == LUA_TNUMBER) || (lua_type(L, 1) == LUA_TBOOLEAN)),
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

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_cast_tostring(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "tostring requires 1 arguments, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string, number, or boolean expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		((lua_type(L, 1) == LUA_TSTRING) || (lua_type(L, 1) == LUA_TNUMBER) || (lua_type(L, 1) == LUA_TBOOLEAN)),
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

	assert(lua_gettop(L) == 1);

	return 1;
}