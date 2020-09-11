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

#include "../../core/log.h"
#include "../../core/mqtt.h"
#include "../lua.h"
#include "../table.h"
#include "../network.h"
#include "../../config/settings.h"

static void plua_network_mqtt_object(lua_State *L, struct lua_mqtt_t *mqtt);

#ifdef PILIGHT_UNITTEST
extern void plua_network_mqtt_gc(void *ptr);
#else
static void plua_network_mqtt_gc(void *ptr) {
	struct lua_mqtt_t *lua_mqtt = ptr;

	if(lua_mqtt != NULL) {
		int x = 0;
		if((x = atomic_dec(lua_mqtt->ref)) == 0) {
			plua_metatable_free(lua_mqtt->table);
			if(lua_mqtt->callback != NULL) {
				FREE(lua_mqtt->callback);
			}
			if(lua_mqtt->sigterm == 1) {
				lua_mqtt->gc = NULL;
			}
			plua_gc_unreg(NULL, lua_mqtt);
			FREE(lua_mqtt);
		}

		assert(x >= 0);
	}
}
#endif

static void plua_network_mqtt_global_gc(void *ptr) {
	struct lua_mqtt_t *lua_mqtt = ptr;
	lua_mqtt->sigterm = 1;
	plua_network_mqtt_gc(ptr);
}

static void get_options(char *func, lua_State *L, int *dub, int *qos, int *retain) {
	char buf[128] = { '\0' }, *p = buf;
	char *error = "table expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, 1)));

	luaL_argcheck(L,
		(lua_type(L, 1) == LUA_TTABLE),
		1, buf);

	lua_pushnil(L);
	while(lua_next(L, -2)) {
		lua_pushvalue(L, -2);
		switch(lua_type(L, -2)) {
			case LUA_TFUNCTION:
			case LUA_TSTRING:
			case LUA_TBOOLEAN:
			case LUA_TTABLE:
			default:
				pluaL_error(L, "mqtt.%s options table only takes numeric values", func);
			break;
			case LUA_TNUMBER: {
				int val = lua_tonumber(L, -2);
				switch(val) {
					case MQTT_RETAIN:
					if(retain == NULL) {
						pluaL_error(L, "mqtt.%s doesn't accept the MQTT_RETAIN options", func);
					}
					*retain = val;
					break;
					case MQTT_QOS1:
						if(qos == NULL) {
							pluaL_error(L, "mqtt.%s doesn't accept the MQTT_QOS1 or MQTT_QOS2 options", func);
						}
						if(*qos > 0) {
							pluaL_error(L, "mqtt.%s options table has multiple QOS options set", func);
						}
						*qos = 1;
					break;
					case MQTT_QOS2:
						if(qos == NULL) {
							pluaL_error(L, "mqtt.%s doesn't accept the MQTT_QOS1 or MQTT_QOS2 options", func);
						}
						if(*qos > 0) {
							pluaL_error(L, "mqtt.%s options table has multiple QOS options set", func);
						}
						*qos = 2;
					break;
					case MQTT_DUB:
						if(dub == NULL) {
							pluaL_error(L, "mqtt.%s doesn't accept the MQTT_DUB options", func);
						}
						*dub = 1;
					break;
				}
			}
			break;
		}
		lua_pop(L, 2);
	}
	lua_remove(L, 1);
}

static int plua_network_mqtt_set_userdata(lua_State *L) {
	struct lua_mqtt_t *mqtt = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_set_userdata(L, (struct plua_interface_t *)mqtt, "mqtt");
}

static int plua_network_mqtt_get_userdata(lua_State *L) {
	struct lua_mqtt_t *mqtt = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_get_userdata(L, (struct plua_interface_t *)mqtt, "mqtt");
}

