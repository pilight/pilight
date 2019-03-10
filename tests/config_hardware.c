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

static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void stop(uv_work_t *req) {
	uv_timer_stop(timer_req);
	uv_stop(uv_default_loop());
	plua_gc();
}

void test_config_hardware(CuTest *tc) {
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "config_hardware.c");

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("config_hardware.c", "", &file);

	config_init();

	char config[1024] =
		"{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\"\
		},\
		\"hardware\":{\"433nano\":{\"comport\":\"/tmp/usb0\"}},\
		\"registry\":{}}";

	FILE *f = fopen("storage_core.json", "w");
	fprintf(f, config, file);
	fclose(f);

	FREE(file);

	CuAssertIntEquals(tc, 0, config_read("storage_core.json", CONFIG_SETTINGS));

	unlink("/tmp/usb0");
	int fd = open("/tmp/usb0", O_CREAT | O_RDWR, 0777);
	close(fd);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 500);

	hardware_init();

	CuAssertIntEquals(tc, 0, config_read("storage_core.json", CONFIG_HARDWARE));

	{
		struct plua_metatable_t *config = config_get_metatable();
		char *comport = NULL;

		CuAssertIntEquals(tc, 0, plua_metatable_get_string(config, "hardware.433nano.comport", &comport));
		CuAssertStrEquals(tc, "/tmp/usb0", comport);
	}

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

	CuAssertIntEquals(tc, 0, xfree());
}
