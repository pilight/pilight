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
#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/json.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/API/xbmc.h"

#include "alltests.h"

#define BUFSIZE 1024*1024

static int cmdnr = 0;
static char *commands[11] = {
	/* Play song */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnPlay\",\"params\":{\"data\":{\"item\":{\"id\":1234,\"type\":\"song\"},\"player\":{\"playerid\":0,\"speed\":1}},\"sender\":\"xbmc\"}}",
	/* Pause song */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnPause\",\"params\":{\"data\":{\"item\":{\"id\":1234,\"type\":\"song\"},\"player\":{\"playerid\":0,\"speed\":0}},\"sender\":\"xbmc\"}}",
	/* Stop song */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnStop\",\"params\":{\"data\":{\"end\":false,\"item\":{\"id\":1234,\"type\":\"song\"}},\"sender\":\"xbmc\"}}",
	/* Play movie */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnPlay\",\"params\":{\"data\":{\"item\":{\"id\":1234,\"type\":\"movie\"},\"player\":{\"playerid\":0,\"speed\":1}},\"sender\":\"xbmc\"}}",
	/* Pause movie */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnPause\",\"params\":{\"data\":{\"item\":{\"id\":1234,\"type\":\"movie\"},\"player\":{\"playerid\":0,\"speed\":0}},\"sender\":\"xbmc\"}}",
	/* Stop movie */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnStop\",\"params\":{\"data\":{\"end\":false,\"item\":{\"id\":1234,\"type\":\"movie\"}},\"sender\":\"xbmc\"}}",
	/* Play tv show */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnPlay\",\"params\":{\"data\":{\"item\":{\"id\":1234,\"type\":\"episode\"},\"player\":{\"playerid\":0,\"speed\":1}},\"sender\":\"xbmc\"}}",
	/* Pause tv show */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnPause\",\"params\":{\"data\":{\"item\":{\"id\":1234,\"type\":\"episode\"},\"player\":{\"playerid\":0,\"speed\":0}},\"sender\":\"xbmc\"}}",
	/* Stop tv show */
	"{\"jsonrpc\":\"2.0\",\"method\":\"Player.OnStop\",\"params\":{\"data\":{\"end\":false,\"item\":{\"id\":1234,\"type\":\"episode\"}},\"sender\":\"xbmc\"}}",
	/* Screensaver activated */
	"{\"jsonrpc\":\"2.0\",\"method\":\"GUI.OnScreensaverActivated\",\"params\":{\"data\":null,\"sender\":\"xbmc\"}}",
	/* Screensaver deactivated */
	"{\"jsonrpc\":\"2.0\",\"method\":\"GUI.OnScreensaverDeactivated\",\"params\":{\"data\":{\"shuttingdown\":false},\"sender\":\"xbmc\"}}"
};

static CuTest *gtc = NULL;
static uv_thread_t pth;
static int xbmc_client = 0;
static int xbmc_server = 0;
static int xbmc_loop = 1;
static int loops = 0;
static int doquit = 0;
static int x = 0;