static int plua_network_mqtt_set_callback(lua_State *L) {
	struct lua_mqtt_t *mqtt = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *func = NULL;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "mqtt.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(mqtt == NULL) {
		pluaL_error(L, "internal error: mqtt object not passed");
	}

	if(mqtt->module == NULL) {
		pluaL_error(L, "internal error: lua state not properly initialized");
	}

	char buf[128] = { '\0' }, *p = buf, name[255] = { '\0' };
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	func = (void *)lua_tostring(L, -1);
	lua_remove(L, -1);

	p = name;
	plua_namespace(mqtt->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		pluaL_error(L, "cannot find %s lua module", mqtt->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		pluaL_error(L, "%s: mqtt callback %s does not exist", mqtt->module->file, func);
	}
	lua_remove(L, -1);
	lua_remove(L, -1);

	if(mqtt->callback != NULL) {
		FREE(mqtt->callback);
	}
	if((mqtt->callback = STRDUP(func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_network_mqtt_subscribe(lua_State *L) {
	struct lua_mqtt_t *data = (void *)lua_topointer(L, lua_upvalueindex(1));

	char *topic = NULL;
	int args = lua_gettop(L), qos = -1;

	if(args > 2 || args < 0) {
		pluaL_error(L, "mqtt.subscribe requires 1 or 2 arguments, %d given", lua_gettop(L));
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TSTRING),
			1, buf);

		topic = (void *)lua_tostring(L, 1);
		lua_remove(L, 1);
	}
	if(args > 1) {
		get_options("subscribe", L, NULL, &qos, NULL);
	}
	if(qos == -1) { qos = 0; };

	mqtt_subscribe(data->client, topic, qos);

	return 0;
}

static int plua_network_mqtt_ping(lua_State *L) {
	struct lua_mqtt_t *data = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "mqtt.ping requires 0 arguments, %d given", lua_gettop(L));
	}

	mqtt_ping(data->client);

	return 0;
}

static int plua_network_mqtt_pubrec(lua_State *L) {
	struct lua_mqtt_t *data = (void *)lua_topointer(L, lua_upvalueindex(1));

	int args = lua_gettop(L), msgid = 0;
	if(args == 0) {
		pluaL_error(L, "mqtt.pubrec requires 1 arguments, %d given", lua_gettop(L));
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		msgid = lua_tonumber(L, 1);
		lua_remove(L, 1);
	}

	mqtt_pubrec(data->client, msgid);

	return 0;
}

static int plua_network_mqtt_pubrel(lua_State *L) {
	struct lua_mqtt_t *data = (void *)lua_topointer(L, lua_upvalueindex(1));

	int args = lua_gettop(L), msgid = 0;
	if(args == 0) {
		pluaL_error(L, "mqtt.pubrel requires 1 arguments, %d given", lua_gettop(L));
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		msgid = lua_tonumber(L, 1);
		lua_remove(L, 1);
	}

	mqtt_pubrel(data->client, msgid);

	return 0;
}

static int plua_network_mqtt_pubcomp(lua_State *L) {
	struct lua_mqtt_t *data = (void *)lua_topointer(L, lua_upvalueindex(1));

	int args = lua_gettop(L), msgid = 0;
	if(args == 0) {
		pluaL_error(L, "mqtt.pubcomp requires 1 arguments, %d given", lua_gettop(L));
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TNUMBER),
			1, buf);

		msgid = lua_tonumber(L, 1);
		lua_remove(L, 1);
	}

	mqtt_pubcomp(data->client, msgid);

	return 0;
}

static int plua_network_mqtt_publish(lua_State *L) {
	struct lua_mqtt_t *data = (void *)lua_topointer(L, lua_upvalueindex(1));

	char *topic = NULL, *message = NULL;
	int args = lua_gettop(L), qos = -1, dub = -1, retain = -1;
	if(args > 3 || args < 2) {
		pluaL_error(L, "mqtt.publish requires 2 or 3 arguments, %d given", lua_gettop(L));
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TSTRING),
			1, buf);

		topic = (void *)lua_tostring(L, 1);
		lua_remove(L, 1);
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TSTRING),
			1, buf);

		message = (void *)lua_tostring(L, 1);
		lua_remove(L, 1);
	}

	if(args > 2) {
		get_options("publish", L, &dub, &qos, &retain);
	}
	if(dub == -1) { dub = 0; }
	if(qos == -1) { qos = 0; }
	if(retain == -1) { retain = 0; }

	mqtt_publish(data->client, dub, qos, retain, topic, message);

	return 0;
}

