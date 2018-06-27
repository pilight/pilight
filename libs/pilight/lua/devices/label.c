/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string.h>
#include <assert.h>

#ifndef PILIGHT_REWRITE
#include "../../config/devices.h"
#endif

#include "../../protocols/protocol.h"

#include "../../core/log.h"
#include "../config.h"
#include "label.h"

static void *reason_control_device_free(void *param) {
	struct reason_control_device_t *data = param;
	FREE(data->dev);
	if(data->state != NULL) {
		FREE(data->state);
	}
	if(data->values != NULL) {
		json_delete(data->values);
	}
	FREE(data);
	return NULL;
}

static void plua_config_label_gc(void *data) {
	struct stack_dt *values = data;
	struct plua_device_value_t *value = NULL;

	while((value = dt_stack_pop(values, 0)) != NULL) {
		if(value->value.type_ == JSON_STRING) {
			FREE(value->value.string_);
		}
		FREE(value->key);
		FREE(value);
	}

	dt_stack_free(values, NULL);
}

static int plua_config_device_label_send(lua_State *L) {
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
	data1->values = json_mkobject();

	struct plua_device_value_t *value = NULL;
	while((value = dt_stack_pop(dev->values, 0)) != NULL) {
		if(value->value.type_ == JSON_STRING) {
			json_append_member(data1->values, value->key, json_mkstring(value->value.string_));
			FREE(value->value.string_);
		} else if(value->value.type_ == JSON_NUMBER) {
			json_append_member(data1->values, value->key, json_mknumber(value->value.number_, value->value.decimals_));
		}
		FREE(value->key);
		FREE(value);
	}

	plua_gc_unreg(L, dev->values);
	if(dev->values != NULL) {
		dt_stack_free(dev->values, NULL);
	}

	eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_config_device_label_get_label(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *slabel = NULL;
	double dlabel = 0.0;
	int decimals = 0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config getLabel requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_string_setting(ORIGIN_ACTION, dev->name, "label", &slabel) == 0) {
		lua_pushstring(L, slabel);

		assert(lua_gettop(L) == 1);
		return 1;
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "label", &dlabel, &decimals) == 0) {
		lua_pushnumber(L, dlabel);

		assert(lua_gettop(L) == 1);
		return 1;
	}

	lua_pushboolean(L, 0);

	assert(lua_gettop(L) == 1);
	return 0;
}

static int plua_config_device_label_get_color(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *slabel = NULL;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config getColor requires 0 arguments, %d given", lua_gettop(L));
	}

	if(devices_select_string_setting(ORIGIN_ACTION, dev->name, "color", &slabel) == 0) {
		lua_pushstring(L, slabel);

		assert(lua_gettop(L) == 1);
		return 1;
	}

	lua_pushboolean(L, 0);

	assert(lua_gettop(L) == 1);

	return 0;
}

static int plua_config_device_label_set_label(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	const char *label = NULL;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 1) {
		luaL_error(L, "config setLabel requires 1 arguments, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	label = lua_tostring(L, -1);
	lua_remove(L, -1);

	struct plua_device_value_t *value = MALLOC(sizeof(struct plua_device_value_t));
	if(value == NULL) {
		OUT_OF_MEMORY
	}
	if((value->key = STRDUP("label")) == NULL) {
		OUT_OF_MEMORY
	}
	value->value.type_ = JSON_STRING;
	if((value->value.string_ = STRDUP((char *)label)) == NULL) {
		OUT_OF_MEMORY
	}

	if(dev->values == NULL) {
		if((dev->values = MALLOC(sizeof(struct stack_dt))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(dev->values, 0, sizeof(struct stack_dt));
		plua_gc_reg(L, dev->values, plua_config_label_gc);
	}
	dt_stack_push(dev->values, sizeof(struct plua_device_value_t), value);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_config_device_label_set_color(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	const char *color = NULL;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 1) {
		luaL_error(L, "config setColor requires 1 arguments, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	color = lua_tostring(L, -1);
	lua_remove(L, -1);

	struct plua_device_value_t *value = MALLOC(sizeof(struct plua_device_value_t));
	if(value == NULL) {
		OUT_OF_MEMORY
	}
	if((value->key = STRDUP("color")) == NULL) {
		OUT_OF_MEMORY
	}
	value->value.type_ = JSON_STRING;
	if((value->value.string_ = STRDUP((char *)color)) == NULL) {
		OUT_OF_MEMORY
	}
	if(dev->values == NULL) {
		if((dev->values = MALLOC(sizeof(struct stack_dt))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(dev->values, 0, sizeof(struct stack_dt));
		plua_gc_reg(L, dev->values, plua_config_label_gc);
	}
	dt_stack_push(dev->values, sizeof(struct plua_device_value_t), value);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

int plua_config_device_label(lua_State *L, struct plua_device_t *dev) {
	lua_pushstring(L, "getLabel");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_label_get_label, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setLabel");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_label_set_label, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getColor");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_label_get_color, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setColor");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_label_set_color, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "send");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_label_send, 1);
	lua_settable(L, -3);

	return 1;
}