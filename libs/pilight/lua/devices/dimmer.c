/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string.h>
#include <assert.h>

#include "../../protocols/protocol.h"

#include "../../core/log.h"
#include "../config.h"
#include "dimmer.h"

static void *reason_control_device_free(void *param) {
	struct reason_control_device_t *data = param;
	if(data->state != NULL) {
		FREE(data->state);
	}
	if(data->values != NULL) {
		json_delete(data->values);
	}
	FREE(data->dev);
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
	values = NULL;
}

static int plua_config_device_dimmer_send(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config send requires 0 argument, %d given", lua_gettop(L));
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
		if(strcmp(value->key, "state") == 0) {
			if(value->value.type_ == JSON_STRING) {
				if((data1->state = STRDUP(value->value.string_)) == NULL) {
					OUT_OF_MEMORY
				}
				FREE(value->value.string_);
			}
		} else {
			if(value->value.type_ == JSON_STRING) {
				json_append_member(data1->values, value->key, json_mkstring(value->value.string_));
				FREE(value->value.string_);
			} else if(value->value.type_ == JSON_NUMBER) {
				json_append_member(data1->values, value->key, json_mknumber(value->value.number_, value->value.decimals_));
			}
		}
		FREE(value->key);
		FREE(value);
	}

	plua_gc_unreg(L, dev->values);
	if(dev->values != NULL) {
		dt_stack_free(dev->values, NULL);
	}

	eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);

	lua_pushboolean(L, 0);

	assert(lua_gettop(L) == 1);

	return 0;
}

static int plua_config_device_dimmer_get_state(lua_State *L) {
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

static int plua_config_device_dimmer_has_state(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct protocol_t *protocol = NULL;
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

	while(devices_select_protocol(ORIGIN_ACTION, dev->name, i++, &protocol) == 0) {
		struct options_t *opt = protocol->options;
		while(opt) {
			if(opt->conftype == DEVICES_STATE) {
				if(strcmp(opt->name, state) == 0) {
					lua_pushboolean(L, 1);

					assert(lua_gettop(L) == 1);

					return 1;
				}
			}
			opt = opt->next;
		}
	}

	lua_pushboolean(L, 0);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_config_device_dimmer_set_state(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct protocol_t *protocol = NULL;
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

	while(devices_select_protocol(ORIGIN_ACTION, dev->name, i++, &protocol) == 0) {
		struct options_t *opt = protocol->options;
		while(opt) {
			if(opt->conftype == DEVICES_STATE) {
				if(strcmp(opt->name, state) == 0) {
					match = 1;
					break;
				}
			}
			opt = opt->next;
		}
	}
	if(match == 0) {
		lua_pushboolean(L, 0);

		assert(lua_gettop(L) == 1);

		return 1;
	}

	struct plua_device_value_t *value = MALLOC(sizeof(struct plua_device_value_t));
	if(value == NULL) {
		OUT_OF_MEMORY
	}
	if((value->key = STRDUP("state")) == NULL) {
		OUT_OF_MEMORY
	}
	value->value.type_ = JSON_STRING;
	if((value->value.string_ = STRDUP((char *)state)) == NULL) {
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

static int plua_config_device_dimmer_get_dimlevel(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	double dimlevel = 0.0;
	int	decimals = 0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 0) {
		luaL_error(L, "config getDimlevel requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(devices_select_number_setting(ORIGIN_ACTION, dev->name, "dimlevel", &dimlevel, &decimals) != 0) {
		luaL_error(L, "could not retrieve current dimlevel of \"%s\"", dev->name);
	}

	lua_pushnumber(L, (int)dimlevel);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_config_device_dimmer_has_dimlevel(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct protocol_t *protocol = NULL;
	int dimlevel = 0, max = 0, min = 0, has_max = 0, has_min = 0;
	int i = 0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) != 1) {
		luaL_error(L, "config hasDimlevel requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TNUMBER),
		1, buf);

	dimlevel = (int)lua_tonumber(L, -1);
	lua_remove(L, -1);

	while(devices_select_protocol(ORIGIN_ACTION, dev->name, i++, &protocol) == 0) {
		struct options_t *opt = protocol->options;
		while(opt) {
			if(opt->conftype == DEVICES_SETTING) {
				if(strcmp(opt->name, "dimlevel-maximum") == 0) {
					has_max = 1;
					max = (intptr_t)opt->def;
				}
				if(strcmp(opt->name, "dimlevel-minimum") == 0) {
					has_min = 1;
					min = (intptr_t)opt->def;
				}
				if(has_max == 1 && has_min == 1) {
					break;
				}
			}
			opt = opt->next;
		}
	}

	if((
			(has_max == 1 && dimlevel <= max) || has_max == 0
		) && (
			(has_min == 1 && dimlevel >= min) || has_min == 0
		)) {
		lua_pushboolean(L, 1);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	lua_pushboolean(L, 0);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_config_device_dimmer_set_dimlevel(lua_State *L) {
	struct plua_device_t *dev = (void *)lua_topointer(L, lua_upvalueindex(1));

	struct protocol_t *protocol = NULL;
	int dimlevel = 0, max = 0, min = 0, has_max = 0, has_min = 0, i = 0;

	if(dev == NULL) {
		luaL_error(L, "internal error: device object not passed");
	}

	if(lua_gettop(L) > 1) {
		luaL_error(L, "config setState requires 1 argument, %d given", lua_gettop(L));
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TNUMBER),
		1, buf);

	dimlevel = (int)lua_tonumber(L, -1);
	lua_remove(L, -1);

	while(devices_select_protocol(ORIGIN_ACTION, dev->name, i++, &protocol) == 0) {
		struct options_t *opt = protocol->options;
		while(opt) {
			if(opt->conftype == DEVICES_SETTING) {
				if(strcmp(opt->name, "dimlevel-maximum") == 0) {
					has_max = 1;
					max = (intptr_t)opt->def;
				}
				if(strcmp(opt->name, "dimlevel-minimum") == 0) {
					has_min = 1;
					min = (intptr_t)opt->def;
				}
				if(has_max == 1 && has_min == 1) {
					break;
				}
			}
			opt = opt->next;
		}
	}
	if(!((
			(has_max == 1 && dimlevel <= max) || has_max == 0
		) && (
			(has_min == 1 && dimlevel >= min) || has_min == 0
		))) {
		lua_pushboolean(L, 0);
		assert(lua_gettop(L) == 1);
		return 1;
	}

	struct plua_device_value_t *value = MALLOC(sizeof(struct plua_device_value_t));
	if(value == NULL) {
		OUT_OF_MEMORY
	}
	if((value->key = STRDUP("dimlevel")) == NULL) {
		OUT_OF_MEMORY
	}
	value->value.type_ = JSON_NUMBER;
	value->value.number_ = dimlevel;
	value->value.decimals_ = 0;

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


int plua_config_device_dimmer(lua_State *L, struct plua_device_t *dev) {
	lua_pushstring(L, "getState");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_dimmer_get_state, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setState");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_dimmer_set_state, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "hasState");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_dimmer_has_state, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getDimlevel");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_dimmer_get_dimlevel, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setDimlevel");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_dimmer_set_dimlevel, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "hasDimlevel");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_dimmer_has_dimlevel, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "send");
	lua_pushlightuserdata(L, dev);
	lua_pushcclosure(L, plua_config_device_dimmer_send, 1);
	lua_settable(L, -3);

	return 1;
}