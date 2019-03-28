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

#include "../../core/log.h"
#include "../../config/config.h"
#include "../table.h"
#include "../io.h"

typedef struct lua_dir_t {
	struct plua_metatable_t *table;
	struct plua_module_t *module;
	lua_State *L;

	char *dir;

} lua_dir_t;

void plua_io_dir_gc(void *ptr) {
	struct lua_dir_t *data = ptr;

	plua_metatable_free(data->table);

	if(data->dir != NULL) {
		FREE(data->dir);
	}

	FREE(data);
}

static int plua_io_dir_exists(struct lua_State *L) {
	struct lua_dir_t *dir = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "dir.exists requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(dir == NULL) {
		luaL_error(L, "internal error: dir object not passed");
	}

	if(dir->dir == NULL) {
		luaL_error(L, "dir.exists: dir has not been set");
	}

	struct stat s;
	int err = stat(dir->dir, &s);
	if(err == -1) {
    if(ENOENT == errno) {
      lua_pushboolean(L, 0);
    } else {
			logprintf(LOG_ERR, "dir.exists: error running stat (%d)", errno);
      lua_pushboolean(L, 0);
    }
	} else {
    if(S_ISDIR(s.st_mode)) {
			lua_pushboolean(L, 1);
    } else {
      lua_pushboolean(L, 0);
    }
	}

	assert(lua_gettop(L) == 1);

	return 1;
}


static int plua_io_dir_close(struct lua_State *L) {
	struct lua_dir_t *dir = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "dir.close requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(dir == NULL) {
		luaL_error(L, "internal error: dir object not passed");
	}

	if(dir->dir == NULL) {
		luaL_error(L, "dir.close: dir has not been set");
	}

	plua_gc_unreg(L, dir);

	plua_io_dir_gc(dir);

	lua_pushboolean(L, 1);
	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_io_dir_object(lua_State *L, struct lua_dir_t *dir) {
	lua_newtable(L);

	lua_pushstring(L, "exists");
	lua_pushlightuserdata(L, dir);
	lua_pushcclosure(L, plua_io_dir_exists, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "close");
	lua_pushlightuserdata(L, dir);
	lua_pushcclosure(L, plua_io_dir_close, 1);
	lua_settable(L, -3);
}

int plua_io_dir(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "dir requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	char *name = NULL;

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TSTRING),
			1, buf);

		name = (void *)lua_tostring(L, -1);
		lua_remove(L, -1);
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	struct lua_dir_t *lua_dir = MALLOC(sizeof(struct lua_dir_t));
	if(lua_dir == NULL) {
		OUT_OF_MEMORY
	}
	memset(lua_dir, '\0', sizeof(struct lua_dir_t));

	plua_metatable_init(&lua_dir->table);

	if((lua_dir->dir = STRDUP(name)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_dir->module = state->module;
	lua_dir->L = L;

	plua_gc_reg(L, lua_dir, plua_io_dir_gc);

	plua_io_dir_object(L, lua_dir);

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
