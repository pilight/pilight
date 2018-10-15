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

#include "lua.h"
#include "../core/log.h"

#include <wiringx.h>

int plua_wiringx_setup(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "wiringX setup requires 1 argument, %d given", lua_gettop(L));
	}

#if !defined(__aarch64__) && !defined(__arm__) && !defined(__mips__) && !defined(PILIGHT_UNITTEST)
	lua_remove(L, -1);
	lua_pushboolean(L, 0);
#else

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TSTRING),
		1, buf);

	const char *platform = NULL;
	if(lua_type(L, -1) == LUA_TSTRING) {
		platform = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	if(wiringXSetup((char *)platform, logprintf1) == 0) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
#endif

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_wiringx_has_gpio(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "wiringX hasGPIO requires 1 argument, %d given", lua_gettop(L));
	}

#if !defined(__aarch64__) && !defined(__arm__) && !defined(__mips__) && !defined(PILIGHT_UNITTEST)
	lua_remove(L, -1);
	lua_pushboolean(L, 0);
#else

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";
	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TNUMBER),
		1, buf);

	int gpio = 0;
	if(lua_type(L, -1) == LUA_TNUMBER) {
		gpio = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	if(wiringXPlatform() == NULL) {
		lua_pushboolean(L, 0);
	} else if(wiringXValidGPIO(gpio) == 0) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
#endif

	assert(lua_gettop(L) == 1);

	return 1;
}
