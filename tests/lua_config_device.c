/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"
#include "../libs/pilight/protocols/API/datetime.h"
#include "../libs/pilight/protocols/GPIO/relay.h"
#include "../libs/pilight/protocols/433.92/arctech_switch.h"
#include "../libs/pilight/protocols/433.92/arctech_dimmer.h"
#include "../libs/pilight/protocols/433.92/arctech_screen.h"
#include "../libs/pilight/protocols/generic/generic_label.h"
#include "alltests.h"

static struct {
	union {
		int number_;
		char *string_;
	} var;
	int type;
} lua_return[20];

static int iter = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static int plua_print(lua_State* L) {
	CuAssertIntEquals(gtc, lua_type(L, -1), lua_return[iter].type);
	switch(lua_type(L, -1)) {
		case LUA_TSTRING:
			CuAssertStrEquals(gtc, lua_return[iter].var.string_, lua_tostring(L, -1));
		break;
		case LUA_TNUMBER: {
			CuAssertIntEquals(gtc, lua_return[iter].var.number_, lua_tonumber(L, -1));
		} break;
		case LUA_TBOOLEAN:
			CuAssertIntEquals(gtc, lua_return[iter].var.number_, (int)lua_toboolean(L, -1));
		break;
		case LUA_TUSERDATA: {
		} break;
		case LUA_TTABLE: {
			if(lua_gettop(L) > 1) {
				struct JsonNode *json = json_mkobject();
				lua_pushnil(L);
				while(lua_next(L, -2)) {
					lua_pushvalue(L, -2);
					const char *key = lua_tostring(L, -1);
					const char *value = lua_tostring(L, -2);
					json_append_member(json, key, json_mkstring(value));
					lua_pop(L, 2);
				}
				lua_pop(L, 1);

				char *out = json_stringify(json, NULL);
				CuAssertStrEquals(gtc, lua_return[iter].var.string_, out);
				json_free(out);
				json_delete(json);
			}
		} break;
	}
	iter++;
	return 0;
}

