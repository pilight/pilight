/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/mqtt.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"

static uv_timer_t *timer_req = NULL;
static uv_timer_t *start_timer_req = NULL;
static uv_async_t *async_close_req = NULL;
static int run = 0;
static int test = 0;
static uv_thread_t pth;
static CuTest *gtc = NULL;
static int foo = 0;
static int _send = 0;
static int _recv = 0;
static int _check = 0;

static int plua_print(lua_State* L) {
	if(strcmp(lua_tostring(L, 1), "send") == 0) {
		lua_remove(L, 1);
		switch(_send++) {
			case 0: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, 1));
				CuAssertStrEquals(gtc, "{[\"type\"]=2,}", lua_tostring(L, 1));
			} break;
		}
	} else if(strcmp(lua_tostring(L, 1), "recv") == 0) {
		lua_remove(L, 1);
		switch(_recv++) {
			case 0: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, 1));
				CuAssertStrEquals(gtc, "{[\"type\"]=2,}", lua_tostring(L, 1));
			} break;
			case 1: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, 1));
				CuAssertStrEquals(gtc, "{[\"qos\"]=2,[\"msgid\"]=0,[\"type\"]=9,}", lua_tostring(L, 1));
			} break;
			case 2:
			case 3:
			case 4:
			case 5: {
				CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, 1));
				if(strstr(lua_tostring(L, 1), "pilight/mqtt/pilight-abcd") != NULL) {
					char *foo = "{[\"qos\"]=2,[\"dub\"]=0,[\"message\"]=connected,[\"type\"]=3,[\"topic\"]=pilight/mqtt/pilight-abcd,[\"msgid\"]=%d,[\"retain\"]=0,}";
					char msg[1024] = { '\0' };
					snprintf((char *)&msg, 1024, foo, _check);
					CuAssertStrEquals(gtc, msg, lua_tostring(L, 1));
					_check++;
				}
				if(strstr(lua_tostring(L, 1), "pilight/mqtt/pilight-send") != NULL) {
					char *foo = "{[\"qos\"]=2,[\"dub\"]=0,[\"message\"]=connected,[\"type\"]=3,[\"topic\"]=pilight/mqtt/pilight-send,[\"msgid\"]=%d,[\"retain\"]=0,}";
					char msg[1024] = { '\0' };
					snprintf((char *)&msg, 1024, foo, _check);
					CuAssertStrEquals(gtc, msg, lua_tostring(L, 1));
					_check++;
				}
				if(strstr(lua_tostring(L, 1), "pilight/lamp/status") != NULL) {
					char *foo = "{[\"qos\"]=2,[\"dub\"]=0,[\"message\"]=off,[\"type\"]=3,[\"topic\"]=pilight/lamp/status,[\"msgid\"]=%d,[\"retain\"]=0,}";
					char msg[1024] = { '\0' };
					snprintf((char *)&msg, 1024, foo, _check);
					CuAssertStrEquals(gtc, msg, lua_tostring(L, 1));
					_check++;
				}
				if(strstr(lua_tostring(L, 1), "tele/sonoff/POWER") != NULL) {
					char *foo = "{[\"qos\"]=2,[\"dub\"]=0,[\"message\"]=on,[\"type\"]=3,[\"topic\"]=tele/sonoff/POWER,[\"msgid\"]=%d,[\"retain\"]=0,}";
					char msg[1024] = { '\0' };
					snprintf((char *)&msg, 1024, foo, _check);
					CuAssertStrEquals(gtc, msg, lua_tostring(L, 1));
					_check++;
				}
			}
		}
	}
	return 0;
}

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

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);


	mqtt_client_gc();
	mqtt_server_gc();
	uv_stop(uv_default_loop());
}

static void stop(uv_timer_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);


	mqtt_client_gc();
	mqtt_server_gc();
	uv_stop(uv_default_loop());
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

static void mqtt_callback(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	if(pkt != NULL) {
		if(pkt->type == MQTT_CONNACK) {
			mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "off");
		}
	}
}

static void start_lua(char *module) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(gtc, file);

	str_replace("lua_network_mqtt.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%s%s.lua", file, module);
	FREE(file);
	file = NULL;

	p = name;

	plua_module_load(path, UNITTEST);

	state = plua_get_free_state();
	CuAssertPtrNotNull(gtc, state);
	CuAssertPtrNotNull(gtc, (L = state->L));

	CuAssertIntEquals(gtc, 0, plua_module_exists(module, UNITTEST));

	sprintf(name, "unittest.%s", module);
	lua_getglobal(L, name);
	CuAssertIntEquals(gtc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp(module, tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(gtc, file);
	CuAssertIntEquals(gtc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	plua_clear_state(state);
}

static void start_clients(void *param) {
	switch(foo++) {
		case 0: {
			start_lua("mqtt_recv");
		} break;
		case 1: {
			start_lua("mqtt_send");
		} break;
		case 2: {
			mqtt_client("127.0.0.1", 11883, "pilight-abcd", NULL, NULL, mqtt_callback, NULL);
		} break;
	}
}

static void test_lua_network_mqtt_server_client(CuTest *tc) {

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 1;

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	/*
	 * Don't make this too quick so we can properly test the
	 * timeout of the mqtt library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 2500, 0);

	eventpool_init(EVENTPOOL_NO_THREADS);

	mqtt_activate();
	mqtt_server(11883);

	start_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, start_timer_req);

	uv_timer_init(uv_default_loop(), start_timer_req);
	uv_timer_start(start_timer_req, (void (*)(uv_timer_t *))start_clients, 500, 500);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_pause_coverage(0);
	plua_gc();
	uv_thread_join(&pth);

	CuAssertIntEquals(tc, 4, _check);
	CuAssertIntEquals(tc, 6, _recv);
	CuAssertIntEquals(tc, 1, _send);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_network_mqtt(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_network_mqtt_server_client);

	return suite;
}
