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

#include "common.h"

int plua_common_random(struct lua_State *L) {
	if(lua_gettop(L) != 2) {
		luaL_error(L, "random requires 2 arguments, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TNUMBER),
		1, buf);

	sprintf(p, error, lua_typename(L, lua_type(L, 2)));
	luaL_argcheck(L,
		(lua_type(L, 2) == LUA_TNUMBER),
		2, buf);

	int min = 0, max = 0;
	if(lua_type(L, -1) == LUA_TNUMBER || lua_type(L, -1) == LUA_TSTRING) {
		max = (int)lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	if(lua_type(L, -1) == LUA_TNUMBER || lua_type(L, -1) == LUA_TSTRING) {
		min = (int)lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	struct timeval t1;
	gettimeofday(&t1, NULL);
	srand(t1.tv_usec * t1.tv_sec);

	int r = rand() / (RAND_MAX + 1.0) * (max - min + 1) + min;

	lua_pushnumber(L, r);

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_common_explode(struct lua_State *L) {
	if(lua_gettop(L) != 2) {
		luaL_error(L, "random requires 2 arguments, %d given", lua_gettop(L));
	}

	char **array = NULL;
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	int i = 0, n = 0;

	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TSTRING),
		1, buf);

	sprintf(p, error, lua_typename(L, lua_type(L, 2)));
	luaL_argcheck(L,
		(lua_type(L, 2) == LUA_TSTRING),
		2, buf);

	const char *str = NULL, *delimiter = NULL;

	if(lua_type(L, -1) == LUA_TSTRING) {
		delimiter = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	if(lua_type(L, -1) == LUA_TSTRING) {
		str = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	p = (char *)str;
	n = explode(p, delimiter, &array);
	lua_createtable(L, n, 0);
	for(i=0;i<n;i++) {
		lua_pushnumber(L, i+1);
		lua_pushstring(L, array[i]);
		lua_settable(L, -3);
	}
	array_free(&array, n);

	assert(lua_gettop(L) == 1);

	return 1;
}