static void stop(uv_work_t *req) {
	uv_stop(uv_default_loop());
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

static void test_lua_config_device_unknown(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	iter = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	/* print(dev); */
	lua_return[0].type = LUA_TNIL;

	luaL_dostring(state->L, "\
		local config = pilight.config(); \
		local dev = config.getDevice(\"test\");\
		print(dev);\
	");

	uv_mutex_unlock(&state->lock);

	eventpool_gc();
	plua_pause_coverage(0);
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_config_device_switch(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	iter = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"kaku_switch\"],\"id\":[{\"id\":100,\"unit\":1},{\"id\":200,\"unit\":2}],\"state\":\"off\"}}," \
		"\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}

	FILE *f = fopen("lua_config.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	arctechSwitchInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "lua_config_device.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("lua_config.json", CONFIG_DEVICES));

	eventpool_init(EVENTPOOL_THREADED);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	int i = 0;
	/* print(dev); */
	lua_return[i].type = LUA_TTABLE;
	lua_return[i].var.string_ = STRDUP("{}");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getType[1]); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getName()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("switch");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getState()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("off");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(#dev:getId()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:getId()[1]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 100;

	/* print(dev:getId()[1]['unit']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getId()[2]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 200;

	/* print(dev:getId()[2]['unit']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:hasState("on")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("off")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("foo")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:hasSetting("state")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasSetting("dimlevel")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:setState(\"on\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	int ret = luaL_dostring(state->L, "\
		local config = pilight.config(); \
		local dev = config.getDevice(\"switch\");\
		print(dev);\
		print(dev.getType()[1]);\
		print(dev.getName());\
		print(dev.getState());\
		print(#dev.getId());\
		print(dev.getId()[1]['id']);\
		print(dev.getId()[1]['unit']);\
		print(dev.getId()[2]['id']);\
		print(dev.getId()[2]['unit']);\
		print(dev.hasState(\"on\"));\
		print(dev.hasState(\"off\"));\
		print(dev.hasState(\"foo\"));\
		print(dev.hasSetting(\"state\"));\
		print(dev.hasSetting(\"dimlevel\"));\
		print(dev.setState(\"on\"));\
	");
	CuAssertIntEquals(tc, 0, ret);

	FREE(lua_return[0].var.string_);
	FREE(lua_return[2].var.string_);
	FREE(lua_return[3].var.string_);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	storage_gc();
	protocol_gc();
	eventpool_gc();
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_config_device_screen(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	iter = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	char config[1024] = "{\"devices\":{\"screen\":{\"protocol\":[\"kaku_screen\"],\"id\":[{\"id\":100,\"unit\":1},{\"id\":200,\"unit\":2}],\"state\":\"down\"}}," \
		"\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}

	FILE *f = fopen("lua_config.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	arctechScreenInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "lua_config_device.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("lua_config.json", CONFIG_DEVICES));

	eventpool_init(EVENTPOOL_THREADED);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	int i = 0;
	/* print(dev); */
	lua_return[i].type = LUA_TTABLE;
	lua_return[i].var.string_ = STRDUP("{}");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getType[1]); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 5;

	/* print(dev:getName()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("screen");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getState()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("down");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(#dev:getId()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:getId()[1]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 100;

	/* print(dev:getId()[1]['unit']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getId()[2]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 200;

	/* print(dev:getId()[2]['unit']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:hasState("up")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("down")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("on")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:hasSetting("state")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasSetting("dimlevel")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:setState(\"up\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	int ret = luaL_dostring(state->L, "\
		local config = pilight.config(); \
		local dev = config.getDevice(\"screen\");\
		print(dev);\
		print(dev.getType()[1]);\
		print(dev.getName());\
		print(dev.getState());\
		print(#dev.getId());\
		print(dev.getId()[1]['id']);\
		print(dev.getId()[1]['unit']);\
		print(dev.getId()[2]['id']);\
		print(dev.getId()[2]['unit']);\
		print(dev.hasState(\"up\"));\
		print(dev.hasState(\"down\"));\
		print(dev.hasState(\"on\"));\
		print(dev.hasSetting(\"state\"));\
		print(dev.hasSetting(\"dimlevel\"));\
		print(dev.setState(\"up\"));\
	");
	CuAssertIntEquals(tc, 0, ret);

	FREE(lua_return[0].var.string_);
	FREE(lua_return[2].var.string_);
	FREE(lua_return[3].var.string_);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	storage_gc();
	protocol_gc();
	eventpool_gc();
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_config_device_relay(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	iter = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	char config[1024] = "{\"devices\":{\"foo\":{\"protocol\":[\"relay\"],\"id\":[{\"gpio\":1}],\"state\":\"on\"}}," \
		"\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}

	FILE *f = fopen("lua_config.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	relayInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "lua_config_device.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("lua_config.json", CONFIG_DEVICES));

	eventpool_init(EVENTPOOL_THREADED);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	int i = 0;
	/* print(dev); */
	lua_return[i].type = LUA_TTABLE;
	lua_return[i].var.string_ = STRDUP("{}");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getType[1]); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 4;

	/* print(dev:getName()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("foo");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getState()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("on");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(#dev:getId()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getId()[1]['gpio']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("on")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("off")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("down")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:hasSetting("state")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasSetting("dimlevel")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:setState(\"up\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	int ret = luaL_dostring(state->L, "\
		local config = pilight.config();\
		local dev = config.getDevice(\"foo\");\
		print(dev);\
		print(dev.getType()[1]);\
		print(dev.getName());\
		print(dev.getState());\
		print(#dev.getId());\
		print(dev.getId()[1]['gpio']);\
		print(dev.hasState(\"on\"));\
		print(dev.hasState(\"off\"));\
		print(dev.hasState(\"down\"));\
		print(dev.hasSetting(\"state\"));\
		print(dev.hasSetting(\"dimlevel\"));\
		print(dev.setState(\"on\"));\
	");
	CuAssertIntEquals(tc, 0, ret);

	FREE(lua_return[0].var.string_);
	FREE(lua_return[2].var.string_);
	FREE(lua_return[3].var.string_);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	storage_gc();
	protocol_gc();
	eventpool_gc();
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_config_device_label(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	iter = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	char config[1024] = "{\"devices\":{\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":100},{\"id\":200}],\"label\":\"foo bar\",\"color\":\"pink\"}}," \
		"\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}

	FILE *f = fopen("lua_config.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "lua_config_device.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("lua_config.json", CONFIG_DEVICES));

	eventpool_init(EVENTPOOL_THREADED);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	int i = 0;
	/* print(dev); */
	lua_return[i].type = LUA_TTABLE;
	lua_return[i].var.string_ = STRDUP("{}");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getType[1]); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 15;

	/* print(dev:getName()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("label");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getColor()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("pink");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getLabel()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("foo bar");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(#dev:getId()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:getId()[1]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 100;

	/* print(dev:getId()[2]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 200;

	/* print(dev:hasSetting("state")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:hasSetting("dimlevel")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:hasSetting("color")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasSetting("label")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:setLabel(\"bar foo\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:setLabel(\"black\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	int ret = luaL_dostring(state->L, "\
		local config = pilight.config(); \
		local dev = config.getDevice(\"label\");\
		print(dev);\
		print(dev.getType()[1]);\
		print(dev.getName());\
		print(dev.getColor());\
		print(dev.getLabel());\
		print(#dev.getId());\
		print(dev.getId()[1]['id']);\
		print(dev.getId()[2]['id']);\
		print(dev.hasSetting(\"state\"));\
		print(dev.hasSetting(\"dimlevel\"));\
		print(dev.hasSetting(\"color\"));\
		print(dev.hasSetting(\"label\"));\
		print(dev.setColor(\"bar foo\"));\
		print(dev.setColor(\"black\"));\
	");
	CuAssertIntEquals(tc, 0, ret);

	FREE(lua_return[0].var.string_);
	FREE(lua_return[2].var.string_);
	FREE(lua_return[3].var.string_);
	FREE(lua_return[4].var.string_);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	storage_gc();
	protocol_gc();
	eventpool_gc();
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_config_device_datetime(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	iter = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	char config[1024] = "{\"devices\":{\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"year\":2015,\"month\":1,\"day\":27,\"hour\":14,\"minute\":37,\"second\":8,\"weekday\":3,\"dst\":1}},\"gui\":{},\"rules\":{},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}

	FILE *f = fopen("lua_config.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	datetimeInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "lua_config_device.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("lua_config.json", CONFIG_DEVICES));

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	int i = 0;
	/* print(dev); */
	lua_return[i].type = LUA_TTABLE;
	lua_return[i].var.string_ = STRDUP("{}");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getType[1]); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 8;

	/* print(dev:getName()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("test");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getYear()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2015;

	/* print(dev:getMonth()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getDay()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 27;

	/* print(dev:getHour()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 14;

	/* print(dev:getMinute()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 37;

	/* print(dev:getSecond()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 8;

	/* print(dev:getWeekday()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 3;

	/* print(dev:getDST()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getTable()); */
	lua_return[i].type = LUA_TTABLE;
	lua_return[i].var.string_ = STRDUP(
		"{\"day\":\"27\",\"weekday\":\"2\",\"second\":\"8\",\"minute\":\"37\",\"year\":\"2015\",\"month\":\"1\",\"hour\":\"14\"}"
	);
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(#dev:getId()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getId()[1]['longitude']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 4.895167899999933;

	/* print(dev:getId()[1]['latitude']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 52.3702157;

	/* print(dev:hasSetting(\"hour\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasSetting(\"state\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	int ret = luaL_dostring(state->L, "\
		local config = pilight.config(); \
		local dev = config.getDevice(\"test\");\
		print(dev);\
		print(dev.getType()[1]);\
		print(dev.getName());\
		print(dev.getYear());\
		print(dev.getMonth());\
		print(dev.getDay());\
		print(dev.getHour());\
		print(dev.getMinute());\
		print(dev.getSecond());\
		print(dev.getWeekday());\
		print(dev.getDST());\
		print(dev.getTable());\
		print(#dev.getId());\
		print(dev.getId()[1]['longitude']);\
		print(dev.getId()[1]['latitude']);\
		print(dev.hasSetting(\"hour\"));\
		print(dev.hasSetting(\"state\"));\
	");

	CuAssertIntEquals(tc, 0, ret);

	FREE(lua_return[0].var.string_);
	FREE(lua_return[2].var.string_);
	FREE(lua_return[11].var.string_);

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	storage_gc();
	protocol_gc();
	eventpool_gc();
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_config_device_dimmer(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	iter = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	char config[1024] = "{\"devices\":{\"dimmer\":{\"protocol\":[\"kaku_dimmer\"],\"id\":[{\"id\":100,\"unit\":1},{\"id\":200,\"unit\":2}],\"state\":\"off\",\"dimlevel\":10}}," \
		"\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}

	FILE *f = fopen("lua_config.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	arctechDimmerInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "lua_config_device.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("lua_config.json", CONFIG_DEVICES));

	eventpool_init(EVENTPOOL_THREADED);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);
	struct lua_state_t *state = plua_get_free_state();

	int i = 0;
	/* print(dev); */
	lua_return[i].type = LUA_TTABLE;
	lua_return[i].var.string_ = STRDUP("{}");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getType[1]); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:getName()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("dimmer");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(dev:getState()); */
	lua_return[i].type = LUA_TSTRING;
	lua_return[i].var.string_ = STRDUP("off");
	CuAssertPtrNotNull(tc, lua_return[i++].var.string_);

	/* print(#dev:getId()); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:getId()[1]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 100;

	/* print(dev:getId()[1]['unit']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 1;

	/* print(dev:getId()[2]['id']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 200;

	/* print(dev:getId()[2]['unit']); */
	lua_return[i].type = LUA_TNUMBER;
	lua_return[i++].var.number_ = 2;

	/* print(dev:hasState("on")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("off")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasState("foo")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:hasSetting("state")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:hasSetting("dimlevel")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:setState(\"on\")); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:setDimlevel(5)); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 1;

	/* print(dev:setDimlevel(-1)); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	/* print(dev:setDimlevel(99)); */
	lua_return[i].type = LUA_TBOOLEAN;
	lua_return[i++].var.number_ = 0;

	int ret = luaL_dostring(state->L, "\
		local config = pilight.config(); \
		local dev = config.getDevice(\"dimmer\");\
		print(dev);\
		print(dev.getType()[1]);\
		print(dev.getName());\
		print(dev.getState());\
		print(#dev.getId());\
		print(dev.getId()[1]['id']);\
		print(dev.getId()[1]['unit']);\
		print(dev.getId()[2]['id']);\
		print(dev.getId()[2]['unit']);\
		print(dev.hasState(\"on\"));\
		print(dev.hasState(\"off\"));\
		print(dev.hasState(\"foo\"));\
		print(dev.hasSetting(\"state\"));\
		print(dev.hasSetting(\"dimlevel\"));\
		print(dev.setState(\"on\"));\
		print(dev.setDimlevel(10));\
		print(dev.setDimlevel(-1));\
		print(dev.setDimlevel(99));\
	");
	CuAssertIntEquals(tc, 0, ret);

	FREE(lua_return[0].var.string_);
	FREE(lua_return[2].var.string_);
	FREE(lua_return[3].var.string_);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_mutex_unlock(&state->lock);

	plua_pause_coverage(0);
	storage_gc();
	protocol_gc();
	eventpool_gc();
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_config(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_config_device_unknown);
	SUITE_ADD_TEST(suite, test_lua_config_device_switch);
	SUITE_ADD_TEST(suite, test_lua_config_device_relay);
	SUITE_ADD_TEST(suite, test_lua_config_device_screen);
	SUITE_ADD_TEST(suite, test_lua_config_device_label);
	SUITE_ADD_TEST(suite, test_lua_config_device_datetime);
	SUITE_ADD_TEST(suite, test_lua_config_device_dimmer);

	return suite;
}