static void mqtt_callback(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	struct lua_mqtt_t *data = userdata;

	if(data->sigterm == 1) {
		return;
	}

	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the http callback is called
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = data->module;

	logprintf(LOG_DEBUG, "lua mqtt on state #%d", state->idx);

	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		pluaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, data->callback);

	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		pluaL_error(state->L, "%s: mqtt callback %s does not exist", state->module->file, data->callback);
	}

	plua_network_mqtt_object(state->L, data);

	if(client != NULL) {
		data->client = client;
	}

	lua_newtable(state->L);

	if(pkt != NULL) {
		atomic_inc(data->ref);

		if(pkt->type == MQTT_SUBACK || pkt->type == MQTT_PUBLISH) {
			lua_pushstring(state->L, "qos");
			lua_pushnumber(state->L, pkt->qos);
			lua_settable(state->L, -3);
		}

		if(pkt->type == MQTT_PUBLISH) {
			lua_pushstring(state->L, "dub");
			lua_pushnumber(state->L, pkt->dub);
			lua_settable(state->L, -3);

			lua_pushstring(state->L, "retain");
			lua_pushnumber(state->L, pkt->retain);
			lua_settable(state->L, -3);
		}

		lua_pushstring(state->L, "type");
		lua_pushnumber(state->L, pkt->type);
		lua_settable(state->L, -3);

		switch(pkt->type) {
			case MQTT_CONNACK: {
			} break;
			case MQTT_PUBREC: {
				lua_pushstring(state->L, "msgid");
				lua_pushnumber(state->L, pkt->payload.pubrec.msgid);
				lua_settable(state->L, -3);
			} break;
			case MQTT_PUBREL: {
				lua_pushstring(state->L, "msgid");
				lua_pushnumber(state->L, pkt->payload.pubrel.msgid);
				lua_settable(state->L, -3);
			} break;
			case MQTT_PUBCOMP: {
				lua_pushstring(state->L, "msgid");
				lua_pushnumber(state->L, pkt->payload.pubcomp.msgid);
				lua_settable(state->L, -3);
			} break;
			case MQTT_SUBACK: {
				lua_pushstring(state->L, "msgid");
				lua_pushnumber(state->L, pkt->payload.suback.msgid);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "qos");
				lua_pushnumber(state->L, pkt->payload.suback.qos);
				lua_settable(state->L, -3);
			} break;
			case MQTT_PUBLISH: {
				if(pkt->qos > 0) {
					lua_pushstring(state->L, "msgid");
					lua_pushnumber(state->L, pkt->payload.publish.msgid);
					lua_settable(state->L, -3);
				}

				lua_pushstring(state->L, "topic");
				lua_pushstring(state->L, pkt->payload.publish.topic);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "message");
				lua_pushstring(state->L, pkt->payload.publish.message);
				lua_settable(state->L, -3);
			} break;
			case MQTT_DISCONNECTED:
			case MQTT_DISCONNECT: {
				lua_pushstring(state->L, "type");
				lua_pushnumber(state->L, MQTT_DISCONNECT);
				lua_settable(state->L, -3);
			} break;
		}
	} else {
		lua_pushstring(state->L, "type");
		lua_pushnumber(state->L, MQTT_DISCONNECT);
		lua_settable(state->L, -3);
	}

	plua_gc_reg(state->L, data, plua_network_mqtt_gc);

	assert(plua_check_stack(state->L, 4, PLUA_TTABLE, PLUA_TFUNCTION, PLUA_TTABLE, PLUA_TTABLE) == 0);
	if(plua_pcall(state->L, state->module->file, 2, 0) == -1) {
		plua_clear_state(state);
		return;
	}

	lua_remove(state->L, 1);
	assert(plua_check_stack(state->L, 0) == 0);
	plua_clear_state(state);
}

