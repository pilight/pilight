/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/mqtt.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"
#include "../libs/pilight/lua_c/table.h"

#include "alltests.h"

static CuTest *gtc = NULL;
static uv_thread_t pth;
static uv_timer_t *timer_req = NULL;
static struct eventpool_listener_t *node = NULL;
static int test[2] = { 0 };

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static int call(struct lua_State *L, char *file, char *func) {
	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		return -1;
	}

	if(plua_pcall(L, file, 0, 0) == -1) {
		assert(plua_check_stack(L, 0) == 0);
		return -1;
	}

	lua_pop(L, 1);

	return 1;
}

static void *receiveAPI1(int reason, void *param, void *userdata) {
	struct plua_metatable_t *table = param;
	struct JsonNode *json = NULL;
	char *protocol = NULL;
	char *origin = NULL;

	plua_metatable_to_json(table, &json);

	if(json != NULL &&
		json_find_string(json, "protocol", &protocol) == 0 &&
		json_find_string(json, "origin", &origin) == 0) {
			char *foo = json_stringify(json, NULL);
			if(strcmp("{\"origin\":\"receiver\",\"message\":{\"topic\":\"tele/sonoff/POWER\",\"payload\":\"on\"},\"protocol\":\"mqtt\"}", foo) == 0) {
				test[0] = 1;
			} else if(strcmp("{\"origin\":\"receiver\",\"message\":{\"topic\":\"pilight/lamp/status\",\"payload\":\"{\\\"Time\\\":\\\"1970-01-01T02:32:27\\\",\\\"Uptime\\\":\\\"0T02:25:16\\\",\\\"Vcc\\\":3.238,\\\"SleepMode\\\":\\\"Dynamic\\\",\\\"Sleep\\\":50,\\\"LoadAvg\\\":19,\\\"POWER\\\":\\\"ON\\\",\\\"Wifi\\\":{\\\"AP\\\":1,\\\"SSId\\\":\\\"pilight\\\",\\\"BSSId\\\":\\\"AA:BB:CC:DD:EE:FF\\\",\\\"Channel\\\":2,\\\"RSSI\\\":82,\\\"LinkCount\\\":1,\\\"Downtime\\\":\\\"0T00:00:06\\\"}}\"},\"protocol\":\"mqtt\"}", foo) == 0) {
				test[1] = 1;
			}

			json_free(foo);
	}
	if(json != NULL) {
		json_delete(json);
	}
	return NULL;
}

static void stop(uv_work_t *req) {
	eventpool_callback_remove(node);
	mqtt_gc();
	uv_timer_stop(timer_req);
	uv_stop(uv_default_loop());
	plua_gc();
}

static void mqtt_callback2(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	if(pkt != NULL) {
		if(pkt->type == MQTT_CONNACK) {
			mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
			usleep(1000);
			mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
		}
	}
}

static void start_clients(void *param) {
	mqtt_client("127.0.0.1", 11883, "foo", NULL, NULL, mqtt_callback2, NULL);
}

void test_lua_hardware_mqtt_receive(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "lua_hardware_mqtt_receive.c");

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_hardware_mqtt_receive.c", "", &file);

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"mqtt-port\":11883\
		},\
		\"hardware\":{\"mqtt\":{}},\
		\"registry\":{}}";

	FILE *f = fopen("lua_hardware_mqtt.json", "w");
	fprintf(f, config, file);
	fclose(f);

	FREE(file);

	eventpool_init(EVENTPOOL_THREADED);
	node = eventpool_callback(REASON_RECEIVED_API+10000, receiveAPI1, NULL);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_mqtt.json", CONFIG_SETTINGS));
	plua_clear_state(state);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 1000);

	mqtt_server(11883);

	hardware_init();

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_mqtt.json", CONFIG_HARDWARE));
	plua_clear_state(state);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	sprintf(name, "hardware.%s", "mqtt");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("mqtt", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	plua_clear_state(state);

	uv_thread_create(&pth, start_clients, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	hardware_gc();
	storage_gc();
	plua_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 1, test[0]);
	CuAssertIntEquals(tc, 1, test[1]);
	CuAssertIntEquals(tc, 0, xfree());
}
