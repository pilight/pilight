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

#include "../core/strptime.h"
#include "../core/datetime.h"
#include "datetime.h"

int plua_datetime_strptime(struct lua_State *L) {
	if(lua_gettop(L) != 2) {
		luaL_error(L, "strptime requires 2 arguments, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TSTRING),
		1, buf);

	sprintf(p, error, lua_typename(L, lua_type(L, 2)));
	luaL_argcheck(L,
		(lua_type(L, 2) == LUA_TSTRING),
		2, buf);

	const char *datetime = NULL;
	const char *format = NULL;
	if(lua_type(L, -1) == LUA_TSTRING) {
		format = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	if(lua_type(L, -1) == LUA_TSTRING) {
		datetime = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));

	if(strptime(datetime, format, &tm) == NULL) {
		luaL_error(L, "strptime is unable to parse \"%s\" as \"%s\" ", datetime, format);
	}

	int year = tm.tm_year+1900;
	int month = tm.tm_mon+1;
	int day = tm.tm_mday;
	int hour = tm.tm_hour;
	int minute = tm.tm_min;
	int second = tm.tm_sec;
	int weekday = tm.tm_wday;

	datefix(&year, &month, &day, &hour, &minute, &second, &weekday);

	lua_createtable(L, 0, 2);

	lua_pushstring(L, "year");
	lua_pushnumber(L, year);
	lua_settable(L, -3);

	lua_pushstring(L, "month");
	lua_pushnumber(L, month);
	lua_settable(L, -3);

	lua_pushstring(L, "day");
	lua_pushnumber(L, day);
	lua_settable(L, -3);

	lua_pushstring(L, "hour");
	lua_pushnumber(L, hour);
	lua_settable(L, -3);

	lua_pushstring(L, "min");
	lua_pushnumber(L, minute);
	lua_settable(L, -3);

	lua_pushstring(L, "sec");
	lua_pushnumber(L, second);
	lua_settable(L, -3);

	lua_pushstring(L, "weekday");
	lua_pushnumber(L, weekday);
	lua_settable(L, -3);

	assert(lua_gettop(L) == 1);

	return 1;
}