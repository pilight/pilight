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
static int loops = 0;
static int x = 0;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void *received(int reason, void *param, void *userdata) {
	struct reason_code_received_t *data = param;

	if(loops < 2) {
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
			break;
			case 11:
				CuAssertStrEquals(gtc, "{\"action\":\"shutdown\",\"media\":\"none\",\"server\":\"127.0.0.1\",\"port\":19090}", data->message);
				x = 0;
			break;
		}
	}

	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void write_cb(uv_write_t *req, int status) {
	CuAssertIntEquals(gtc, 0, status);
	FREE(req);
}

static void rewrite(uv_timer_t *timer_req) {
	uv_tcp_t *client_req = timer_req->data;
	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);	
	char buffer[BUFFER_SIZE];
	strcpy(buffer, commands[cmdnr++]);
	int len = strlen(buffer);
	uv_buf_t buf1 = uv_buf_init(buffer, len);

	int r = uv_write(write_req, (uv_stream_t *)client_req, &buf1, 1, write_cb);
	CuAssertIntEquals(gtc, 0, r);

	if(cmdnr == (sizeof(commands)/sizeof(commands[0]))) {
		uv_close((uv_handle_t *)client_req, close_cb);
		uv_timer_stop(timer_req);
	}
}

static void connection_cb(uv_stream_t *server_req, int status) {
	if(loops == 2) {
		uv_stop(uv_default_loop());
		return;
	}

	uv_tcp_t *client_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, client_req);
	CuAssertIntEquals(gtc, 0, status);
	uv_tcp_init(uv_default_loop(), client_req);

	int r = uv_accept(server_req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	uv_timer_t *timer_req = NULL;
	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	char msg[128];
	sprintf(msg, "%s%d", "- round ", ++loops);
	printf("[ %-48s ]\n", msg);
	fflush(stdout);

	cmdnr = 0;
	timer_req->data = client_req;
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))rewrite, 100, 100);	
}

static void test_protocols_api_xbmc(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	uv_tcp_t *server_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(tc, server_req);

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);
	
	struct sockaddr_in addr;
	int r = uv_ip4_addr("127.0.0.1", 19090, &addr);
	CuAssertIntEquals(tc, r, 0);

	uv_tcp_init(uv_default_loop(), server_req);
	uv_tcp_bind(server_req, (const struct sockaddr *)&addr, 0);

	r = uv_listen((uv_stream_t *)server_req, 128, connection_cb);
	CuAssertIntEquals(tc, r, 0);	

	xbmcInit();

	char *add = "{\"test\":{\"protocol\":[\"xbmc\"],\"id\":[{\"server\":\"127.0.0.1\",\"port\":19090}],\"action\":\"shutdown\",\"media\":\"none\"}}";

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

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