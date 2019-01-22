/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*
 * Test coverage
 * 1. Socket server through socket library client connecting bypassing socket library.
 *    Can we send a message from the server to the client and back?
 * 2. Socket server and client connecting through socket library.
 *    Can we send messages back and forth?
 * 3. Socket server and client connecting through socket library.
 *    Can we send messages larger than buffer size back and forth?
 * 4. Socket server through socket library client connecting bypassing socket library.
 *    Due to whitelist limitations the client is rejected.
 *
 * In both cases, we are also testing if sending to a closed socket doesn't break.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/socket.h"

#include "alltests.h"
#include "gplv3.h"

#define CLIENT			0
#define SERVER			1
#define BIGCONTENT	2

static char sbuffer[65536+1];
static int client_fd = 0;
static int server_fd = 0;
static int check = 0;
static int reject = 0;
static int run = CLIENT;
static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;
static uv_tcp_t *client_req = NULL;
static char *data1 = NULL;
static ssize_t size1 = 0;
CuTest *gtc = NULL;

struct data_t {
	int fd;
} data_t;

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

void read_cb(int fd, char *buffer, ssize_t len, char **buffer1, ssize_t *len1) {
	if(run == SERVER) {
		if(socket_recv(buffer, len, buffer1, len1) > 0) {
			if(strstr(*buffer1, "\n") != NULL) {
				char **array = NULL;
				int n = explode(buffer, "\n", &array), i = 0;
				for(i=0;i<n;i++) {
					if(fd == client_fd) {
						CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"to\":\"client\"}", array[i]);
						check = 1;
					} else {
						CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"to\":\"server\"}", array[i]);
						check = 1;
					}
				}
				array_free(&array, n);
			} else {
				if(fd == client_fd) {
					CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"to\":\"client\"}", buffer);
					check = 1;
				} else {
					CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"to\":\"server\"}", buffer);
					check = 1;
				}
			}
			FREE(*buffer1);
			*len1 = 0;
		}
	} else if(run == BIGCONTENT) {
		if(socket_recv(buffer, len, buffer1, len1) > 0) {
			char *cpy = STRDUP(gplv3);
			str_replace(EOSS, "\n", &cpy);
			CuAssertTrue(gtc, strcmp(cpy, *buffer1) == 0);
			check = 1;
			FREE(cpy);
			FREE(*buffer1);
			*len1 = 0;
		}
	}

	if(run == SERVER && server_fd != fd) {
		socket_close(fd);
	}

	return;
}

static void *socket_disconnected(int reason, void *param, void *userdata) {
	struct reason_socket_disconnected_t *data = param;

	CuAssertTrue(gtc, (data->fd > 0));

	return NULL;
}

static void dosend(int fd) {
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	char buf[INET_ADDRSTRLEN+1];

	if(getpeername(fd, (struct sockaddr *)&addr, (socklen_t *)&socklen) == 0) {
		memset(&buf, '\0', INET_ADDRSTRLEN+1);
		uv_ip4_name((void *)&(addr.sin_addr), buf, INET_ADDRSTRLEN+1);

		if(run == SERVER || run == CLIENT) {
			if(htons(addr.sin_port) != 15001) {
				int r = socket_write(fd, "{\"foo\":\"bar\",\"to\":\"client\"}");
				CuAssertIntEquals(gtc, 29, r);

				r = socket_write(fd, "{\"foo\":\"bar\",\"to\":\"client\"}");
				CuAssertIntEquals(gtc, 29, r);
			} else {
				int r = socket_write(fd, "{\"foo\":\"bar\",\"to\":\"server\"}");
				CuAssertIntEquals(gtc, 29, r);

				r = socket_write(fd, "{\"foo\":\"bar\",\"to\":\"server\"}");
				CuAssertIntEquals(gtc, 29, r);
			}
		}
		if(run == BIGCONTENT) {
			if(htons(addr.sin_port) != 15001) {
				int r = socket_write(fd, gplv3);
				CuAssertIntEquals(gtc, 35148, r);
			}
		}
	}
}

static void resend(uv_timer_t *timer_req) {
	struct data_t *data = timer_req->data;
	dosend(data->fd);

	FREE(timer_req->data);
}

static void *socket_connected(int reason, void *param, void *userdata) {
	struct reason_socket_connected_t *data = param;

	dosend(data->fd);

	if(run == SERVER) {
		uv_timer_t *timer_req1 = MALLOC(sizeof(uv_timer_t));
		CuAssertPtrNotNull(gtc, timer_req1);

		struct data_t *data1 = MALLOC(sizeof(struct data_t));
		CuAssertPtrNotNull(gtc, data1);
		data1->fd = data->fd;
		timer_req1->data = data1;
		uv_timer_init(uv_default_loop(), timer_req1);
		uv_timer_start(timer_req1, (void (*)(uv_timer_t *))resend, 250, 0);
	}

	return NULL;
}

