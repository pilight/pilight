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
static uv_timer_t *timer_req1 = NULL;
static uv_timer_t *timer_req2 = NULL;
static int running = 1;
static struct eventpool_listener_t *node = NULL;
static int test[10] = { 0 };

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
			if(strcmp("{\"origin\":\"receiver\",\"message\":{\"state\":\"off\",\"power\":0,\"energy\":138838,\"overtemperature\":0,\"temperature\":34.21,\"id\":\"A123B4\"},\"protocol\":\"shelly1pm\"}", foo) == 0) {
				test[0]++;
			} else if(strcmp("{\"origin\":\"receiver\",\"message\":{\"state\":\"off\",\"id\":\"A123D4\"},\"protocol\":\"shelly1\"}", foo) == 0) {
				test[1]++;
			} else if(strcmp("{\"origin\":\"receiver\",\"message\":{\"state\":\"on\",\"id\":\"A123D4\"},\"protocol\":\"shelly1\"}", foo) == 0) {
				test[3]++;
			} else if(strcmp("{\"origin\":\"receiver\",\"message\":{\"state\":\"off\",\"power\":0,\"energy\":4,\"id\":\"A123E4\"},\"protocol\":\"shellyplug-s\"}", foo) == 0) {
				test[4]++;
			} else if(strcmp("{\"origin\":\"receiver\",\"message\":{\"state\":\"off\",\"power\":0,\"energy\":4,\"id\":\"A123F4\"},\"protocol\":\"shellyplug\"}", foo) == 0) {
				test[5]++;
			}

			json_free(foo);
	}
	if(json != NULL) {
		json_delete(json);
	}
	return NULL;
}

static void stop(uv_work_t *req) {
	running = 0;
	eventpool_callback_remove(node);
	mqtt_gc();
	uv_timer_stop(timer_req);
	uv_stop(uv_default_loop());
	plua_gc();
}

static void ping(uv_timer_t *handle) {
	if(running == 1) {
		mqtt_ping(handle->data);
	}
}

static void mqtt_callback2(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	if(pkt != NULL) {
		if(pkt->type == MQTT_CONNACK) {
			if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			timer_req2->data = client;
			uv_timer_init(uv_default_loop(), timer_req2);
			uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 100, 100);

			mqtt_subscribe(client, "#", 0);
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/overtemperature", "0");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/temperature_f", "93.58");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/temperature", "34.21");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/relay/0/energy", "138838");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/relay/0/power", "0.00");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/input/0", "0");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/relay/0", "off");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1-A123D4/relay/0", "off");
			mqtt_publish(client, 0, 0, 0, "shellies/shelly1-A123D4/input/0", "off");
			mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-s-A123E4/relay/0", "off");
			mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-s-A123E4/relay/0/power", "0.00");
			mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-s-A123E4/relay/0/energy", "4");
			mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-A123F4/relay/0", "off");
			mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-A123F4/relay/0/power", "0.00");
			mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-A123F4/relay/0/energy", "4");
		}
		if(pkt->type == MQTT_PUBLISH) {
			if(strcmp("shellies/shelly1-A123D4/relay/0/command", pkt->payload.publish.topic) == 0 &&
				strcmp("on", pkt->payload.publish.message) == 0) {
				mqtt_publish(client, 0, 0, 0, "shellies/shelly1-A123D4/relay/0", "on");
				test[2]++;
			}
			if(strcmp("shellies/shelly1pm-A123B4/relay/0/command", pkt->payload.publish.topic) == 0 &&
				strcmp("on", pkt->payload.publish.message) == 0) {
				mqtt_publish(client, 0, 0, 0, "shellies/shelly1pm-A123B4/relay/0", "on");
				test[6]++;
			}
			if(strcmp("shellies/shellyplug-s-A123E4/relay/0/command", pkt->payload.publish.topic) == 0 &&
				strcmp("on", pkt->payload.publish.message) == 0) {
				mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-s-A123E4/relay/0", "on");
				test[7]++;
			}
			if(strcmp("shellies/shellyplug-A123F4/relay/0/command", pkt->payload.publish.topic) == 0 &&
				strcmp("on", pkt->payload.publish.message) == 0) {
				mqtt_publish(client, 0, 0, 0, "shellies/shellyplug-A123F4/relay/0", "on");
				test[8]++;
			}
		}
	}
}

