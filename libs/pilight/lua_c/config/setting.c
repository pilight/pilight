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

#ifndef PILIGHT_REWRITE
#define ORIGIN_ACTION ACTION
#endif

#include "../../core/log.h"
#include "../../config/settings.h"
#include "../config.h"

int plua_config_setting(lua_State *L) {
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	const char *name = NULL;
	char *sval = NULL;
	int ival = 0;

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	if(lua_type(L, -1) == LUA_TSTRING) {
		name = lua_tostring(L, -1);
		lua_remove(L, -1);
	}

	if(config_setting_get_string((char *)name, 0, &sval) == 0) {
		lua_pushstring(L, sval);
		FREE(sval);
	} else if(config_setting_get_number((char *)name, 0, &ival) == 0) {
		lua_pushnumber(L, ival);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}