static void stop(void) {
	uv_async_send(async_close_req);
}

static void alloc_cb(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	buf->base = sbuffer;
	memset(buf->base, 0, len);
}

static void on_read(uv_stream_t *server_req, ssize_t nread, const uv_buf_t *buf) {
	uv_os_fd_t fd;

	if(nread < -1) {
		/*
		 * Disconnected by server
		 */
		reject = 1;
		return;
	}

	if(socket_recv(buf->base, nread, &data1, &size1) > 0) {
		if(strstr(data1, "\n") != NULL) {
			char **array = NULL;
			int n = explode(data1, "\n", &array), i = 0;
			for(i=0;i<n;i++) {
				CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"to\":\"client\"}", array[i]);
				check = 1;
			}
			array_free(&array, n);
		} else {
			CuAssertStrEquals(gtc, "{\"foo\":\"bar\",\"to\":\"client\"}", data1);
			check = 1;
		}
		FREE(data1);
		size1 = 0;
	}

	int r = uv_fileno((uv_handle_t *)server_req, &fd);
	CuAssertIntEquals(gtc, 0, r);

	// free(buf->base);

#ifdef _WIN32
	closesocket((SOCKET)fd);
#else
	close(fd);
#endif
}

static void on_connect(uv_connect_t *connect_req, int status) {
	CuAssertIntEquals(gtc, 0, status);
	uv_read_start(connect_req->handle, alloc_cb, on_read);
	FREE(connect_req);
}

static void client_connect(void) {
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

	client_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, client_req);

	r = uv_tcp_init(uv_default_loop(), client_req);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_ip4_addr("127.0.0.1", 15001, &addr);
	CuAssertIntEquals(gtc, 0, r);

	uv_connect_t *connect_req = MALLOC(sizeof(uv_connect_t));
	CuAssertPtrNotNull(gtc, connect_req);

	r = uv_tcp_connect(connect_req, client_req, (struct sockaddr *)&addr, on_connect);
	CuAssertIntEquals(gtc, 0, r);
}

static void test_socket_client(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	check = 0;
	run = CLIENT;

	memtrack();

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	async_close_req = MALLOC(sizeof(uv_async_t));
	CuAssertPtrNotNull(gtc, async_close_req);
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 0);

	eventpool_init(EVENTPOOL_THREADED);
	// eventpool_callback(REASON_SOCKET_RECEIVED, socket_received, NULL);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_disconnected, NULL);
	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected, NULL);

	server_fd = socket_start(15001, read_cb);

	client_connect();

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_socket_reject_client(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	check = 0;
	run = CLIENT;

	memtrack();

	gtc = tc;

	socket_override(1);

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	async_close_req = MALLOC(sizeof(uv_async_t));
	CuAssertPtrNotNull(gtc, async_close_req);
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 0);

	eventpool_init(EVENTPOOL_THREADED);
	// eventpool_callback(REASON_SOCKET_RECEIVED, socket_received, NULL);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_disconnected, NULL);
	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected, NULL);

	server_fd = socket_start(15001, read_cb);

	client_connect();

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();

	socket_override(-1);
	CuAssertIntEquals(tc, 1, reject);
	CuAssertIntEquals(tc, 0, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_socket_server(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	check = 0;
	run = SERVER;

	memtrack();

	gtc = tc;
	
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	async_close_req = MALLOC(sizeof(uv_async_t));
	CuAssertPtrNotNull(gtc, async_close_req);
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 0);

	eventpool_init(EVENTPOOL_THREADED);
	// eventpool_callback(REASON_SOCKET_RECEIVED, socket_received, NULL);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_disconnected, NULL);
	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected, NULL);
	
	server_fd = socket_start(15001, read_cb);
	client_fd = socket_connect("127.0.0.1", 15001, read_cb);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_socket_large_content(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	check = 0;
	run = BIGCONTENT;

	memtrack();

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	async_close_req = MALLOC(sizeof(uv_async_t));
	CuAssertPtrNotNull(gtc, async_close_req);
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 0);

	eventpool_init(EVENTPOOL_THREADED);
	// eventpool_callback(REASON_SOCKET_RECEIVED, socket_received, NULL);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_disconnected, NULL);
	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected, NULL);

	server_fd = socket_start(15001, read_cb);
	client_fd = socket_connect("127.0.0.1", 15001, read_cb);
	
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();
	
	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_socket(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_socket_client);
	SUITE_ADD_TEST(suite, test_socket_server);
	SUITE_ADD_TEST(suite, test_socket_large_content);
	SUITE_ADD_TEST(suite, test_socket_reject_client);

	return suite;
}