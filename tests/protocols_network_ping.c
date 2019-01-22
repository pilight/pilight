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
#include "../libs/pilight/protocols/network/ping.h"

#include "alltests.h"

static int check = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void *stop(void *param) {
	uv_stop(uv_default_loop());
	return NULL;
}

static void *received(int reason, void *param, void *userdata) {
	struct reason_code_received_t *data = param;

	CuAssertStrEquals(gtc, "{\"ip\":\"127.0.0.1\",\"state\":\"connected\"}", data->message);
	check = 1;

	protocol_gc();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	/*
	 * The ping timeout is 1 second so this has to be slower
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1250, 0);

	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_protocols_network_ping(CuTest *tc) {
	if(geteuid() != 0) {
		printf("[ %-33s (requires root)]\n", __FUNCTION__);
		fflush(stdout);
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	pingInit();

	char *add = "{\"test\":{\"protocol\":[\"ping\"],\"id\":[{\"ip\":\"127.0.0.1\"}],\"state\":\"disconnected\",\"poll-interval\":1}}";

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

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_network_ping(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_network_ping);

	return suite;
}