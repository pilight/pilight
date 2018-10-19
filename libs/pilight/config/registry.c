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
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../lua_c/lua.h"

#include "config.h"
#include "registry.h"

static int config_callback_get(char *module, char *key, struct varcont_t *ret) {
	struct lua_state_t *state = plua_get_module("storage", module);
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	if(plua_get_method(state->L, state->module->file, "get") == -1) {
		return -1;
	}

	if(key == NULL) {
		logprintf(LOG_ERR, "%s key cannot be NULL", __FUNCTION__);
		return -1;
	}

	if(ret == NULL) {
		logprintf(LOG_ERR, "%s return value cannot be NULL", __FUNCTION__);
		return -1;
	}

	lua_pushstring(state->L, key);

	if((x = plua_pcall(state->L, state->module->file, 1, 1)) == 0) {
		if(lua_type(state->L, -1) == LUA_TNUMBER) {
			(*ret).number_ = lua_tonumber(state->L, -1);
			(*ret).type_ = LUA_TNUMBER;

			lua_pop(state->L, 1);

			x = 0;
		} else if(lua_type(state->L, -1) == LUA_TSTRING) {
			if(((*ret).string_ = STRDUP((char *)lua_tostring(state->L, -1))) == NULL) {
				OUT_OF_MEMORY
			}
			(*ret).type_ = LUA_TSTRING;

			lua_pop(state->L, 1);

			x = 0;
		} else if(lua_type(state->L, -1) == LUA_TBOOLEAN) {
			(*ret).bool_ = (int)lua_toboolean(state->L, -1);
			(*ret).type_ = LUA_TBOOLEAN;

			lua_pop(state->L, 1);

			x = 0;
		} else {
			x = 1;
		}
		lua_pop(state->L, -1);
	}

	lua_pop(state->L, -1);

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return x;
}

static int config_callback_set_string(char *module, char *key, char *val) {
	struct lua_state_t *state = plua_get_module("storage", module);
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	plua_get_method(state->L, state->module->file, "set");

	lua_pushstring(state->L, key);
	if(val != NULL) {
		lua_pushstring(state->L, val);
	} else {
		lua_pushnil(state->L);
	}

	if((x = plua_pcall(state->L, state->module->file, 2, 1)) == 0) {
		if(lua_type(state->L, -1) == LUA_TNUMBER) {
			x = lua_tonumber(state->L, -1);
			lua_pop(state->L, 1);
		} else {
			x = -1;
		}
		lua_pop(state->L, -1);
	}

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return x;
}

static int config_callback_set_number(char *module, char *key, double val) {
	struct lua_state_t *state = plua_get_module("storage", module);
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	plua_get_method(state->L, state->module->file, "set");

	lua_pushstring(state->L, key);
	lua_pushnumber(state->L, val);

	if((x = plua_pcall(state->L, state->module->file, 2, 1)) == 0) {
		if(lua_type(state->L, -1) == LUA_TNUMBER) {
			x = lua_tonumber(state->L, -1);
			lua_pop(state->L, 1);
		} else {
			x = -1;
		}
		lua_pop(state->L, -1);
	}

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return x;
}

static int config_callback_set_boolean(char *module, char *key, int val) {
	struct lua_state_t *state = plua_get_module("storage", module);
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	plua_get_method(state->L, state->module->file, "set");

	lua_pushstring(state->L, key);
	lua_pushboolean(state->L, val);

	if((x = plua_pcall(state->L, state->module->file, 2, 1)) == 0) {
		if(lua_type(state->L, -1) == LUA_TNUMBER) {
			x = lua_tonumber(state->L, -1);
			lua_pop(state->L, 1);
		} else {
			x = -1;
		}
		lua_pop(state->L, -1);
	}

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return x;
}

int config_registry_get(char *key, struct varcont_t *ret) {
	return config_callback_get("registry", key, ret);
}

int config_registry_set_number(char *key, double val) {
	return config_callback_set_number("registry", key, val);
}

int config_registry_set_boolean(char *key, int val) {
	return config_callback_set_boolean("registry", key, val);
}

int config_registry_set_string(char *key, char *val) {
	return config_callback_set_string("registry", key, val);
}

int config_registry_set_null(char *key) {
	return config_callback_set_string("registry", key, NULL);
}