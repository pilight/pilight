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
#include "../libs/pilight/protocols/API/lirc.h"

#include "alltests.h"

static char *keys[2] = {
	"000000037ff07bde 1%d KEY_ARROWLEFT logitech",
	"000000037ff07bdf 1%d KEY_ARROWRIGHT logitech"
};

static int i = 0;
static int x = 0;
static int loop = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;
static uv_pipe_t *pipe_req = NULL;

static void start(void);

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void *received(int reason, void *param, void *userdata) {
	struct reason_code_received_t *data = param;

	x++;
	if(x % 2 == 0) {
		CuAssertStrEquals(gtc, "{\"code\":\"000000037ff07bde\",\"repeat\":1,\"button\":\"KEY_ARROWLEFT\",\"remote\":\"logitech\"}", data->message);
	} else {
		CuAssertStrEquals(gtc, "{\"code\":\"000000037ff07bdf\",\"repeat\":1,\"button\":\"KEY_ARROWRIGHT\",\"remote\":\"logitech\"}", data->message);
	}

	if(x > 10) {
		uv_stream_t *client_req = timer_req->data;
		uv_timer_stop(timer_req);
		uv_close((uv_handle_t *)pipe_req, close_cb);
		uv_close((uv_handle_t *)client_req, close_cb);
		
		uv_fs_t req;
#ifdef __FreeBSD__
		uv_fs_unlink(uv_default_loop(), &req, "/tmp/lircd", NULL);
#else
		uv_fs_unlink(uv_default_loop(), &req, "/dev/lircd", NULL);
#endif

		if(loop < 2) {
			loop++;

			char msg[128];
			sprintf(msg, "%s%d", "- round ", loop);
			printf("[ %-48s ]\n", msg);
			fflush(stdout);

			start();
		} else {
			uv_stop(uv_default_loop());
		}
	}
	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

void write_cb(uv_write_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);
	FREE(req);
}

static void timer_cb(void *param) {
	char *p = NULL;

	uv_timer_t *timer_req = param;
	uv_stream_t *client_req = timer_req->data;
	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	i++;
	p = keys[i % 2];

	uv_buf_t buf = uv_buf_init(p, strlen(p));
	int r = uv_write((uv_write_t *)write_req, (uv_stream_t *)client_req, &buf, 1, write_cb);	
	CuAssertIntEquals(gtc, 0, r);
}

static void connect_cb(uv_stream_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);

	i = 0;
	x = 0;
	uv_pipe_t *client_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, client_req);

	int r = uv_pipe_init(uv_default_loop(), client_req, 0);

	r = uv_accept(req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);

	timer_req->data = client_req;
	r = uv_timer_init(uv_default_loop(), timer_req);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_timer_start(timer_req, (void (*)(uv_timer_t *))timer_cb, 10, 10);	
	CuAssertIntEquals(gtc, 0, r);

	return;
}

static void start(void) {
	int r = 0;

	pipe_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, pipe_req);

	r = uv_pipe_init(uv_default_loop(), pipe_req, 1);
	CuAssertIntEquals(gtc, 0, r);

	uv_fs_t file_req;

#ifdef __FreeBSD__
	uv_fs_unlink(uv_default_loop(), &file_req, "/tmp/lircd", NULL);
	r = uv_pipe_bind(pipe_req, "/tmp/lircd");
#else
	uv_fs_unlink(uv_default_loop(), &file_req, "/dev/lircd", NULL);
	r = uv_pipe_bind(pipe_req, "/dev/lircd");
#endif
	CuAssertIntEquals(gtc, 0, r);

	r = uv_listen((uv_stream_t *)pipe_req, 128, connect_cb);
	CuAssertIntEquals(gtc, 0, r);	
}

static void test_protocols_api_lirc(CuTest *tc) {
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

	loop = 0;

	start();

#ifdef __FreeBSD__
	setenv("PILIGHT_LIRC_DEV", "/tmp/lircd", 1);
#endif
	lircInit();

	char *add = "{\"test\":{\"protocol\":[\"lirc\"],\"id\":[{\"remote\":\"logitech\"}],\"code\":\"000000037ff07bde\",\"repeat\":1,\"button\":\"KEY_ARROWLEFT\"}}";
	
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

	CuAssertIntEquals(tc, 2, loop);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_api_lirc(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_api_lirc);

	return suite;
}