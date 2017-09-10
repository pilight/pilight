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
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/strptime.h"
#include "../core/datetime.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../protocols/protocol.h"
#include "../storage/storage.h"

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501
/*
** Adapted from Lua 5.2.0
*/
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup+1, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    lua_pushstring(L, l->name);
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -(nup+1));
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_settable(L, -(nup + 3));
  }
  lua_pop(L, nup);  /* remove upvalues */
}
#endif

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

static void datetime2table(struct lua_State *L, char *device) {
	char *setting = NULL;
	struct varcont_t val;
	int i = 0;
	struct tm tm;
	while(devices_select_settings(ORIGIN_RULE, device, i++, &setting, &val) == 0) {
		if(strcmp(setting, "year") == 0) {
			tm.tm_year = val.number_-1900;
		}
		if(strcmp(setting, "month") == 0) {
			tm.tm_mon = val.number_-1;
		}
		if(strcmp(setting, "day") == 0) {
			tm.tm_mday = val.number_;
		}
		if(strcmp(setting, "hour") == 0) {
			tm.tm_hour = val.number_;
		}
		if(strcmp(setting, "minute") == 0) {
			tm.tm_min = val.number_;
		}
		if(strcmp(setting, "second") == 0) {
			tm.tm_sec = val.number_;
		}
		if(strcmp(setting, "weekday") == 0) {
			tm.tm_wday = val.number_-1;
		}
		if(strcmp(setting, "dst") == 0) {
			tm.tm_isdst = val.number_;
		}
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

	lua_pushstring(L, "type");
	lua_pushnumber(L, DATETIME);
	lua_settable(L, -3);

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
}

static int plua_getdevice(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "pilight.plua_getdevice requires 1 arguments, %d given", lua_gettop(L));
		return 0;
	}
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_isstring(L, 1)),
		1, buf);

	const char *device = NULL;
	if(lua_type(L, -1) == LUA_TSTRING) {
		device = lua_tostring(L, -1);
		lua_pop(L, 1);
	}
	char *d = (char *)device;

	if(devices_select(ORIGIN_RULE, d, NULL) == 0) {
		struct protocol_t *protocol = NULL;
		if(devices_select_protocol(ORIGIN_RULE, d, 0, &protocol) == 0) {
			if(protocol->devtype == DATETIME) {
				datetime2table(L, d);
			} else {
				luaL_error(L, "device \"%s\" is not a datetime protocol", d);
				return 0;
			}
		}
	} else {
		// luaL_error(L, "device \"%s\" does not exist", d);
		return 0;
	}

	return 1;
}

static int plua_strptime(struct lua_State *L) {
	if(lua_gettop(L) != 2) {
		luaL_error(L, "pilight.strptime requires 2 arguments, %d given", lua_gettop(L));
		return 0;
	}
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_isstring(L, 1)),
		1, buf);

	sprintf(p, error, lua_typename(L, lua_type(L, 2)));
	luaL_argcheck(L,
		(lua_isstring(L, 2)),
		2, buf);

	const char *datetime = NULL;
	const char *format = NULL;
	if(lua_type(L, -1) == LUA_TSTRING) {
		datetime = lua_tostring(L, -1);
		lua_pop(L, 1);
	}
	if(lua_type(L, -1) == LUA_TSTRING) {
		format = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	if(strptime(datetime, format, &tm) == NULL) {
		luaL_error(L, "pilight.strptime is unable to parse \"%s\" as \"%s\" ", datetime, format);
		return 0;
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

	return 1;
}

static int plua_random(struct lua_State *L) {
	if(lua_gettop(L) != 2) {
		luaL_error(L, "pilight.random requires 2 arguments, %d given", lua_gettop(L));
		return 0;
	}
	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_isnumber(L, 1)),
		1, buf);

	sprintf(p, error, lua_typename(L, lua_type(L, 2)));
	luaL_argcheck(L,
		(lua_isnumber(L, 2)),
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

	return 1;
}

static const luaL_Reg pilightlib[] = {
  {"toboolean", plua_toboolean},
  {"tonumber", plua_tonumber},
  {"tostring", plua_tostring},
  {"strptime", plua_strptime},
  {"getdevice", plua_getdevice},
  {"random", plua_random},
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