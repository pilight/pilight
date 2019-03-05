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

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"
#include "../libs/pilight/lua_c/table.h"

#include "alltests.h"

static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;
static int fd = 0;
static int run = 1;
static char rbuffer[BUFFER_SIZE];

static int check = 0;

static int pulses[] = {
	286, 2825, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 1353,
	286,  210, 286, 210,
	286, 1353, 286, 1353,
	286,  210, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 210,
	286, 1353, 286, 1353,
	286,  210, 286, 11302
};

static void *reason_send_code_free(void *param) {
	plua_metatable_free(param);
	return NULL;
}

static int plua_print(lua_State* L) {
	plua_stack_dump(L);
	return 0;
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void plua_overwrite_print(void) {
	struct lua_state_t *state[NRLUASTATES];
	struct lua_State *L = NULL;
	int i = 0;

	for(i=0;i<NRLUASTATES;i++) {
		state[i] = plua_get_free_state();

		if(state[i] == NULL) {
			return;
		}
		if((L = state[i]->L) == NULL) {
			uv_mutex_unlock(&state[i]->lock);
			return;
		}

		lua_getglobal(L, "_G");
		lua_pushcfunction(L, plua_print);
		lua_setfield(L, -2, "print");
		lua_pop(L, 1);
	}
	for(i=0;i<NRLUASTATES;i++) {
		uv_mutex_unlock(&state[i]->lock);
	}
}

static int call(struct lua_State *L, char *file, char *func) {
	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		return -1;
	}

	if(lua_pcall(L, 0, 0, 0) == LUA_ERRRUN) {
		logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
		lua_pop(L, 1);
		return -1;
	}

	lua_pop(L, 1);

	return 1;
}

static void callback(uv_fs_t *req) {
	unsigned int len = req->result;

	uv_fs_req_cleanup(req);
	FREE(req);
	if(len > 0) {
		if(strcmp(rbuffer, "c:010203020302030203020302030203020302030203020302030203020302030203020302030203020302030203020302030203030202030302020302030203030204;p:286,2825,210,1353,11302;r:1@") == 0) {
			run = 0;
			check = 1;
			uv_stop(uv_default_loop());
			plua_gc();
			return;
		}
	}

	uv_fs_t *read_req = NULL;
	if((read_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	memset(rbuffer, '\0', BUFFER_SIZE);

	uv_buf_t buf = uv_buf_init(rbuffer, BUFFER_SIZE);
	uv_fs_read(uv_default_loop(), read_req, fd, &buf, 1, -1, callback);
}

static void timeout(uv_timer_t *req) {
	int rawlen = sizeof(pulses)/sizeof(pulses[0]), i = 0;
	char key[255];
	struct plua_metatable_t *table = MALLOC(sizeof(struct plua_metatable_t));
	CuAssertPtrNotNull(gtc, table);

	memset(table, 0, sizeof(struct plua_metatable_t));

	memset(&key, 0, 255);

	plua_metatable_set_number(table, "rawlen", rawlen);
	plua_metatable_set_number(table, "txrpt", 1);
	plua_metatable_set_string(table, "protocol", "dummy");
	plua_metatable_set_number(table, "hwtype", 1);
	plua_metatable_set_string(table, "uuid", "0");

	for(i=0;i<rawlen;i++) {
		snprintf(key, 255, "pulses.%d", i+1);
		plua_metatable_set_number(table, key, pulses[i]);
	}

	eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
}

void test_lua_hardware_433nano_send(CuTest *tc) {
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

	test_set_plua_path(tc, __FILE__, "lua_hardware_433nano_send.c");

	plua_overwrite_print();

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_hardware_433nano_send.c", "", &file);

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\"\
		},\
		\"hardware\":{\"433nano\":{\"comport\":\"/tmp/usb0\"}},\
		\"registry\":{}}";

	FILE *f = fopen("lua_hardware_433nano.json", "w");
	fprintf(f, config, file);
	fclose(f);

	FREE(file);

	eventpool_init(EVENTPOOL_THREADED);

	CuAssertIntEquals(tc, 0, config_read("lua_hardware_433nano.json", CONFIG_SETTINGS));

	unlink("/tmp/usb0");
	fd = open("/tmp/usb0", O_CREAT | O_RDWR, 0777);

	uv_fs_t *read_req = NULL;
	if((read_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	memset(rbuffer, '\0', BUFFER_SIZE);

	uv_buf_t buf = uv_buf_init(rbuffer, BUFFER_SIZE);
	uv_fs_read(uv_default_loop(), read_req, fd, &buf, 1, -1, callback);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))timeout, 500, 500);

	hardware_init();

	CuAssertIntEquals(tc, 0, config_read("lua_hardware_433nano.json", CONFIG_HARDWARE));

	uv_mutex_unlock(&state->lock);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	sprintf(name, "hardware.%s", "433nano");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("433nano", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	uv_mutex_unlock(&state->lock);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}
