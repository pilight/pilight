/*
  Copyright (C) CurlyMo

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

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../protocols/protocol.h"
#include "../storage/storage.h"

#include "lua.h"
#include "cast.h"
#include "datetime.h"
#include "common.h"
#include "config.h"
#include "async.h"
#include "network.h"
#include "io.h"
#include "log.h"
#include "table.h"

#include "wiringx.h"

static const struct {
	char *name;
	union {
		char *string_;
		int number_;
	} value;
	int type;
} pilight_defaults[] = {
	{ "PILIGHT_VERSION", { .string_ = PILIGHT_VERSION }, LUA_TSTRING },
	{ "PILIGHT_V", { .number_ = PILIGHT_V }, LUA_TNUMBER },
	{ "WEBSERVER_HTTP_PORT", { .number_ = WEBSERVER_HTTP_PORT }, LUA_TNUMBER },
	{ "WEBSERVER_HTTPS_PORT", { .number_ = WEBSERVER_HTTPS_PORT }, LUA_TNUMBER },
	{ "WEBSERVER_ENABLE", { .number_ = WEBSERVER_ENABLE }, LUA_TNUMBER },
	{ "WEBSERVER_HTTPS", { .number_ = WEBSERVER_HTTPS }, LUA_TBOOLEAN },
	{ "FIRMWARE_GPIO_SCK", { .number_ = FIRMWARE_GPIO_SCK }, LUA_TNUMBER },
	{ "FIRMWARE_GPIO_RESET", { .number_ = FIRMWARE_GPIO_RESET }, LUA_TNUMBER },
	{ "FIRMWARE_GPIO_MOSI", { .number_ = FIRMWARE_GPIO_MOSI }, LUA_TNUMBER },
	{ "FIRMWARE_GPIO_MISO", { .number_ = FIRMWARE_GPIO_MISO }, LUA_TNUMBER },
};

static const struct {
	char *name;
	int number;
} pilight_reasons[] = {
	{ "REASON_SEND_CODE", REASON_SEND_CODE },
	{ "REASON_CONTROL_DEVICE", REASON_CONTROL_DEVICE },
	{ "REASON_CODE_SENT", REASON_CODE_SENT },
	{ "REASON_CODE_SEND_FAIL", REASON_CODE_SEND_FAIL },
	{ "REASON_CODE_SEND_SUCCESS", REASON_CODE_SEND_SUCCESS },
	{ "REASON_CODE_RECEIVED", REASON_CODE_RECEIVED },
	{ "REASON_RECEIVED_PULSETRAIN", REASON_RECEIVED_PULSETRAIN },
	{ "REASON_RECEIVED_OOK", REASON_RECEIVED_OOK },
	{ "REASON_BROADCAST", REASON_BROADCAST },
	{ "REASON_BROADCAST_CORE", REASON_BROADCAST_CORE },
	{ "REASON_FORWARD", REASON_FORWARD },
	{ "REASON_CONFIG_UPDATE", REASON_CONFIG_UPDATE },
	{ "REASON_CONFIG_UPDATED", REASON_CONFIG_UPDATED },
	{ "REASON_SOCKET_RECEIVED", REASON_SOCKET_RECEIVED },
	{ "REASON_SOCKET_DISCONNECTED", REASON_SOCKET_DISCONNECTED },
	{ "REASON_SOCKET_CONNECTED", REASON_SOCKET_CONNECTED },
	{ "REASON_SOCKET_SEND", REASON_SOCKET_SEND },
	{ "REASON_SSDP_RECEIVED", REASON_SSDP_RECEIVED },
	{ "REASON_SSDP_RECEIVED_FREE", REASON_SSDP_RECEIVED_FREE },
	{ "REASON_SSDP_DISCONNECTED", REASON_SSDP_DISCONNECTED },
	{ "REASON_SSDP_CONNECTED", REASON_SSDP_CONNECTED },
	{ "REASON_WEBSERVER_CONNECTED", REASON_WEBSERVER_CONNECTED },
	{ "REASON_DEVICE_ADDED", REASON_DEVICE_ADDED },
	{ "REASON_DEVICE_ADAPT", REASON_DEVICE_ADAPT },
	{ "REASON_ADHOC_MODE", REASON_ADHOC_MODE },
	{ "REASON_ADHOC_CONNECTED", REASON_ADHOC_CONNECTED },
	{ "REASON_ADHOC_CONFIG_RECEIVED", REASON_ADHOC_CONFIG_RECEIVED },
	{ "REASON_ADHOC_DATA_RECEIVED", REASON_ADHOC_DATA_RECEIVED },
	{ "REASON_ADHOC_UPDATE_RECEIVED", REASON_ADHOC_UPDATE_RECEIVED },
	{ "REASON_ADHOC_DISCONNECTED", REASON_ADHOC_DISCONNECTED },
	{ "REASON_SEND_BEGIN", REASON_SEND_BEGIN },
	{ "REASON_SEND_END", REASON_SEND_END },
	{ "REASON_ARP_FOUND_DEVICE", REASON_ARP_FOUND_DEVICE },
	{ "REASON_ARP_LOST_DEVICE", REASON_ARP_LOST_DEVICE },
	{ "REASON_ARP_CHANGED_DEVICE", REASON_ARP_CHANGED_DEVICE },
	{ "REASON_LOG", REASON_LOG },
	{ "REASON_END", REASON_END },
};

static const struct {
	char *name;
	int number;
} pilight_hardware[] = {
	{ "RF433", RF433 },
	{ "RF868", RF868 },
	{ "NONE", RFNONE }
};

static const struct {
	char *name;
	int number;
} pilight_log[] = {
	{ "LOG_EMERG", LOG_EMERG },
	{ "LOG_ALERT", LOG_ALERT },
	{ "LOG_CRIT", LOG_CRIT },
	{ "LOG_ERR", LOG_ERR },
	{ "LOG_WARNING", LOG_WARNING },
	{ "LOG_NOTICE", LOG_NOTICE },
	{ "LOG_INFO", LOG_INFO },
	{ "LOG_DEBUG", LOG_DEBUG }
};

static const struct {
	const char *name;
	const luaL_Reg *libs;
} pilight_two_lib[] = {
	{ "cast", pilight_cast_lib },
	{ "common", pilight_common_lib },
	{ "datetime", pilight_datetime_lib },
	{ "async", pilight_async_lib },
	{ "network", pilight_network_lib },
	{ "io", pilight_io_lib },
	{ NULL, NULL }
};

static const luaL_Reg pilight_one_lib[] = {
	{ "config", plua_config },
	{ "table", plua_table },
	{ "log", plua_log },
	{NULL, NULL}
};

void plua_register_library(struct lua_State *L) {
	/*
	 * Register pilight library in lua
	 */
	lua_newtable(L);

	int i = 0, x = 0;

	while(pilight_two_lib[i].name != NULL) {
		x = 0;
		lua_pushstring(L, pilight_two_lib[i].name);
		lua_newtable(L);
		const struct luaL_Reg *libs = pilight_two_lib[i].libs;
		while(libs[x].name != NULL) {
			lua_pushstring(L, libs[x].name);
			lua_pushcfunction(L, libs[x].func);
			lua_settable(L, -3);
			x++;
		}
		lua_settable(L, -3);
		i++;
	}
	i = 0;
	while(pilight_one_lib[i].name != NULL) {
		lua_pushstring(L, pilight_one_lib[i].name);
		lua_pushcfunction(L, pilight_one_lib[i].func);
		lua_settable(L, -3);
		i++;
	}

	/*
	 * Defaults
	 */
	lua_pushstring(L, "default");
	lua_newtable(L);
	int len = sizeof(pilight_defaults)/sizeof(pilight_defaults[0]);
	for(i=0;i<len;i++) {
		lua_pushstring(L, pilight_defaults[i].name);
		if(pilight_defaults[i].type == LUA_TNUMBER) {
			lua_pushnumber(L, pilight_defaults[i].value.number_);
		} else if(pilight_defaults[i].type == LUA_TSTRING) {
			lua_pushstring(L, pilight_defaults[i].value.string_);
		} else if(pilight_defaults[i].type == LUA_TBOOLEAN) {
			lua_pushboolean(L, pilight_defaults[i].value.number_);
		}
		lua_settable(L, -3);
	}
	lua_settable(L, -3);

	/*
	 * Reason
	 */
	lua_pushstring(L, "reason");
	lua_newtable(L);
	len = sizeof(pilight_reasons)/sizeof(pilight_reasons[0]);
	for(i=0;i<len;i++) {
		lua_pushstring(L, &pilight_reasons[i].name[7]);
		lua_pushnumber(L, pilight_reasons[i].number+10000);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);

	lua_pushstring(L, "hardware");
	lua_newtable(L);
	len = sizeof(pilight_hardware)/sizeof(pilight_hardware[0]);
	for(i=0;i<len;i++) {
		lua_pushstring(L, pilight_hardware[i].name);
		lua_pushnumber(L, pilight_hardware[i].number);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);

	lua_setglobal(L, "pilight");

	len = sizeof(pilight_log)/sizeof(pilight_log[0]);
	for(i=0;i<len;i++) {
		lua_pushnumber(L, pilight_log[i].number);
		lua_setglobal(L, pilight_log[i].name);
	}

	/*
	 * Defaults
	 */
	lua_newtable(L);

	len = sizeof(wiringx_globals)/sizeof(wiringx_globals[0]);
	for(i=0;i<len;i++) {
		lua_pushstring(L, wiringx_globals[i].name);
		if(wiringx_globals[i].type == LUA_TNUMBER) {
			lua_pushnumber(L, wiringx_globals[i].value.number_);
		} else if(wiringx_globals[i].type == LUA_TSTRING) {
			lua_pushstring(L, wiringx_globals[i].value.string_);
		} else if(wiringx_globals[i].type == LUA_TBOOLEAN) {
			lua_pushboolean(L, wiringx_globals[i].value.number_);
		}
		lua_settable(L, -3);
	}

	i = 0;
	while(wiringx_lib[i].name != NULL) {
		lua_pushstring(L, wiringx_lib[i].name);
		lua_pushcfunction(L, wiringx_lib[i].func);
		lua_settable(L, -3);
		i++;
	}
	lua_setglobal(L, "wiringX");

	assert(lua_gettop(L) == 0);
}
