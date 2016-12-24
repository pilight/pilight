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

#include "../libs/libuv/uv.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/log.h"

#include "alltests.h"

static uv_async_t *async_close_req = NULL;
static uv_thread_t pth;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
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
	uv_stop(uv_default_loop());
}

static void test(void *param) {
	logprintf(LOG_EMERG, "emergency");
	logprintf(LOG_ALERT, "alert");
	logprintf(LOG_CRIT, "critical");
	logprintf(LOG_ERR, "test");
	logprintf(LOG_WARNING, "test");
	logprintf(LOG_NOTICE, "test");
	logprintf(LOG_INFO, "test");
	logprintf(LOG_DEBUG, "test");

	uv_async_send(async_close_req);
	return;
}

static void test_log(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	eventpool_init(EVENTPOOL_NO_THREADS);

	log_init();
	log_file_set("test.log");
	log_level_set(LOG_DEBUG);
	log_file_enable();
	log_shell_enable();

	uv_thread_create(&pth, test, NULL);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}	

	uv_thread_join(&pth);

	char *tmp = NULL, **array = NULL;
	int n = file_get_contents("test.log", &tmp), x = 0, i =0;
	CuAssertIntEquals(tc, 0, n);
	x = explode(tmp, "\n", &array);
	CuAssertIntEquals(tc, 7, x);

	for(i=0;i<x;i++) {
		if(i == 0) {
			CuAssertPtrNotNull(tc, strstr(array[i], "emergency"));
		} else if(i == 1) {
			CuAssertPtrNotNull(tc, strstr(array[i], "alert"));
		} else if(i == 2) {
			CuAssertPtrNotNull(tc, strstr(array[i], "critical"));
		} else if(i == 3) {
			CuAssertPtrNotNull(tc, strstr(array[i], "ERROR: test"));
		} else if(i == 4) {
			CuAssertPtrNotNull(tc, strstr(array[i], "WARNING: test"));
		} else if(i == 5) {
			CuAssertPtrNotNull(tc, strstr(array[i], "NOTICE: test"));
		} else if(i == 6) {
			CuAssertPtrNotNull(tc, strstr(array[i], "INFO: test"));
		} else if(i == 7) {
			CuAssertPtrNotNull(tc, strstr(array[i], "DEBUG: test"));
		}
	}
	FREE(tmp);

	array_free(&array, x);
	
	log_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_log(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_log);

	return suite;
}