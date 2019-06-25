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
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../lua_c/lua.h"
#include "../lua_c/table.h"

#include "config.h"
#include "registry.h"

int config_hardware_get_type(lua_State *L, char *module) {
	struct lua_state_t *state = plua_get_module(L, "hardware", "433nano");
	int out = -1;
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	plua_get_method(state->L, state->module->file, "implements");

	assert(plua_check_stack(state->L, 2, PLUA_TTABLE, PLUA_TFUNCTION) == 0);
	if((x = plua_pcall(state->L, state->module->file, 0, 1)) == 0) {
		if(lua_type(state->L, -1) != LUA_TNUMBER && lua_type(state->L, -1) != LUA_TNIL) {
			logprintf(LOG_ERR, "%s: the implements function returned %s, string or nil expected", state->module->file, lua_typename(state->L, lua_type(state->L, -1)));
			return -1;
		}

		if(lua_type(state->L, -1) == LUA_TNUMBER) {
			out = (int)lua_tonumber(state->L, -1);
		} else if(lua_type(state->L, -1) == LUA_TNIL) {
			out = -1;
		}

		lua_pop(state->L, 1);
		lua_pop(state->L, -1);
	}

	assert(lua_gettop(state->L) == 0);

	return out;
}

int config_hardware_run(void) {
	struct lua_state_t *state = NULL;
	char name[512] = { '\0' }, *p = name;
	struct varcont_t val;
	int match = 0;

	val.type_ = LUA_TTABLE;

	if((state = plua_get_free_state()) == NULL) {
		return -1;
	}

	struct plua_metatable_t *table = config_get_metatable();
	struct plua_module_t *tmp = plua_get_modules(), *oldmod = NULL;
	while(tmp) {
		memset(name, '\0', 512);
		sprintf(p, "hardware.%s", tmp->name);

		if(plua_metatable_get(table, p, &val) == LUA_TTABLE) {
			memset(name, '\0', 512);
			sprintf(p, "hardware.%s", tmp->name);

			lua_getglobal(state->L, name);
			if(lua_type(state->L, -1) == LUA_TNIL) {
				lua_pop(state->L, 1);
				tmp = tmp->next;
				continue;
			}

			oldmod = state->module;
			state->module = tmp;

			lua_getfield(state->L, -1, "run");
			if(lua_type(state->L, -1) != LUA_TFUNCTION) {
				logprintf(LOG_ERR, "%s module misses the required run function", tmp->name);
				state->module = oldmod;
				tmp = tmp->next;
				continue;
			}

			assert(plua_check_stack(state->L, 2, PLUA_TTABLE, PLUA_TFUNCTION) == 0);
			if(lua_pcall(state->L, 0, 0, 0) == LUA_ERRRUN) {
				logprintf(LOG_ERR, "%s", lua_tostring(state->L,  -1));
				state->module = oldmod;
				lua_pop(state->L, 1);
				tmp = tmp->next;
				continue;
			}

			match = 1;
			lua_pop(state->L, 1);
			state->module = oldmod;
		}

		tmp = tmp->next;
	}

	lua_pop(state->L, -1);

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	if(match == 1) {
		return 0;
	} else {
		return -1;
	}
}