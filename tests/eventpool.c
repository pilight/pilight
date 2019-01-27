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
// #include <unistd.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "../libs/libuv/uv.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/eventpool.h"

static uv_thread_t pth;
static CuTest *gtc = NULL;
static int threads = EVENTPOOL_NO_THREADS;

struct data_t {
	int a;
} data_t;

static uv_async_t *async_close_req = NULL;
static int check = 0;
static int i = 0;
static int x = 0;
static int y = 0;
static void *foo = NULL;
static struct eventpool_listener_t *node[2] = { NULL };

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	FREE(param);
	return NULL;
}

static void *listener1(int reason, void *param, void *userdata) {
	CuAssertPtrNotNull(gtc, param);
	struct data_t *data = param;

	if(reason == 1) {
		CuAssertTrue(gtc, userdata == foo);
	}
	CuAssertTrue(gtc, (data->a >= 0 && data->a <= 5));
#ifdef _WIN32
	InterlockedIncrement(&check);
#else
	__sync_add_and_fetch(&check, 1);
#endif
	if(data->a == 5) {
		uv_async_send(async_close_req);
	}
	return NULL;
}

static void *listener2(int reason, void *param, void *userdata) {
	CuAssertPtrNotNull(gtc, param);
	struct data_t *data = param;
	CuAssertIntEquals(gtc, 4, data->a);
#ifdef _WIN32
	InterlockedIncrement(&check);
#else
	__sync_add_and_fetch(&check, 1);
#endif

	return NULL;
}

static void *listener3(int reason, void *param, void *userdata) {
	CuAssertPtrNotNull(gtc, param);
	struct data_t *data = param;
	CuAssertIntEquals(gtc, 5, data->a);

	struct data_t *tmp = MALLOC(sizeof(struct data_t));
	if(tmp == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	tmp->a = data->a;
	eventpool_trigger(1, done, tmp);
#ifdef _WIN32
	InterlockedIncrement(&check);
#else
	__sync_add_and_fetch(&check, 1);
#endif

	eventpool_callback_remove(node[0]);
	eventpool_callback_remove(node[1]);
	eventpool_trigger(2, NULL, NULL);

	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void loop(void *param) {
	i = 0, x = 0,	y = 0, check = 0;

	for(i=0;i<6;i++) {
		struct data_t *tmp = MALLOC(sizeof(struct data_t));
		if(tmp == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		tmp->a = i;
		switch(i) {
			case 0:
				eventpool_trigger(1, done, tmp);
			break;
			case 1:
				eventpool_trigger(1, done, tmp);
			break;
			case 2: {
				eventpool_trigger(1, done, tmp);
			} break;
			case 3: {
				eventpool_trigger(1, done, tmp);
			} break;
			case 4: {
				eventpool_trigger(2, done, tmp);
			} break;
			case 5: {
				eventpool_trigger(3, done, tmp);
			} break;
		}
	}
	return;
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_stop(uv_default_loop());
}

static void test_callback(CuTest *tc) {
	gtc = tc;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	foo = MALLOC(0);
	CuAssertPtrNotNull(tc, foo);
	eventpool_init(threads);
	eventpool_callback(1, listener1, foo);

	node[0] = eventpool_callback(2, listener1, NULL);
	node[1] = eventpool_callback(2, listener2, NULL);

	eventpool_callback(3, listener3, NULL);

	// pthread_create(&pth, NULL, loop, NULL);
	uv_thread_create(&pth, loop, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth);

	eventpool_gc();
	FREE(foo);

	CuAssertIntEquals(tc, 8, check);
	CuAssertIntEquals(tc, 0, xfree());

	/*
	 * The next test should be threaded
	 */
	threads = EVENTPOOL_THREADED;
}

static void test_eventpool_callback_nothreads(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	test_callback(tc);
}

static void test_eventpool_callback_threaded(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	test_callback(tc);
}

CuSuite *suite_eventpool(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_eventpool_callback_nothreads);
	SUITE_ADD_TEST(suite, test_eventpool_callback_threaded);

	return suite;
}