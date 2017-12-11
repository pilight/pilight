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
#ifndef _WIN32
	#include <unistd.h>
#endif
#include <mbedtls/sha256.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/webserver.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/http.h"
#include "../libs/pilight/core/ssl.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/socket.h"

#include "alltests.h"

#define CORE				0

static int run = CORE;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static int check = 0;
static uv_async_t *async_close_req = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void stop(uv_timer_t *req) {
	uv_async_send(async_close_req);
}

static void async_close_cb(uv_async_t *handle) {
	webserver_gc();
	whitelist_free();
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_stop(uv_default_loop());
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void on_read(int fd, char *buf, ssize_t len, char **buf1, ssize_t *len1) {
	if(strstr(buf, "404 Not Found") != NULL) {
		check = 1;
	}

	return;
}

static void *socket_connected(int reason, void *param) {
	struct reason_socket_connected_t *data = param;

	char *out = "{\"action\": \"identify\", \"options\": {\"core\": 0, \"receiver\": 1, \"config\": 0, \"forward\": 0}}";
	socket_write(data->fd, out);

	return NULL;
}


static void test_webserver(CuTest *tc) {
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("webserver.json", CONFIG_SETTINGS));

	ssl_init();

	if(run == CORE) {
		eventpool_init(EVENTPOOL_NO_THREADS);
	}
	webserver_start();

	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected);
	socket_connect("127.0.0.1", 10080, on_read);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	ssl_gc();
	storage_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_webserver_core_api(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	FILE *f = fopen("webserver.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"../res/pilight.pem\",\"webserver-root\":\"../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	gtc = tc;
	check = 0;

	run = CORE;
	test_webserver(tc);
}

CuSuite *suite_webserver_rest(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_webserver_core_api);

	return suite;
}
