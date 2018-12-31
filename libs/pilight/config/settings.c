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
#include "settings.h"

int config_callback_get_number(char *module, char *key, int idx, int *ret) {
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
	lua_pushnumber(state->L, idx);

	if((x = plua_pcall(state->L, state->module->file, 2, 1)) == 0) {
		if(lua_type(state->L, -1) == LUA_TNUMBER) {
			*ret = (int)lua_tonumber(state->L, -1);

			lua_pop(state->L, 1);

			x = 0;
		} else {
			x = -1;
		}
		lua_pop(state->L, -1);
	}

	lua_pop(state->L, -1);

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return x;
}

int config_callback_get_string(char *module, char *key, int idx, char **ret) {
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
	lua_pushnumber(state->L, idx);

	if((x = plua_pcall(state->L, state->module->file, 2, 1)) == 0) {
		if(lua_type(state->L, -1) == LUA_TSTRING) {
			if((*ret = STRDUP((char *)lua_tostring(state->L, -1))) == NULL) {
				OUT_OF_MEMORY
			}

			lua_pop(state->L, 1);

			x = 0;
		} else {
			x = -1;
		}
		lua_pop(state->L, -1);
	}

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return x;
}

int config_callback_set_string(char *module, char *key, int idx, char *val) {
	struct lua_state_t *state = plua_get_module("storage", module);
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	plua_get_method(state->L, state->module->file, "set");

	lua_pushstring(state->L, key);
	lua_pushnumber(state->L, idx);
	lua_pushstring(state->L, val);

	if((x = plua_pcall(state->L, state->module->file, 3, 1)) == 0) {
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

int config_callback_set_number(char *module, char *key, int idx, int val) {
	struct lua_state_t *state = plua_get_module("storage", module);
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	plua_get_method(state->L, state->module->file, "set");

	lua_pushstring(state->L, key);
	lua_pushnumber(state->L, idx);
	lua_pushnumber(state->L, val);

	if((x = plua_pcall(state->L, state->module->file, 3, 1)) == 0) {
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

int config_setting_get_number(char *key, int idx, int *ret) {
	return config_callback_get_number("settings", key, idx, ret);
}

int config_setting_get_string(char *key, int idx, char **ret) {
	return config_callback_get_string("settings", key, idx, ret);
}

int config_setting_set_number(char *key, int idx, int val) {
	return config_callback_set_number("settings", key, idx, val);
}

int config_setting_set_string(char *key, int idx, char *val) {
	return config_callback_set_string("settings", key, idx, val);
}
