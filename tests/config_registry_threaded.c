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
#include <math.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/binary.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/config/registry.h"

#include "alltests.h"

#include <wiringx.h>

#define max(a, b) \
 ({ __typeof__ (a) _a = (a); \
		 __typeof__ (b) _b = (b); \
	 _a > _b ? _a : _b; })

#define min(a, b) \
 ({ __typeof__ (a) _a = (a); \
		 __typeof__ (b) _b = (b); \
	 _a < _b ? _a : _b; })

static int loop = 1;
static int run1 = 0;
static int run2 = 0;
static int run3 = 0;
static uv_thread_t pth[3];
static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;

static void thread1(void *param) {
	while(loop) {
		struct lua_state_t *state = plua_get_free_state();
		config_registry_set_null(state->L, "pilight.version");
		config_registry_set_string(state->L, "pilight.version.thread1", "8.1.1");
		assert(plua_check_stack(state->L, 0) == 0);
		plua_clear_state(state);
		run1++;
	}
}

static void thread2(void *param) {
	while(loop) {
		struct lua_state_t *state = plua_get_free_state();
		config_registry_set_null(state->L, "pilight.version");
		config_registry_set_string(state->L, "pilight.version.thread2", "8.1.2");
		assert(plua_check_stack(state->L, 0) == 0);
		plua_clear_state(state);
		run2++;
	}
}

static void thread3(void *param) {
	while(loop) {
		struct lua_state_t *state = plua_get_free_state();
		config_registry_set_null(state->L, "pilight.version");
		config_registry_set_string(state->L, "pilight.version.thread2", "8.1.3");
		assert(plua_check_stack(state->L, 0) == 0);
		plua_clear_state(state);
		run3++;
	}
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);

	uv_stop(uv_default_loop());
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void stop(uv_timer_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);
	loop = 0;
	uv_stop(uv_default_loop());
}

void test_config_registry_threaded(CuTest *tc) {
	memtrack();
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("config_registry_threaded.c", "", &file);

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	plua_init();

	test_set_plua_path(tc, __FILE__, "config_registry_threaded.c");

	config_init();

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";

	FILE *f = fopen("storage_core.json", "w");
	fprintf(f, config, "");
	fclose(f);

	struct lua_state_t *state = plua_get_free_state();
	CuAssertIntEquals(tc, 0, config_read(state->L, "storage_core.json", CONFIG_REGISTRY));
	plua_clear_state(state);

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
	 * timeout of the COAP library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	uv_thread_create(&pth[0], thread1, NULL);
	uv_thread_create(&pth[1], thread2, NULL);
	uv_thread_create(&pth[2], thread3, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	CuAssertTrue(tc, run1 > 3000 && run2 > 3000 && fabs((double)min(run1, run2)/(double)max(run2, run1)) > 0.7);
	CuAssertTrue(tc, run2 > 3000 && run3 > 3000 && fabs((double)min(run2, run3)/(double)max(run3, run2)) > 0.7);
	CuAssertTrue(tc, run1 > 3000 && run3 > 3000 && fabs((double)min(run1, run3)/(double)max(run3, run1)) > 0.7);
	config_write();
	config_gc();
	plua_gc();
	wiringXGC();

	FREE(file);

	CuAssertIntEquals(tc, 0, xfree());
}

