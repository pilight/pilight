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

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/ntp.h"
#include "../libs/pilight/core/log.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"

static uv_thread_t pth;
static int ntp_socket = 0;
static int ntp_loop = 1;

static char request[48] = {
	0xe3, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static char response[48] = { 
	0x24, 0x01, 0x03, 0xe9,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1d,
	0x50, 0x50, 0x53, 0x00,
	0xdb, 0xfc, 0x2f, 0x6b,
	0xbc, 0xe3, 0x87, 0x94,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xdb, 0xfc, 0x2f, 0x79,
	0xc6, 0x5c, 0x66, 0xe8,
	0xdb, 0xfc, 0x2f, 0x79,
	0xc6, 0x5e, 0x6a, 0x6e
};

static CuTest *gtc = NULL;
static int steps = 0;

static void callback(int status, time_t ntptime) {
	steps++;
	if(steps == 1) {
		CuAssertULongEquals(gtc, 0, (unsigned long)ntptime);
	}
	if(steps == 2) {
		CuAssertULongEquals(gtc, 1481748729, (unsigned long)ntptime);
		ntp_loop = 0;
		uv_stop(uv_default_loop());
	}
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void ntp_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	char *message = NULL;
	int n = 0, r = 0;
	socklen_t addrlen = sizeof(addr);
	fd_set fdsread;

	if((message = MALLOC(BUFFER_SIZE)) == NULL) {
		OUT_OF_MEMORY
	}

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(ntp_socket, FIONBIO, &on);
	assert(r >= 0);
#else
	long arg = fcntl(ntp_socket, F_GETFL, NULL);
	r = fcntl(ntp_socket, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, (r >= 0));
#endif

	while(ntp_loop) {
		FD_ZERO(&fdsread);
		FD_SET((unsigned long)ntp_socket, &fdsread);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		do {
			n = select(ntp_socket+1, &fdsread, NULL, NULL, &tv);
		} while(n == -1 && errno == EINTR && ntp_loop);

		if(n == 0) {
			continue;
		}
		if(ntp_loop == 0) {
			break;
		}
		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(FD_ISSET(ntp_socket, &fdsread)) {
				memset(message, '\0', BUFFER_SIZE);
				r = recvfrom(ntp_socket, message, BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
				CuAssertTrue(gtc, (r >= 0));

				CuAssertStrEquals(gtc, request, message);
				r = sendto(ntp_socket, response, 48, 0, (struct sockaddr *)&addr, addrlen);
				CuAssertIntEquals(gtc, 48, r);
				break;
			}
		}
	}

clear:
	FREE(message);
	return;
}

static void ntp_custom_server(void) {
	struct sockaddr_in addr;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	ntp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (ntp_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(10123);

	r = setsockopt(ntp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(ntp_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));
}

static void test_ntp(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	steps = 0;
	ntp_loop = 1;

	ntp_custom_server();
	
	uv_thread_create(&pth, ntp_wait, NULL);

	strcpy(ntp_servers.server[0].host, "0.0.0.0");
	ntp_servers.server[0].port = 10124;
	ntp_servers.callback = callback;

	strcpy(ntp_servers.server[1].host, "127.0.0.1");
	ntp_servers.server[1].port = 10123;

	ntp_servers.nrservers = 2;

	ntpsync();

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	uv_thread_join(&pth);

	if(ntp_socket > 0) {
#ifdef _WIN32
		closesocket(ntp_socket);
#else
		close(ntp_socket);
#endif
		ntp_socket = 0;
	}
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_ntp(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_ntp);

	return suite;
}