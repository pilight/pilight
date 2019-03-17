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

#include "../libs/pilight/core/log.h"
#include "log.h"

int plua_log(struct lua_State *L) {
	if(lua_gettop(L) != 2) {
		luaL_error(L, "log requires 2 arguments, %d given", lua_gettop(L));
	}

	int loglevel = 0, line = -1;
	const char *msg = NULL, *file = NULL;

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, 1) == LUA_TNUMBER) {
			loglevel = lua_tonumber(L, 1);
			lua_remove(L, 1);
		}
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TSTRING),
			1, buf);

		if(lua_type(L, 1) == LUA_TSTRING) {
			msg = lua_tostring(L, 1);
			lua_remove(L, 1);
		}
	}

	lua_Debug ar;
	if(lua_getstack(L, 1, &ar)) {
		lua_getinfo(L, "Sl", &ar);
		if(ar.currentline > 0) {  /* is there info? */
			file = ar.short_src;
			line = ar.currentline;
		}
	}

	if(loglevel < 0 || loglevel > LOG_DEBUG) {
		luaL_error(L, "%d is an invalid loglevel", loglevel);
	}

	if(line == -1 || file == NULL) {
		logprintf(loglevel, "%s", msg);
	} else {
		_logprintf(loglevel, (char *)file, line, msg);
	}

	assert(lua_gettop(L) == 0);

	return 0;
}