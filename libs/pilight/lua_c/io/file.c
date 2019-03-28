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

typedef struct lua_file_t {
	struct plua_metatable_t *table;
	struct plua_module_t *module;
	lua_State *L;

	char *file;
	char *mode;
	FILE *fp;

} lua_file_t;

void plua_io_file_gc(void *ptr) {
	struct lua_file_t *data = ptr;

	plua_metatable_free(data->table);

	if(data->fp != NULL) {
		if(fclose(data->fp) != 0) {
			logprintf(LOG_ERR, "file.gc: could not close file %s with fd (%p)", data->file, data->fp);
		}
		data->fp = NULL;
	}

	if(data->file != NULL) {
		FREE(data->file);
	}

	if(data->mode != NULL) {
		FREE(data->mode);
	}

	FREE(data);
}

static int plua_io_file_exists(struct lua_State *L) {
	struct lua_file_t *file = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "file.exists requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(file == NULL) {
		luaL_error(L, "internal error: file object not passed");
	}

	if(file->file == NULL) {
		luaL_error(L, "file.exists: file has not been opened");
	}

	if(file_exists(file->file) == 0) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_io_file_close(struct lua_State *L) {
	struct lua_file_t *file = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "file.close requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(file == NULL) {
		luaL_error(L, "internal error: file object not passed");
	}

	if(file->file == NULL) {
		luaL_error(L, "file.close: file has not been set");
	}

	if(file->fp == NULL) {
		luaL_error(L, "file.close: %s has not been opened", file->file);
	}

	plua_gc_unreg(L, file);
	if(fclose(file->fp) != 0) {
		logprintf(LOG_ERR, "file.close: could not close file %s with fd (%p)", file->file, file->fp);
		lua_pushboolean(L, 0);
	} else {
		lua_pushboolean(L, 0);
	}
	file->fp = NULL;

	plua_io_file_gc(file);

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_io_file_readline_iter(struct lua_State *L) {
	struct lua_file_t *file = NULL;
	struct timeval timeout;
	char *line = NULL;
	size_t len = 0;
	ssize_t read = 0;
	fd_set readset;

	memset(&readset, 0, sizeof(fd_set));

	lua_remove(L, -1);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		file = lua_touserdata(L, -1);
		lua_pop(L, 1);
	}

	int fd = fileno(file->fp);

retry:
	FD_SET(fd, &readset);

	memset(&timeout, 0, sizeof(struct timeval));

	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;

	int ret = select(fd+1, &readset, NULL, NULL, &timeout);

	if(ret >= 0 && errno == EINTR) {
		lua_pushnil(L);
		assert(lua_gettop(L) == 1);
		return 1;
	} else if((ret == -1 && errno == EINTR) || ret == 0) {
		goto retry;
	}

	read = getline(&line, &len, file->fp);
	if(read == -1) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, line);
	}
	free(line);

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_io_file_readline(struct lua_State *L) {
	struct lua_file_t *file = (void *)lua_topointer(L, lua_upvalueindex(1));
	char list[6][3] = { "r", "r+", "a", "a+" };

	if(lua_gettop(L) != 0) {
		luaL_error(L, "file.readline requires 0 arguments, %d given", lua_gettop(L));
	}

	if(file == NULL) {
		luaL_error(L, "internal error: file object not passed");
	}

	if(file->file == NULL) {
		luaL_error(L, "file.close: file has not been set");
	}

	if(file->fp == NULL) {
		luaL_error(L, "file.close: %s has not been opened", file->file);
	}

	int i = 0, match = 0;
	for(i=0;i<6;i++) {
		if(strcmp(file->mode, list[i]) == 0) {
			match = 1;
			break;
		}
	}
	if(match == 0) {
		luaL_error(L, "file.readline: %s has not been opened for reading", file->file);
	}

	lua_pushcfunction(L, plua_io_file_readline_iter);
	lua_pushlightuserdata(L, file);
	lua_pushnil(L);

	return 3;
}

static int plua_io_file_write(lua_State *L) {
	struct lua_file_t *file = (void *)lua_topointer(L, lua_upvalueindex(1));
	char list[6][3] = { "w", "w+", "a", "a+" };
	char *content = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "file.write requires 1 argument, %d given", lua_gettop(L));
	}

	if(file == NULL) {
		luaL_error(L, "internal error: file object not passed");
	}

	if(file->file == NULL) {
		luaL_error(L, "file.close: file has not been set");
	}

	if(file->fp == NULL) {
		luaL_error(L, "file.close: %s has not been opened", file->file);
	}

	int i = 0, match = 0;
	for(i=0;i<6;i++) {
		if(strcmp(file->mode, list[i]) == 0) {
			match = 1;
			break;
		}
	}
	if(match == 0) {
		luaL_error(L, "file.write: %s has not been opened for writing", file->file);
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	content = (void *)lua_tostring(L, -1);

	lua_remove(L, -1);

	if(fprintf(file->fp, "%s", content) == strlen(content)) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_io_file_read(lua_State *L) {
	struct lua_file_t *file = (void *)lua_topointer(L, lua_upvalueindex(1));
	char list[6][3] = { "r", "r+", "a", "a+" };
	char buffer[1024] = { '\0' }, *content = buffer;
	int size = 1024;

	if(lua_gettop(L) > 1 && lua_gettop(L) < 0) {
		luaL_error(L, "file.read requires 0 or 1 arguments, %d given", lua_gettop(L));
	}

	if(file == NULL) {
		luaL_error(L, "internal error: file object not passed");
	}

	if(file->file == NULL) {
		luaL_error(L, "file.close: file has not been set");
	}

	if(file->fp == NULL) {
		luaL_error(L, "file.close: %s has not been opened", file->file);
	}

	int i = 0, match = 0;
	for(i=0;i<6;i++) {
		if(strcmp(file->mode, list[i]) == 0) {
			match = 1;
			break;
		}
	}
	if(match == 0) {
		luaL_error(L, "file.read: %s has not been opened for reading", file->file);
	}

	if(lua_gettop(L) == 1) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TNUMBER),
			1, buf);

		size = lua_tonumber(L, -1);

		lua_remove(L, -1);

		if(size > 1024) {
			if((content = MALLOC(size)) == NULL) {
				OUT_OF_MEMORY
			}
		}
	}

	if(fread(content, 1, size, file->fp) > 0) {
		lua_pushstring(L, content);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	if(content != buffer) {
		FREE(content);
	}
	return 1;
}

