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
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"
#include "../libs/pilight/lua_c/table.h"

#include "alltests.h"

void wiringXIOCTLCallback(int (*callback)(int fd, unsigned long req, void *cmd));

static CuTest *gtc = NULL;
static pthread_t pthread;
static char rbuffer[BUFFER_SIZE];
static uv_timer_t *timer_req = NULL;
static struct eventpool_listener_t *node = NULL;
static int fd = 0;
static int run = 1;

static int check = 0;
static int foo = 0;

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

static void *listener(int reason, void *param, void *userdata) {
	check = 1;
	return NULL;
}

static void *thread(void *arg) {
	int fd = *((int *)arg);
	char *code = "c:1304400446000014740000046044744344000444063444440037:73370;453744<0>0041;p:140,1440,2470,810,300,1940,430,510,3250,3540,1110,1010,2080,2710,3940,5740@";
	int l = strlen(code);
	while(run) {
		CuAssertIntEquals(gtc, l, write(fd, code, l));
		usleep(10000);
	}
	return NULL;
}

static void callback(uv_fs_t *req) {
	unsigned int len = req->result;

	uv_fs_req_cleanup(req);
	FREE(req);

	if(len > 0) {
		if(strcmp(rbuffer, "   2       0       0    6321       0   0   0   0   0   0 150   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0    4097    4097") == 0) {
			pthread_attr_t attrs;
			pthread_attr_init(&attrs);
			pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);

			pthread_create(&pthread, NULL, thread, &fd);
			return;
		}
	}

	if(run == 1) {
		uv_fs_t *read_req = NULL;
		if((read_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		memset(rbuffer, '\0', BUFFER_SIZE);

		uv_buf_t buf = uv_buf_init(rbuffer, BUFFER_SIZE);
		uv_fs_read(uv_default_loop(), read_req, fd, &buf, 1, -1, callback);
	}
}

static void stop(uv_work_t *req) {
	run = 0;
	eventpool_callback_remove(node);
	uv_timer_stop(timer_req);
	uv_stop(uv_default_loop());
	plua_gc();
}

static int ioctl_callback(int fd1, unsigned long req, void *cmd) {
	CuAssertTrue(gtc, fd > 0);
	if(req == TIOCMGET) {
		CuAssertIntEquals(gtc, 0, *((int *)cmd));
		foo++;
	}
	if(req == TIOCMSET) {
		CuAssertIntEquals(gtc, 6, *((int *)cmd));
		foo++;
	}
	return 0;
}

void test_lua_hardware_433nano_receive_invalid(CuTest *tc) {
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

	test_set_plua_path(tc, __FILE__, "lua_hardware_433nano_receive_invalid.c");

	plua_override_global("print", plua_print);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_hardware_433nano_receive_invalid.c", "", &file);

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

	wiringXIOCTLCallback(ioctl_callback);

	eventpool_init(EVENTPOOL_THREADED);
	node = eventpool_callback(REASON_RECEIVED_PULSETRAIN+10000, listener, NULL);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_433nano.json", CONFIG_SETTINGS));
	plua_clear_state(state);

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
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 500);

	hardware_init();

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 0, config_read(state->L, "lua_hardware_433nano.json", CONFIG_HARDWARE));
	plua_clear_state(state);

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

	plua_clear_state(state);

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

	CuAssertIntEquals(tc, 0, check);
	CuAssertIntEquals(tc, 2, foo);
	CuAssertIntEquals(tc, 0, xfree());
}
