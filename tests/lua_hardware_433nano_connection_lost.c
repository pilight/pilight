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
static pthread_t pthread;
static struct eventpool_listener_t *node = NULL;
static int fd = 0;
static int run = 1;

static int round = 0;
static int check = 0;

static int pulses[] = {
	286, 2825, 286, 201,
	289, 1337, 287, 209,
	283, 1351, 287, 204,
	289, 1339, 288, 207,
	288, 1341, 289, 207,
	281, 1343, 284, 205,
	292, 1346, 282, 212,
	283, 1348, 282, 213,
	279, 1352, 282, 211,
	281, 1349, 282, 210,
	283, 1347, 284, 211,
	288, 1348, 281, 211,
	285, 1353, 278, 213,
	280, 1351, 280, 232,
	282, 1356, 279, 213,
	285, 1351, 276, 215,
	285, 1348, 277, 216,
	278, 1359, 278, 216,
	279, 1353, 272, 214,
	283, 1358, 276, 216,
	276, 1351, 278, 214,
	284, 1357, 275, 217,
	276, 1353, 270, 217,
	277, 1353, 272, 220,
	277, 1351, 275, 220,
	272, 1356, 275, 1353,
	273, 224, 277, 236,
	282, 1355, 272, 1353,
	273, 233, 273, 222,
	268, 1358, 270, 219,
	277, 1361, 274, 218,
	280, 1358, 272, 1355,
	271, 243, 251, 11302,
	0
};

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

static void *listener(int reason, void *param, void *userdata) {
	struct plua_metatable_t *table = param;
	double length = 0.0;
	double pulse = 0.0;
	char nr[255], *p = nr;
	int buffer[133];
	memset(&buffer, 0, 133*sizeof(int));

	memset(&nr, 0, 255);

	int i = 0;
	plua_metatable_get_number(table, "length", &length);

	for(i=1;i<length;i++) {
		snprintf(p, 254, "pulses.%d", i);
		plua_metatable_get_number(table, nr, &pulse);
		buffer[i-1] = (int)pulse;
	}

	if(round >= 1) {
		for(i=0;i<length-1;i++) {
			if(!((int)(buffer[i]*0.75) <= pulses[i] && (int)(buffer[i]*1.25) >= pulses[i])) {
				check = 0;
			}
		}
		if(check == 1 || round > 5) {
			run = 0;
			eventpool_callback_remove(node);
			uv_stop(uv_default_loop());
			plua_gc();
			return NULL;
		}
	}
	check = 1;
	round++;
	return NULL;
}

static void *thread(void *arg) {
	sleep(1);
	unlink("/tmp/usb0");
	sleep(2);
	fd = open("/tmp/usb0", O_CREAT | O_RDWR, 0777);

	int fd = *((int *)arg);
	char *code = "c:010203020302030203020302030203020302030203020302030203020302030203020302030203020302030203020302030203030202030302020302030203030204;p:286,2825,210,1353,11302;r:1@";
	int l = strlen(code);
	while(run) {
		write(fd, code, l);
		usleep(10000);
	}
	return NULL;
}

void test_lua_hardware_433nano_connection_lost(CuTest *tc) {
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

	test_set_plua_path(tc, __FILE__, "lua_hardware_433nano_connection_lost.c");

	plua_overwrite_print();

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_hardware_433nano_connection_lost.c", "", &file);

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
	node = eventpool_callback(10006, listener, NULL);

	CuAssertIntEquals(tc, 0, config_read("lua_hardware_433nano.json", CONFIG_SETTINGS));

	unlink("/tmp/usb0");
	fd = open("/tmp/usb0", O_CREAT | O_RDWR, 0777);

	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);

	pthread_create(&pthread, NULL, thread, &fd);

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