static int plua_network_mqtt_connect(lua_State *L) {
	struct lua_mqtt_t *mqtt = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *ip = "127.0.0.1", *willmsg = NULL, *willtopic = NULL;
	char suffix[5] = { 0 }, *id = NULL;
	int port = MQTT_PORT, args = lua_gettop(L), freeid = 0;

	struct lua_state_t *state1 = plua_get_free_state();
	config_setting_get_number(state1->L, "mqtt-port", 0, &port);
	assert(plua_check_stack(state1->L, 0) == 0);
	plua_clear_state(state1);

	if(args != 0 && args != 2 && args != 3 && args != 5) {
		pluaL_error(L, "mqtt.connect requires 0, 2, 3 or 5 arguments, %d given", lua_gettop(L));
	}

	struct lua_state_t *state = plua_get_current_state(L);

	if(mqtt->callback == NULL) {
		pluaL_error(L, "%s: no mqtt callback has been set", state->module->file);
	}

	if(args == 2 || args == 3 || args == 5) {
		{
			char buf[128] = { '\0' }, *p = buf;
			char *error = "string expected, got %s";

			sprintf(p, error, lua_typename(L, lua_type(L, 1)));

			luaL_argcheck(L,
				(lua_type(L, 1) == LUA_TSTRING),
				1, buf);

			ip = (void *)lua_tostring(L, 1);
			lua_remove(L, 1);
		}

		{
			char buf[128] = { '\0' }, *p = buf;
			char *error = "number expected, got %s";

			sprintf(p, error, lua_typename(L, lua_type(L, 1)));

			luaL_argcheck(L,
				(lua_type(L, 1) == LUA_TNUMBER),
				1, buf);

			port = lua_tonumber(L, 1);
			lua_remove(L, 1);
		}
	}

	if(args == 3 || args == 5) {
		{
			char buf[128] = { '\0' }, *p = buf;
			char *error = "string expected, got %s";

			sprintf(p, error, lua_typename(L, lua_type(L, 1)));

			luaL_argcheck(L,
				(lua_type(L, 1) == LUA_TSTRING),
				1, buf);

			id = (void *)lua_tostring(L, 1);
			lua_remove(L, 1);
		}
	}

	if(args == 5) {
		{
			char buf[128] = { '\0' }, *p = buf;
			char *error = "string expected, got %s";

			sprintf(p, error, lua_typename(L, lua_type(L, 1)));

			luaL_argcheck(L,
				(lua_type(L, 1) == LUA_TSTRING),
				1, buf);

			willtopic = (void *)lua_tostring(L, 1);
			lua_remove(L, 1);
		}

		{
			char buf[128] = { '\0' }, *p = buf;
			char *error = "string expected, got %s";

			sprintf(p, error, lua_typename(L, lua_type(L, 1)));

			luaL_argcheck(L,
				(lua_type(L, 1) == LUA_TSTRING),
				1, buf);

			willmsg = (void *)lua_tostring(L, 1);
			lua_remove(L, 1);
		}
	}

	alpha_random(suffix, 4);
	if(id == NULL) {
		if((id = MALLOC(strlen("pilight-")+5)) == NULL) {
			OUT_OF_MEMORY
		}
		sprintf(id, "pilight-%s", suffix);
		freeid = 1;
	}

	if(strcmp(id, "pilight") == 0) {
		pluaL_error(L, "mqtt.connect id cannot be set to 'pilight' which is reserved by pilight itself");
	}

	if(willtopic != NULL && willmsg != NULL) {
		if(strstr("+", willtopic) != NULL || strstr("#", willtopic) != NULL) {
			pluaL_error(L, "mqtt.connect last will topic cannot contain wildcards");
		}
	}

	mqtt_client(ip, port, id, willtopic, willmsg, mqtt_callback, mqtt);
	atomic_inc(mqtt->ref);

	if(freeid == 1) {
		FREE(id);
	}

	return 0;
}

