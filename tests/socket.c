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

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/socket.h"

#include "alltests.h"

static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;
uv_thread_t pth;
CuTest *gtc = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void async_close_cb(uv_async_t *handle) {
	socket_gc();
	whitelist_free();
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

static void test(void) {
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	socket_connect("127.0.0.1", 15001);
}

static void *socket_received(int reason, void *param) {
	struct reason_socket_received_t *data = param;
	CuAssertStrEquals(gtc, "{\"foo\":\"bar\"}", data->buffer);

	return NULL;
}

static void *socket_disconnected(int reason, void *param) {
	struct reason_socket_disconnected_t *data = param;

	CuAssertTrue(gtc, (data->fd > 0));

	return NULL;
}

static void *socket_connected(int reason, void *param) {
	struct reason_socket_connected_t *data = param;

	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	char buf[INET_ADDRSTRLEN+1];
	
	if(getpeername(data->fd, (struct sockaddr *)&addr, (socklen_t *)&socklen) == 0) {
		memset(&buf, '\0', INET_ADDRSTRLEN+1);
		inet_ntop(AF_INET, (void *)&(addr.sin_addr), buf, INET_ADDRSTRLEN+1);

		if(htons(addr.sin_port) == 15001) {
			int r = socket_write(data->fd, "{\"foo\":\"bar\"}", 4);
			CuAssertIntEquals(gtc, 15, r);

			r = socket_write(data->fd, "{\"foo\":\"bar\"}", 4);
			CuAssertIntEquals(gtc, 15, r);

			socket_close(data->fd);
		}
	}

	return NULL;
}

static void stop(void) {
	uv_async_send(async_close_req);
}

static void test_socket(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;
	
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 250, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_SOCKET_RECEIVED, socket_received);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_disconnected);
	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected);
	
	socket_start(15001);

	test();
	
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();
	
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_socket(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_socket);

	return suite;
}