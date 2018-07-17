/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string.h>
#include <assert.h>

#include "../../core/log.h"
#include "../config.h"
#include "switch.h"

typedef struct screen_t {
	char *state;
} screen_t;

static void *reason_control_device_free(void *param) {
	struct reason_control_device_t *data = param;
	FREE(data->state);
	FREE(data->dev);
	FREE(data);
	return NULL;
}

static void plua_config_device_screen_gc(void *data) {
	struct screen_t *values = data;
	if(values != NULL) {
		if(values->state != NULL) {
			FREE(values->state);
		}
		FREE(values);
	}
	values = NULL;
}

static int plua_config_device_screen_send(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config send requires 0 arguments, %d given", lua_gettop(L));
	}

	struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
	if(data1 == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	if((data1->dev = STRDUP(dev->name)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	data1->state = NULL;
	data1->values = NULL;

	struct screen_t *obj = dev->data;
	if(obj != NULL) {
		if(obj->state != NULL) {
			if((data1->state = STRDUP(obj->state)) == NULL) {
				OUT_OF_MEMORY
			}
			FREE(obj->state);
		}
		FREE(obj);
	}

	plua_gc_unreg(L, dev->data);

	eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);

	lua_pushboolean(L, 0);

	assert(lua_gettop(L) == 1);

	return 0;
}

static int plua_config_device_screen_get_state(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *state = NULL;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config getState requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_string_setting(ORIGIN_ACTION, dev->name, "state", &state) == 0) {
		lua_pushstring(L, state);

		assert(lua_gettop(L) == 1);

		return 1;
	}

	lua_pushnil(L);

	assert(lua_gettop(L) == 1);

	return 0;
}

static int plua_config_device_screen_has_state(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	const char *state = NULL;
	int i = 0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 1) {
		luaL_error(L, "config hasState requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	state = lua_tostring(L, -1);
	lua_remove(L, -1);

	while(devices_is_state(ORIGIN_ACTION, dev->name, i++, (char *)state) == 0) {
		lua_pushboolean(L, 1);

		assert(lua_gettop(L) == 1);
		return 1;
	}

	lua_pushboolean(L, 0);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_config_device_screen_set_state(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	const char *state = NULL;
	int i = 0, match = 0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 1) {
		luaL_error(L, "config setState requires 1 argument, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	state = lua_tostring(L, -1);
	lua_remove(L, -1);

	while(devices_is_state(ORIGIN_ACTION, dev->name, i++, (char *)state) == 0) {
		match = 1;
		break;
	}

	if(match == 0) {
		lua_pushboolean(L, 0);

		assert(lua_gettop(L) == 1);

		return 1;
	}

	if(dev->data == NULL) {
		if((dev->data = MALLOC(sizeof(struct screen_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(dev->data, 0, sizeof(struct screen_t));
		plua_gc_reg(L, dev->data, plua_config_device_screen_gc);
	}

	struct screen_t *obj = dev->data;
	if(obj->state != NULL) {
		FREE(obj->state);
	}
	if((obj->state = STRDUP((char *)state)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_config_device_screen(lua_State *L, struct plua_device_t *dev) {
	lua_pushstring(L, "getState");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_screen_get_state, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setState");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_screen_set_state, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "hasState");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_screen_has_state, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "send");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_screen_send, 1);
	lua_settable(L, -3);

	return 1;
}