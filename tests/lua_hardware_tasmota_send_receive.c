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
			if(strcmp("{\"origin\":\"receiver\",\"message\":{\"state\":\"on\",\"id\":\"tasmota-pilight\"},\"protocol\":\"tasmota_switch\"}", foo) == 0) {
				test[0]++;
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

			mqtt_publish(client, 0, 0, 0, "tele/tasmota-pilight/LWT", "Online");
			mqtt_publish(client, 0, 0, 0, "cmnd/tasmota-pilight/POWER", "Online");
		}
		if(pkt->type == MQTT_PUBLISH) {
			/*
			 * pilight to Tasmota
			 */
			if(strcmp(pkt->payload.publish.topic, "cmnd/tasmota-pilight/status") == 0 &&
				 strcmp(pkt->payload.publish.message, "0") == 0) {
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS", "{\"Status\":{\"Module\":1,\"FriendlyName\":[\"tasmota-pilight\"],\"Topic\":\"tasmota-pilight\",\"ButtonTopic\":\"0\",\"Power\":1,\"PowerOnState\":3,\"LedState\":1,\"SaveData\":1,\"SaveState\":1,\"SwitchTopic\":\"0\",\"SwitchMode\":[0,0,0,0,0,0,0,0],\"ButtonRetain\":0,\"SwitchRetain\":0,\"SensorRetain\":0,\"PowerRetain\":0}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS1", "{\"StatusPRM\":{\"Baudrate\":115200,\"GroupTopic\":\"sonoffs\",\"OtaUrl\":\"http://thehackbox.org/tasmota/release/sonoff.bin\",\"RestartReason\":\"Software/System restart\",\"Uptime\":\"0T00:00:08\",\"StartupUTC\":\"\",\"Sleep\":50,\"CfgHolder\":4617,\"BootCount\":67,\"SaveCount\":773,\"SaveAddress\":\"F7000\"}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS2", "{\"StatusFWR\":{\"Version\":\"6.5.0(release-sonoff)\",\"BuildDateTime\":\"2019-03-19T12:24:10\",\"Boot\":7,\"Core\":\"2_3_0\",\"SDK\":\"1.5.3(aec24ac9)\"}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS3", "{\"StatusLOG\":{\"SerialLog\":2,\"WebLog\":2,\"SysLog\":0,\"LogHost\":\"\",\"LogPort\":514,\"SSId\":[\"WIFI\",\"\"],\"TelePeriod\":300,\"Resolution\":\"558180C0\",\"SetOption\":[\"00008009\",\"280500000100000000000000000000000000\",\"00000020\"]}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS4", "{\"StatusMEM\":{\"ProgramSize\":507,\"Free\":496,\"Heap\":14,\"ProgramFlashSize\":1024,\"FlashSize\":1024,\"FlashChipId\":\"146085\",\"FlashMode\":3,\"Features\":[\"0000000\",\"00000000\",\"00000000\",\"00000000\",\"00000000\"]}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS5", "{\"StatusMQT\":{\"MqttHost\":\"192.168.2.1\",\"MqttPort\":1883,\"MqttClientMask\":\"DVES_12345678\",\"MqttClient\":\"DVES_12345678\",\"MqttUser\":\"DVES_USER\",\"MqttCount\":1,\"MAX_PACKET_SIZE\":1000,\"KEEPALIVE\":15}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS6", "{\"StatusTIM\":{\"UTC\":\"Thu Jan 01 00:00:09 1970\",\"Local\":\"Thu Jan 01 00:00:09 1970\",\"StartDST\":\"Thu Jan 01 00:00:00 1970\",\"EndDST\":\"Thu Jan 01 00:00:00 1970\",\"Timezone\":\"+00:00\",\"Sunrise\":\"07:43\",\"Sunset\":\"16:03\"}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS10", "{\"StatusSNS\":{\"Time\":\"1970-01-01T00:00:09\"}}");
				mqtt_publish(client, 0, 0, 0, "stat/tasmota-pilight/STATUS11", "{\"StatusSTS\":{\"Time\":\"1970-01-01T00:00:09\",\"Uptime\":\"0T00:00:08\",\"Vcc\":3.223,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":34,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"WIFI\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":6,\"RSSI\":62,\"LinkCount\":1,\"Downtime\":\"0T00:00:04\"}}}");
			}
			if(strcmp(pkt->payload.publish.topic, "cmnd/tasmota-pilight/POWER") == 0 &&
				 strcmp(pkt->payload.publish.message, "off") == 0) {
				test[1]++;
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

		plua_metatable_set_string(table, "protocol", "tasmota_switch");
		plua_metatable_set_number(table, "hwtype", TASMOTA);
		plua_metatable_set_string(table, "id", "tasmota-pilight");
		plua_metatable_set_string(table, "state", "off");
		plua_metatable_set_string(table, "uuid", "0");

		eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
	}
}

void test_lua_hardware_tasmota_send_receive(CuTest *tc) {
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

	test_set_plua_path(tc, __FILE__, "lua_hardware_tasmota_send_receive.c");

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_hardware_tasmota_send_receive.c", "", &file);

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"mqtt-port\":11883\
		},\
		\"hardware\":{\"tasmota\":{}},\
		\"registry\":{}}";

	FILE *f = fopen("lua_hardware_tasmota.json", "w");
	fprintf(f, config, file);
	fclose(f);

	FREE(file);

	eventpool_init(EVENTPOOL_THREADED);
	node = eventpool_callback(REASON_RECEIVED_API+10000, receiveAPI1, NULL);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_tasmota.json", CONFIG_SETTINGS));
	plua_clear_state(state);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 1000);

	timer_req1 = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req1);
	uv_timer_init(uv_default_loop(), timer_req1);
	uv_timer_start(timer_req1, (void (*)(uv_timer_t *))timeout, 500, 0);

	mqtt_server(11883);

	hardware_init();

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_tasmota.json", CONFIG_HARDWARE));
	plua_clear_state(state);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	sprintf(name, "hardware.%s", "tasmota");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("tasmota", tmp->name) == 0) {
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
