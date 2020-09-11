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
#ifndef _WIN32
	#include <unistd.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/mqtt.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"
#include "../libs/pilight/lua_c/table.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"

static CuTest *gtc = NULL;
static uv_timer_t *timer_req1 = NULL;
static uv_timer_t *timer_req2 = NULL;
static uv_timer_t *stop_timer_req = NULL;
static uv_timer_t *start_timer_req = NULL;
static uv_async_t *async_req = NULL;
static uv_thread_t pth;
static int test[3] = { 0 };

static int running = 1;

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

// static void ping(uv_timer_t *handle) {
	// if(running == 1) {
		// mqtt_ping(handle->data);
	// }
// }

static void stop(uv_timer_t *handle) {
	running = 0;
	usleep(1000);

	mqtt_client_gc();
	mqtt_server_gc();
	uv_stop(uv_default_loop());
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	running = 0;
	usleep(1000);

	mqtt_client_gc();
	mqtt_server_gc();;
	uv_stop(uv_default_loop());
}

static void mqtt_callback1(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	if(pkt != NULL) {
		if(pkt->type == MQTT_CONNACK) {
			mqtt_subscribe(client, "#", 0);
		} else if(pkt->type == MQTT_SUBACK) {
		} else if(pkt->type == MQTT_PUBLISH) {
			if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
				test[0] = 1;
			}
			if(strcmp(pkt->payload.publish.topic, "pilight/device/tv/state") == 0) {
				test[1] = 1;
			}
			if(strcmp(pkt->payload.publish.topic, "pilight/lamp/status") == 0) {
				test[2] = 1;
			}
		} else {
		}
	}
}

static void mqtt_callback2(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	if(pkt != NULL) {
		if(pkt->type == MQTT_CONNACK) {
			mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
			mqtt_publish(client, 0, 0, 0, "pilight/device/tv/state", "on");
			mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
		} else {
		}
	} else {
		uv_async_send(async_req);
	}
}

static void start_clients(uv_async_t *handle) {
	usleep(100);
	mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
	usleep(100);
	mqtt_client("127.0.0.1", 11883, "pilight1", NULL, NULL, mqtt_callback2, NULL);
}

void test_mqtt_blacklist(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	test[0] = 0; test[1] = 0; test[2] = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "mqtt_whiteblacklist.c");

	struct lua_state_t *state = NULL;

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
			\"mqtt-blacklist\":[\"tele/sonoff/POWER\",\"pilight/device/#\"]\
		},\
		\"hardware\":{},\
		\"registry\":{}}";

	FILE *f = fopen("mqtt_blacklist.json", "w");
	fprintf(f, "%s", config);
	fclose(f);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "mqtt_blacklist.json", CONFIG_SETTINGS));
	plua_clear_state(state);

	timer_req1 = NULL;
	timer_req2 = NULL;

	running = 1;

	eventpool_init(EVENTPOOL_NO_THREADS);

	async_req = MALLOC(sizeof(uv_async_t));
	if(async_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_req, async_close_cb);

	if((stop_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 2000, 0);

	mqtt_activate();
	mqtt_server(11883);

	start_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, start_timer_req);

	uv_timer_init(uv_default_loop(), start_timer_req);
	uv_timer_start(start_timer_req, (void (*)(uv_timer_t *))start_clients, 500, 0);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_thread_join(&pth);

	storage_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, test[0]);
	CuAssertIntEquals(tc, 0, test[1]);
	CuAssertIntEquals(tc, 1, test[2]);
	CuAssertIntEquals(tc, 0, xfree());
}

void test_mqtt_whitelist(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	test[0] = 0; test[1] = 0; test[2] = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "mqtt_whiteblacklist.c");

	struct lua_state_t *state = NULL;

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"mqtt-whitelist\":[\"pilight/device/tv/state\"],\
				\"mqtt-blacklist\":[\"tele/sonoff/POWER\",\"pilight/device/#\"]\
		},\
		\"hardware\":{},\
		\"registry\":{}}";

	FILE *f = fopen("mqtt_whitelist.json", "w");
	fprintf(f, "%s", config);
	fclose(f);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "mqtt_whitelist.json", CONFIG_SETTINGS));
	plua_clear_state(state);

	timer_req1 = NULL;
	timer_req2 = NULL;

	running = 1;

	eventpool_init(EVENTPOOL_NO_THREADS);

	async_req = MALLOC(sizeof(uv_async_t));
	if(async_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_req, async_close_cb);

	if((stop_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 2000, 0);

	mqtt_activate();
	mqtt_server(11883);

	start_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, start_timer_req);

	uv_timer_init(uv_default_loop(), start_timer_req);
	uv_timer_start(start_timer_req, (void (*)(uv_timer_t *))start_clients, 500, 0);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_thread_join(&pth);

	storage_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, test[0]);
	CuAssertIntEquals(tc, 1, test[1]);
	CuAssertIntEquals(tc, 1, test[2]);
	CuAssertIntEquals(tc, 0, xfree());
}
