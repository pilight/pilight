/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string.h>
#include <assert.h>

#include "../../core/log.h"
#include "../config.h"
#include "switch.h"

#include "../../core/datetime.h"

static int plua_config_device_datetime_get_year(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getYear requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "year", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_month(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getMonth requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "month", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_day(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getDay requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "day", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_hour(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getHour requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "hour", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_minute(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getMinute requires 1 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "minute", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_second(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getSecond requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "second", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_weekday(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getWeekday requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "weekday", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_dst(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double num = 0.0;
	int dec = 0.0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getDST requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "dst", &num, &dec) == 0) {
		lua_pushnumber(L, num);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	plua_ret_false(L);
	return 0;
}

static int plua_config_device_datetime_get_table(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *setting = NULL;
	struct varcont_t val;
	struct tm tm;
	int i = 0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 0) {
		luaL_error(L, "config getTable requires 0 arguments, %d given", lua_gettop(L));
	}

	while(devices_select_settings(0, dev->name, i++, &setting, &val) == 0) {
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

	lua_pushstring(L, "minute");
	lua_pushnumber(L, minute);
	lua_settable(L, -3);

	lua_pushstring(L, "second");
	lua_pushnumber(L, second);
	lua_settable(L, -3);

	lua_pushstring(L, "weekday");
	lua_pushnumber(L, weekday);
	lua_settable(L, -3);

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_config_device_datetime(lua_State *L, struct plua_device_t *dev) {
	lua_pushstring(L, "getYear");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_year, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getMonth");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_month, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getDay");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_day, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getHour");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_hour, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getMinute");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_minute, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getSecond");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_second, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getWeekday");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_weekday, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getDST");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_dst, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getTable");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_datetime_get_table, 1);
	lua_settable(L, -3);

	return 1;
}