static int plua_network_mqtt_get_callback(lua_State *L) {
	struct lua_mqtt_t *mqtt = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "mqtt.getCallback requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mqtt == NULL) {
		pluaL_error(L, "internal error: mqtt object not passed");
	}

	if(mqtt->callback != NULL) {
		lua_pushstring(L, mqtt->callback);
	} else {
		lua_pushnil(L);
	}

	assert(plua_check_stack(L, 1, PLUA_TSTRING | PLUA_TNIL) == 0);

	return 1;
}

static int plua_network_mqtt_call(lua_State *L) {
	struct lua_mqtt_t *mqtt = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(mqtt == NULL) {
		logprintf(LOG_ERR, "internal error: mqtt object not passed");
		return 0;
	}

	lua_pushlightuserdata(L, mqtt);

	return 1;
}

static int plua_network_mqtt_valid_topic(lua_State *L) {
	int args = lua_gettop(L);
	if(args == 0) {
		pluaL_error(L, "mqtt.validTopic requires 1 arguments, %d given", lua_gettop(L));
	}

	const char *topic = NULL;
	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, 1)));

		luaL_argcheck(L,
			(lua_type(L, 1) == LUA_TSTRING),
			1, buf);

		topic = lua_tostring(L, 1);
		lua_remove(L, 1);
	}

	lua_pushboolean(L, mosquitto_sub_topic_check(topic) == 0);

	return 1;
}

static void plua_network_mqtt_object(lua_State *L, struct lua_mqtt_t *mqtt) {
	lua_newtable(L);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getCallback");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_get_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "validTopic");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_valid_topic, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "connect");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_connect, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "subscribe");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_subscribe, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "publish");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_publish, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "pubrec");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_pubrec, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "pubrel");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_pubrel, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "pubcomp");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_pubcomp, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "ping");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_ping, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_get_userdata, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_set_userdata, 1);
	lua_settable(L, -3);

	lua_newtable(L);

	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, mqtt);
	lua_pushcclosure(L, plua_network_mqtt_call, 1);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);
}

int plua_network_mqtt(struct lua_State *L) {
	struct lua_mqtt_t *lua_mqtt = NULL;
	if(lua_gettop(L) < 0 || lua_gettop(L) > 1) {
		pluaL_error(L, "mqtt requires 0 or 1 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	if(lua_gettop(L) == 1) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "lightuserdata expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TLIGHTUSERDATA),
			1, buf);

		lua_mqtt = lua_touserdata(L, -1);
		lua_remove(L, -1);

		if(lua_mqtt->type != PLUA_MQTT) {
			luaL_error(L, "mqtt requires a mqtt object but %s object passed", plua_interface_to_string(lua_mqtt->type));
		}
		atomic_inc(lua_mqtt->ref);
	} else {
		lua_mqtt = MALLOC(sizeof(struct lua_mqtt_t));
		if(lua_mqtt == NULL) {
			OUT_OF_MEMORY
		}
		memset(lua_mqtt, '\0', sizeof(struct lua_mqtt_t));

		lua_mqtt->type = PLUA_MQTT;
		lua_mqtt->ref = 1;
		lua_mqtt->gc = plua_network_mqtt_gc;

		plua_metatable_init(&lua_mqtt->table);

		lua_mqtt->module = state->module;
		lua_mqtt->L = L;
		plua_gc_reg(NULL, lua_mqtt, plua_network_mqtt_global_gc);
	}
	plua_gc_reg(L, lua_mqtt, plua_network_mqtt_gc);

	plua_network_mqtt_object(L, lua_mqtt);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}
