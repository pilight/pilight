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

static const struct {
	const char *name;
	const luaL_Reg *libs;
} pilight_lib[] = {
	{ "cast", pilight_cast_lib },
	{ "common", pilight_common_lib },
	{ "datetime", pilight_datetime_lib },
	{ "async", pilight_async_lib },
	{ "config", pilight_config_lib },
	{ "network", pilight_network_lib },
	{ NULL, NULL }
};

void plua_register_library(struct lua_State *L) {
	/*
	 * Register pilight library in lua
	 */
	lua_newtable(L);

	int i = 0, x = 0;

	while(pilight_lib[i].name != NULL) {
		x = 0;
		lua_pushstring(L, pilight_lib[i].name);
		lua_newtable(L);
		const struct luaL_Reg *libs = pilight_lib[i].libs;
		while(libs[x].name != NULL) {
			lua_pushstring(L, libs[x].name);
			lua_pushcfunction(L, libs[x].func);
			lua_settable(L, -3);
			x++;
		}
		lua_settable(L, -3);
		i++;
	}
	lua_setglobal(L, "pilight");

	assert(lua_gettop(L) == 0);
}