static int plua_io_file_open(lua_State *L) {
	struct lua_file_t *file = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *mode = "r";
	char list[6][3] = { "r", "r+", "w", "w+", "a", "a+" };
	int nrargs = lua_gettop(L);

	if(nrargs > 1 || nrargs < 0) {
		luaL_error(L, "file.open requires 0 or 1 arguments, %d given", lua_gettop(L));
	}

	if(file == NULL) {
		luaL_error(L, "internal error: file object not passed");
	}

	if(nrargs == 1) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, nrargs)));

		luaL_argcheck(L,
			(lua_type(L, nrargs) == LUA_TSTRING),
			1, buf);

		mode = (void *)lua_tostring(L, nrargs);
		if(file->mode != NULL) {
			FREE(file->mode);
		}

		lua_remove(L, nrargs);
		nrargs--;
	}

	if((file->mode = STRDUP(mode)) == NULL) {
		OUT_OF_MEMORY
	}

	int i = 0, match = 0;
	for(i=0;i<6;i++) {
		if(strcmp(mode, list[i]) == 0) {
			match = 1;
			break;
		}
	}
	if(match == 0) {
		luaL_error(L, "file.open: %s is not a valid open mode", mode);
	}

	if((file->fp = fopen(file->file, file->mode)) == NULL) {
		luaL_error(L, "file.open: could not open file \"%s\" with mode \"%s\"", file->file, file->mode);
	} else {
		lua_pushboolean(L, 1);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_io_file_seek(struct lua_State *L) {
	struct lua_file_t *file = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *whence = "set";
	int nrargs = lua_gettop(L), offset = 0;

	if(nrargs > 2 || nrargs <= 0) {
		luaL_error(L, "file.seek requires 1 or 2 arguments, %d given", lua_gettop(L));
	}

	if(file == NULL) {
		luaL_error(L, "internal error: file object not passed");
	}

	if(file->file == NULL) {
		luaL_error(L, "file.close: file has not been set");
	}

	if(file->fp == NULL) {
		luaL_error(L, "file.close: %s has not been opened", file->file);
	}

	if(nrargs == 2) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, nrargs)));

		luaL_argcheck(L,
			(lua_type(L, nrargs) == LUA_TSTRING),
			1, buf);

		whence = (void *)lua_tostring(L, nrargs);

		lua_remove(L, nrargs);
		nrargs--;
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";
		sprintf(p, error, lua_typename(L, lua_type(L, nrargs)));

		if(lua_type(L, nrargs) == LUA_TNUMBER) {
			offset = lua_tonumber(L, nrargs);
			lua_remove(L, nrargs);
		}
	}

	int x = SEEK_SET;
	if(strcmp(whence, "set") == 0) {
		x = SEEK_SET;
	} else if(strcmp(whence, "cur") == 0) {
		x = SEEK_CUR;
	} else if(strcmp(whence, "end") == 0) {
		x = SEEK_END;
	} else {
		luaL_error(L, "file.seek: %s is not a valid seek mode (set, cur or end)", whence);
	}
	int ret = fseek(file->fp, offset, x);

	if(ret == -1) {
		lua_pushnil(L);
	} else {
		ret = ftell(file->fp);
		if(ret == -1) {
			lua_pushnil(L);
		} else {
			lua_pushnumber(L, ret);
		}
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_io_file_object(lua_State *L, struct lua_file_t *file) {
	lua_newtable(L);

	lua_pushstring(L, "open");
	lua_pushlightuserdata(L, file);
	lua_pushcclosure(L, plua_io_file_open, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "exists");
	lua_pushlightuserdata(L, file);
	lua_pushcclosure(L, plua_io_file_exists, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "read");
	lua_pushlightuserdata(L, file);
	lua_pushcclosure(L, plua_io_file_read, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "readline");
	lua_pushlightuserdata(L, file);
	lua_pushcclosure(L, plua_io_file_readline, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "seek");
	lua_pushlightuserdata(L, file);
	lua_pushcclosure(L, plua_io_file_seek, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "write");
	lua_pushlightuserdata(L, file);
	lua_pushcclosure(L, plua_io_file_write, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "close");
	lua_pushlightuserdata(L, file);
	lua_pushcclosure(L, plua_io_file_close, 1);
	lua_settable(L, -3);
}

int plua_io_file(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "file requires 1 argument, %d given", lua_gettop(L));
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

	struct lua_file_t *lua_file = MALLOC(sizeof(struct lua_file_t));
	if(lua_file == NULL) {
		OUT_OF_MEMORY
	}
	memset(lua_file, '\0', sizeof(struct lua_file_t));

	plua_metatable_init(&lua_file->table);

	if((lua_file->file = STRDUP(name)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_file->module = state->module;
	lua_file->L = L;

	plua_gc_reg(L, lua_file, plua_io_file_gc);

	plua_io_file_object(L, lua_file);

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
