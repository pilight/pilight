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
#include <unistd.h>

#include <wiringx.h>

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
static uv_pipe_t *pipe_req0 = NULL;
static uv_pipe_t *pipe_req1 = NULL;
static uv_poll_t *poll_req = NULL;
static struct eventpool_listener_t *node = NULL;

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

static int iter = 0;

static void foo(int prio, char *file, int line, const char *format_str, ...) {
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
			wiringXGC();
			eventpool_callback_remove(node);
			uv_poll_stop(poll_req);
			uv_stop(uv_default_loop());
		}
	}
	check = 1;
	round++;
	return NULL;
}

static void poll_cb(uv_poll_t *req, int status, int events) {
	int fd = req->io_watcher.fd;
	int duration = 0;

	if(events & UV_WRITABLE) {
		duration = pulses[iter++];
		write(fd, "a", 1);
		usleep(duration);
		if(pulses[iter] == 0) {
			iter = 0;
		}
	}
}

static void connect_cb(uv_stream_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);

	uv_os_fd_t fd = 0;
	uv_pipe_t *client_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, client_req);

	int r = uv_pipe_init(uv_default_loop(), client_req, 0);

	r = uv_accept(req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_fileno((uv_handle_t *)client_req, &fd);

	if((void *)req == (void *)pipe_req1) {
		poll_req = MALLOC(sizeof(uv_poll_t));
		CuAssertPtrNotNull(gtc, poll_req);

		uv_poll_init(uv_default_loop(), poll_req, fd);
		uv_poll_start(poll_req, UV_WRITABLE, poll_cb);
	}

	return;
}

void test_lua_hardware_433gpio_receive(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}

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

	test_set_plua_path(tc, __FILE__, "lua_hardware_433gpio_receive.c");

	plua_overwrite_print();

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_hardware_433gpio_receive.c", "", &file);

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"gpio-platform\":\"gpio-stub\"\
		},\
		\"hardware\":{\"433gpio\":{\"sender\":0,\"receiver\":1}},\
		\"registry\":{}}";

	FILE *f = fopen("lua_hardware_433gpio.json", "w");
	fprintf(f, config, file);
	fclose(f);

	FREE(file);

	eventpool_init(EVENTPOOL_THREADED);
	node = eventpool_callback(REASON_RECEIVED_OOK+10000, listener, NULL);

	CuAssertIntEquals(tc, 0, config_read("lua_hardware_433gpio.json", CONFIG_SETTINGS));

	int r = 0;

	pipe_req1 = MALLOC(sizeof(uv_pipe_t));
	pipe_req0 = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, pipe_req1);
	CuAssertPtrNotNull(gtc, pipe_req0);

	r = uv_pipe_init(uv_default_loop(), pipe_req0, 1);
	CuAssertIntEquals(gtc, 0, r);
	r = uv_pipe_init(uv_default_loop(), pipe_req1, 1);
	CuAssertIntEquals(gtc, 0, r);

	uv_fs_t file_req0;
	uv_fs_t file_req1;
	uv_fs_unlink(uv_default_loop(), &file_req0, "/dev/gpio0", NULL);
	uv_fs_unlink(uv_default_loop(), &file_req1, "/dev/gpio1", NULL);

	r = uv_pipe_bind(pipe_req0, "/dev/gpio0");
	CuAssertIntEquals(gtc, 0, r);
	r = uv_pipe_bind(pipe_req1, "/dev/gpio1");
	CuAssertIntEquals(gtc, 0, r);

	r = uv_listen((uv_stream_t *)pipe_req0, 128, connect_cb);
	CuAssertIntEquals(gtc, 0, r);
	r = uv_listen((uv_stream_t *)pipe_req1, 128, connect_cb);
	CuAssertIntEquals(gtc, 0, r);

	struct plua_metatable_t *table = config_get_metatable();
	plua_metatable_set_number(table, "registry.hardware.RF433.mingaplen", 5100);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxgaplen", 99999);
	plua_metatable_set_number(table, "registry.hardware.RF433.minrawlen", 25);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxrawlen", 512);

	hardware_init();

	CuAssertIntEquals(tc, 0, config_read("lua_hardware_433gpio.json", CONFIG_HARDWARE));

	uv_mutex_unlock(&state->lock);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	sprintf(name, "hardware.%s", "433gpio");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("433gpio", tmp->name) == 0) {
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
	wiringXGC();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}