static void start_clients(void *param) {
	mqtt_client("127.0.0.1", 11883, "foo", NULL, NULL, mqtt_callback2, NULL);
}

static void *reason_send_code_free(void *param) {
	plua_metatable_free(param);
	return NULL;
}

static void timeout(uv_timer_t *req) {
	{
		struct plua_metatable_t *table = NULL;
		plua_metatable_init(&table);

		plua_metatable_set_string(table, "protocol", "shelly1");
		plua_metatable_set_number(table, "hwtype", SHELLY);
		plua_metatable_set_string(table, "id", "A123D4");
		plua_metatable_set_string(table, "state", "on");
		plua_metatable_set_string(table, "uuid", "0");

		eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
	}

	{
		struct plua_metatable_t *table = NULL;
		plua_metatable_init(&table);

		plua_metatable_set_string(table, "protocol", "shelly1pm");
		plua_metatable_set_number(table, "hwtype", SHELLY);
		plua_metatable_set_string(table, "id", "A123B4");
		plua_metatable_set_string(table, "state", "on");
		plua_metatable_set_string(table, "uuid", "0");

		eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
	}

	{
		struct plua_metatable_t *table = NULL;
		plua_metatable_init(&table);

		plua_metatable_set_string(table, "protocol", "shellyplug-s");
		plua_metatable_set_number(table, "hwtype", SHELLY);
		plua_metatable_set_string(table, "id", "A123E4");
		plua_metatable_set_string(table, "state", "on");
		plua_metatable_set_string(table, "uuid", "0");

		eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
	}

	{
		struct plua_metatable_t *table = NULL;
		plua_metatable_init(&table);

		plua_metatable_set_string(table, "protocol", "shellyplug");
		plua_metatable_set_number(table, "hwtype", SHELLY);
		plua_metatable_set_string(table, "id", "A123F4");
		plua_metatable_set_string(table, "state", "on");
		plua_metatable_set_string(table, "uuid", "0");

		eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
	}
}

void test_lua_hardware_shelly_send_receive(CuTest *tc) {
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

	test_set_plua_path(tc, __FILE__, "lua_hardware_shelly_send_receive.c");

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_hardware_shelly_send_receive.c", "", &file);

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"mqtt-port\":11883\
		},\
		\"hardware\":{\"shelly\":{}},\
		\"registry\":{}}";

	FILE *f = fopen("lua_hardware_shelly.json", "w");
	fprintf(f, config, file);
	fclose(f);

	FREE(file);

	eventpool_init(EVENTPOOL_THREADED);
	node = eventpool_callback(REASON_RECEIVED_API+10000, receiveAPI1, NULL);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_shelly.json", CONFIG_SETTINGS));
	plua_clear_state(state);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 3000, 3000);

	timer_req1 = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req1);
	uv_timer_init(uv_default_loop(), timer_req1);
	uv_timer_start(timer_req1, (void (*)(uv_timer_t *))timeout, 100, 0);

	mqtt_server(11883);

	hardware_init();

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_shelly.json", CONFIG_HARDWARE));
	plua_clear_state(state);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	sprintf(name, "hardware.%s", "shelly");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("shelly", tmp->name) == 0) {
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

	CuAssertIntEquals(tc, test[0], 1);
	CuAssertIntEquals(tc, test[1], 1);
	CuAssertIntEquals(tc, test[2], 1);
	CuAssertIntEquals(tc, test[3], 1);
	CuAssertIntEquals(tc, test[4], 1);
	CuAssertIntEquals(tc, test[5], 1);
	CuAssertIntEquals(tc, test[6], 1);
	CuAssertIntEquals(tc, test[7], 1);
	CuAssertIntEquals(tc, test[8], 1);
	CuAssertIntEquals(tc, 0, xfree());
}
