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

#define CLIENT	0
#define SERVER	1

static int check = 0;
static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;
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

static void *socket_received(int reason, void *param) {
	struct reason_socket_received_t *data = param;
	if(data->endpoint == SOCKET_SERVER) {
		CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"type\":\"server\"}", data->buffer);
	} else {
		CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"type\":\"client\"}", data->buffer);
	}
	check++;

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
			int r = socket_write(data->fd, "{\"foo\":\"bar\",\"type\":\"server\"}");
			CuAssertIntEquals(gtc, 31, r);

			r = socket_write(data->fd, "{\"foo\":\"bar\",\"type\":\"server\"}");
			CuAssertIntEquals(gtc, 31, r);
		} else {
			int r = socket_write(data->fd, "{\"foo\":\"bar\",\"type\":\"client\"}");
			CuAssertIntEquals(gtc, 31, r);

			r = socket_write(data->fd, "{\"foo\":\"bar\",\"type\":\"client\"}");
			CuAssertIntEquals(gtc, 31, r);
		}
	}

	return NULL;
}

static void stop(void) {
	uv_async_send(async_close_req);
}

static void alloc_cb(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	if((buf->base = malloc(len)) == NULL) {
		OUT_OF_MEMORY
	}
	memset(buf->base, 0, len);
}

static void on_read(uv_stream_t *server, ssize_t nread, const uv_buf_t *buf) {
	char *data = NULL;
	size_t size = 0;
	if(socket_recv(buf->base, nread, &data, &size) > 0) {
		if(strstr(data, "\n") != NULL) {
			char **array = NULL;
			int n = explode(data, "\n", &array), i = 0;
			for(i=0;i<n;i++) {
				CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"type\":\"client\"}", array[i]);
				check++;
			}
			array_free(&array, n);
			FREE(data);
		} else {
			CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"type\":\"client\"}", buf->base);
			check++;
		}
	}
}

static void on_connect(uv_connect_t *connect_req, int status) {
	uv_read_start(connect_req->handle, alloc_cb, on_read);
	FREE(connect_req);
}

static void test_socket_client(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	check = 0;

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

	/*
	 * Bypass socket_connect
	 */
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	struct sockaddr_in addr;
	int r = 0;

	uv_tcp_t *tcp_req = MALLOC(sizeof(uv_tcp_t));
	if(tcp_req == NULL) {
		OUT_OF_MEMORY
	}

	r = uv_tcp_init(uv_default_loop(), tcp_req);
	CuAssertIntEquals(tc, 0, r);

	r = uv_ip4_addr("127.0.0.1", 15001, &addr);
	CuAssertIntEquals(tc, 0, r);

	uv_connect_t *connect_req = MALLOC(sizeof(uv_connect_t));
	CuAssertPtrNotNull(tc, connect_req);

	r = uv_tcp_connect(connect_req, tcp_req, (struct sockaddr *)&addr, on_connect);
	CuAssertIntEquals(tc, 0, r);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();

	CuAssertIntEquals(tc, 2, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_socket_server(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	check = 0;

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

	socket_connect("127.0.0.1", 15001);
	
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();
	
	CuAssertIntEquals(tc, 4, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_socket(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_socket_client);
	SUITE_ADD_TEST(suite, test_socket_server);

	return suite;
}