static void start(int port);

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void *received(int reason, void *param) {
	struct reason_code_received_t *data = param;

	switch(x++) {
		case 0:
			CuAssertStrEquals(gtc, "{\"action\":\"play\",\"media\":\"song\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 1:
			CuAssertStrEquals(gtc, "{\"action\":\"pause\",\"media\":\"song\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 2:
		case 5:
		case 8:
			CuAssertStrEquals(gtc, "{\"action\":\"home\",\"media\":\"none\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 3:
			CuAssertStrEquals(gtc, "{\"action\":\"play\",\"media\":\"movie\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 4:
			CuAssertStrEquals(gtc, "{\"action\":\"pause\",\"media\":\"movie\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 6:
			CuAssertStrEquals(gtc, "{\"action\":\"play\",\"media\":\"episode\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 7:
			CuAssertStrEquals(gtc, "{\"action\":\"pause\",\"media\":\"episode\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 9:
			CuAssertStrEquals(gtc, "{\"action\":\"active\",\"media\":\"screensaver\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
		break;
		case 10:
			CuAssertStrEquals(gtc, "{\"action\":\"inactive\",\"media\":\"screensaver\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
			doquit = 1;
		break;
		case 11:
			CuAssertStrEquals(gtc, "{\"action\":\"shutdown\",\"media\":\"none\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
			x = 0;
		break;
	}

	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	char *message = NULL;
	int n = 0, r = 0, len = 0;
	int doread = 0, dowrite = 0;
	fd_set fdsread;
	fd_set fdswrite;

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(xbmc_server, FIONBIO, &on);
	CuAssertTrue(gtc, r >= 0);
#else
	long arg = fcntl(xbmc_server, F_GETFL, NULL);
	r = fcntl(xbmc_server, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, r >= 0);
#endif

	FD_ZERO(&fdsread);
	FD_ZERO(&fdswrite);

	while(xbmc_loop) {
		FD_SET((unsigned long)xbmc_server, &fdsread);
		tv.tv_sec = 0;
		tv.tv_usec = 1000;

		if(doquit == 2) {
			xbmc_loop = 0;
#ifdef _WIN32
			if(xbmc_client > 0) {
				closesocket(xbmc_client);
			}
			closesocket(xbmc_server);
#else
			if(xbmc_client > 0) {
				close(xbmc_client);
			}
			close(xbmc_server);
#endif
			xbmc_client = 0;
			xbmc_server = 0;

			uv_stop(uv_default_loop());
			break;
		} else if(doquit == 1) {
			cmdnr = 0;
			doquit = 0;
			loops++;

			char msg[128];
			sprintf(msg, "%s%d", "- round ", loops);
			printf("[ %-48s ]\n", msg);
			fflush(stdout);

#ifdef _WIN32
			closesocket(xbmc_client);
#else
			close(xbmc_client);
#endif
			xbmc_client = 0;
		}

		dowrite = 0;
		doread = 0;
		do {
			if(xbmc_client > xbmc_server) {
				n = select(xbmc_client+1, &fdsread, &fdswrite, NULL, &tv);
			} else {
				n = select(xbmc_server+1, &fdsread, &fdswrite, NULL, &tv);
			}
		} while(n == -1 && errno == EINTR && xbmc_loop);

		if(xbmc_loop == 0) {
			doquit = 2;
			xbmc_loop = 1;
		}

		if(n == 0 || doquit == 2) {
			if(xbmc_client > 0) {
				FD_SET((unsigned long)xbmc_client, &fdswrite);
			}
			continue;
		}

		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(FD_ISSET(xbmc_server, &fdsread)) {
				FD_ZERO(&fdsread);
				if(xbmc_client == 0) {
					xbmc_client = accept(xbmc_server, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
					CuAssertTrue(gtc, xbmc_client > 0);
					dowrite = 1;
				}
			}
			if(FD_ISSET(xbmc_client, &fdswrite)) {
				FD_ZERO(&fdswrite);
				if((message = MALLOC(BUFSIZE)) == NULL) {
					OUT_OF_MEMORY
				}
				/*
				 * Actual raw openweathermap http response
				 */
				strcpy(message, commands[cmdnr++]);
				len = strlen(message);

				r = send(xbmc_client, message, len, 0);
				CuAssertIntEquals(gtc, len, r);

				if(cmdnr < (sizeof(commands)/sizeof(commands[0]))) {
				} else {
					if(loops > 1) {
						doquit = 2;
					}
				}
				FREE(message);
			}
		}
	}

clear:
	return;
}

static void start(int port) {
	struct sockaddr_in addr;
	int opt = 1;
	int r = 0;

	x = 0;
	loops = 0;
	cmdnr = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	xbmc_server = socket(AF_INET, SOCK_STREAM, 0);
	CuAssertTrue(gtc, xbmc_server >= 0);

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	r = setsockopt(xbmc_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, r >= 0);

	r = bind(xbmc_server, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, r >= 0);

	r = listen(xbmc_server, 0);
	CuAssertTrue(gtc, r >= 0);
}

static void test_protocols_api_xbmc(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	start(19090);

	uv_thread_create(&pth, wait, NULL);

	xbmcInit();

	char *add = "{\"test\":{\"protocol\":[\"xbmc\"],\"id\":[{\"server\":\"127.0.0.1\",\"port\":19090}],\"action\":\"shutdown\",\"media\":\"none\"}}";

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth);

	eventpool_gc();
	protocol_gc();

	CuAssertIntEquals(tc, 2, loops);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_api_xbmc(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_api_xbmc);

	return suite;
}