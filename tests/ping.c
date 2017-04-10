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
#include "../libs/pilight/core/ping.h"
#include "../libs/pilight/core/log.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"

#define LOCALHOST	0
#define TIMEOUT 	1
#define RESPONSE	2

static int run = LOCALHOST;
static uv_thread_t pth;
static uv_thread_t pth1;
static int ping_socket = 0;
static int ping_loop = 1;
static int threaded = 0;
static int started = 0;

/*
 * A ICMP request to 127.0.0.1
 */
static char request[40] = {
	0x45, 0x00, 0x00, 0x28,
	0x10, 0xe1, 0x00, 0x00,
	0xff, 0x01, 0xac, 0xf1,
	0x7f, 0x00, 0x00, 0x01,
	0x7f, 0x00, 0x00, 0x01,
	0x08, 0x00, 0xf6, 0xff,
	0x00, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

/*
 * A ICMP response from 123.123.123.123
 */
static char response[40] = { 
	0x45, 0x00, 0x00, 0x28,
	0xd4, 0x8b, 0x00, 0x00,
	0x40, 0x01, 0x90, 0xe9,
	0x7b, 0x7b, 0x7b, 0x7b,
	0x0a, 0x00, 0x00, 0xd4,
	0x00, 0x00, 0xfe, 0xff,
	0x00, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static CuTest *gtc = NULL;

static void callback(char *ip, int status) {
	if(run == LOCALHOST) {
		CuAssertStrEquals(gtc, "127.0.0.1", ip);
		CuAssertIntEquals(gtc, 0, status);
	} else if(run == TIMEOUT) {
		CuAssertStrEquals(gtc, "123.123.123.123", ip);
		CuAssertIntEquals(gtc, -1, status);
	} else if(run == RESPONSE) {
		CuAssertStrEquals(gtc, "123.123.123.123", ip);
		CuAssertIntEquals(gtc, 0, status);
	}
	ping_loop = 0;
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


static void ping_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	char *message = NULL;
	int n = 0, r = 0;
	socklen_t addrlen = sizeof(addr);
	fd_set fdsread;
	fd_set fdswrite;

	if((message = MALLOC(BUFFER_SIZE)) == NULL) {
		OUT_OF_MEMORY
	}

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(ping_socket, FIONBIO, &on);
	assert(r >= 0);
#else
	long arg = fcntl(ping_socket, F_GETFL, NULL);
	r = fcntl(ping_socket, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, (r >= 0));
#endif

	while(ping_loop) {
		FD_ZERO(&fdsread);
		FD_ZERO(&fdswrite);
		if(run == LOCALHOST) {
			FD_SET((unsigned long)ping_socket, &fdsread);
		} else if(run == RESPONSE) {
			FD_SET((unsigned long)ping_socket, &fdswrite);
		}

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		do {
			n = select(ping_socket+1, &fdsread, &fdswrite, NULL, &tv);
		} while(n == -1 && errno == EINTR && ping_loop);

		if(n == 0) {
			continue;
		}
		if(ping_loop == 0) {
			break;
		}
		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(run == LOCALHOST) {
				if(FD_ISSET(ping_socket, &fdsread)) {
					memset(message, '\0', BUFFER_SIZE);
					r = recvfrom(ping_socket, message, BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
					CuAssertIntEquals(gtc, 40, r);
					CuAssertStrEquals(gtc, request, message);
					break;
				}
			}
			if(run == RESPONSE) {
				if(FD_ISSET(ping_socket, &fdswrite)) {
					struct sockaddr_in dst;
					memset(&dst, 0, sizeof(struct sockaddr));
					dst.sin_family = AF_INET;
					inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);
					dst.sin_port = htons(0);

					r = sendto(ping_socket, response, 40, 0, (struct sockaddr *)&dst, sizeof(dst));
					CuAssertIntEquals(gtc, 40, r);
					break;
				}
			}
		}
	}

clear:
#ifdef _WIN32
	closesocket(ping_socket);
#else
	close(ping_socket);
#endif
	FREE(message);
	return;
}

static void ping_custom_server(void) {
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

	ping_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	CuAssertTrue(gtc, (ping_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(0);

	r = setsockopt(ping_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

#ifdef __FreeBSD__
	int off = 0;
	r = setsockopt(ping_socket, IPPROTO_IP, IP_HDRINCL, (const char *)&off, sizeof(off));
#else	
	r = setsockopt(ping_socket, IPPROTO_IP, IP_HDRINCL, (const char *)&opt, sizeof(opt));
#endif
	CuAssertTrue(gtc, (r >= 0));
	
	r = bind(ping_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));
}

static void thread(void *param) {
	struct ping_list_t *list = NULL;
	if(run == LOCALHOST) {
		ping_add_host(&list, "127.0.0.1");
	} else if(run == TIMEOUT || run == RESPONSE) {
		ping_add_host(&list, "123.123.123.123");
	}
	ping(list, callback);

#ifndef _WIN32
	if(run == LOCALHOST || run == RESPONSE) {
		uv_thread_create(&pth, ping_wait, NULL);
	}
#endif
	started = 1;
}

static void test_ping(CuTest *tc) {
	gtc = tc;

	ping_loop = 1;
	started = 0;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

#ifndef _WIN32
	ping_custom_server();
#endif

	if(threaded == 1) {
		uv_thread_create(&pth1, thread, NULL);
	} else {
		thread(NULL);
	}

#ifdef _WIN32
	SleepEx(1000, TRUE);
#else
	while(started == 0) {
		usleep(10);
	}
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);
	
	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	if(run == LOCALHOST || run == RESPONSE) {
		uv_thread_join(&pth);
	}
	if(threaded == 1) {
		uv_thread_join(&pth1);
	}
#endif

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_ping_localhost(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	run = LOCALHOST;

	test_ping(tc);
}

static void test_ping_localhost_threaded(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	run = LOCALHOST;
	threaded = 1;

	test_ping(tc);
}

static void test_ping_timeout(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	run = TIMEOUT;

	test_ping(tc);
}

static void test_ping_timeout_threaded(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	run = TIMEOUT;
	threaded = 1;

	test_ping(tc);
}

#if !defined(_WIN32) && !defined(__FreeBSD__)
static void test_ping_response(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	gtc = tc;

	run = RESPONSE;

	test_ping(tc);
}

static void test_ping_response_threaded(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	gtc = tc;

	run = RESPONSE;
	threaded = 1;

	test_ping(tc);
}
#endif

CuSuite *suite_ping(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_ping_localhost);
	SUITE_ADD_TEST(suite, test_ping_timeout);
#if !defined(_WIN32) && !defined(__FreeBSD__)
	SUITE_ADD_TEST(suite, test_ping_response);
#endif

#ifndef _WIN32
	SUITE_ADD_TEST(suite, test_ping_localhost_threaded);
	SUITE_ADD_TEST(suite, test_ping_timeout_threaded);
#ifndef __FreeBSD__
	SUITE_ADD_TEST(suite, test_ping_response_threaded);
#endif
#endif

	return suite;
}