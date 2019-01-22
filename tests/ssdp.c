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
#else
	#include <ws2tcpip.h>
#endif

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/ssdp.h"
#include "../libs/pilight/lua_c/lua.h"
#include "alltests.h"

static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;
static uv_thread_t pth;
static int ssdp_socket = 0;
static int ssdp_loop = 1;
static int check = 0;
static int run = 0;
static CuTest *gtc = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);

	ssdp_gc();
	uv_stop(uv_default_loop());
}

static void ssdp_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	char message[BUFFER_SIZE], header[BUFFER_SIZE];
	int n = 0, r = 0;
	ssize_t len = 0;
	socklen_t addrlen = sizeof(addr);
	fd_set fdsread;

	len = sprintf(header, "NOTIFY * HTTP/1.1\r\n"
		"Host:239.255.255.250:1900\r\n"
		"Cache-Control:max-age=900\r\n"
		"Location:123.123.123.123:1234\r\n"
		"NT:urn:schemas-upnp-org:service:pilight:1\r\n"
		"USN:uuid:0000-00-00-00-000000::urn:schemas-upnp-org:service:pilight:1\r\n"
		"NTS:ssdp:alive\r\n"
		"SERVER: Test UPnP/1.1 pilight (Test)/1.0\r\n\r\n");
	CuAssertIntEquals(gtc, 278, len);

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(ssdp_socket, FIONBIO, &on);
	assert(r >= 0);
#else
	long arg = fcntl(ssdp_socket, F_GETFL, NULL);
	r = fcntl(ssdp_socket, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, (r >= 0));
#endif

	while(ssdp_loop) {
		FD_ZERO(&fdsread);
		FD_SET((unsigned long)ssdp_socket, &fdsread);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		do {
			n = select(ssdp_socket+1, &fdsread, NULL, NULL, &tv);
		} while(n == -1 && errno == EINTR && ssdp_loop);

		if(n == 0) {
			continue;
		}
		if(ssdp_loop == 0) {
			break;
		}
		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(FD_ISSET(ssdp_socket, &fdsread)) {
				memset(message, '\0', BUFFER_SIZE);
				r = recvfrom(ssdp_socket, message, sizeof(message), 0, (struct sockaddr *)&addr, &addrlen);
				CuAssertTrue(gtc, (r >= 0));

				if(strstr(message, "M-SEARCH * HTTP/1.1") > 0 && strstr(message, "urn:schemas-upnp-org:service:pilight:1") > 0) {
					r = sendto(ssdp_socket, header, len, 0, (struct sockaddr *)&addr, sizeof(addr));
					CuAssertIntEquals(gtc, len, r);
				}
			}
		}
	}

clear:
	return;
}

static void ssdp_custom_server(void) {
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	ssdp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (ssdp_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(1900);

	r = setsockopt(ssdp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	inet_pton(AF_INET, "239.255.255.250", &mreq.imr_multiaddr.s_addr);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	r = setsockopt(ssdp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(ssdp_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void *ssdp_found(int reason, void *param, void *userdata) {
	struct reason_ssdp_received_t *data = param;
	if(strlen(data->ip) > 0 && data->port > 0 && strlen(data->name) > 0) {
		if(strcmp(data->ip, "123.123.123.123") == 0 && data->port == 1234 && strcmp(data->name, "Test") == 0) {
			check = 1;
		}
	}
	return NULL;
}

static void stop(void) {
	uv_async_send(async_close_req);
}

static void test_ssdp_client(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	run = 0;
	check = 0;

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	ssdp_custom_server();

	uv_thread_create(&pth, ssdp_wait, NULL);

	ssdp_seek();

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
	 * timeout of the SSDP library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);	

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found, NULL);	

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	ssdp_loop = 0;
	uv_thread_join(&pth);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	if(ssdp_socket > 0) {
#ifdef _WIN32
		closesocket(ssdp_socket);
#else
		close(ssdp_socket);
#endif
		ssdp_socket = 0;
	}	
	
	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_ssdp_server(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	run = 1;
	check = 0;

	gtc = tc;

	plua_init();

	test_set_plua_path(tc, __FILE__, "ssdp.c");

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("ssdp.json", CONFIG_SETTINGS));	

	ssdp_override("123.123.123.123", 1234);

	ssdp_start();
	ssdp_seek();

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
	 * timeout of the SSDP library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);	

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	ssdp_override(NULL, -1);
	eventpool_gc();
	storage_gc();
	plua_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_ssdp(void) {	
	CuSuite *suite = CuSuiteNew();

	FILE *f = fopen("ssdp.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"name\":\"Test\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);
	
	SUITE_ADD_TEST(suite, test_ssdp_client);
	SUITE_ADD_TEST(suite, test_ssdp_server);

	return suite;
}