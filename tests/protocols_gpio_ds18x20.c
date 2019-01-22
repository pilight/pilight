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
#include "../libs/pilight/protocols/GPIO/ds18b20.h"
#include "../libs/pilight/protocols/GPIO/ds18s20.h"

#include "alltests.h"

#define DS18S20 0
#define DS18B20 1

static int run = 0;
static int check = 0;
static CuTest *gtc = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void *received(int reason, void *param, void *userdata) {
	struct reason_code_received_t *data = param;

	switch(run) {
		case DS18B20:
			CuAssertStrEquals(gtc, "{\"id\":\"0000052ba3ac\",\"temperature\":26.625}", data->message);
		break;
		case DS18S20:
			CuAssertStrEquals(gtc, "{\"id\":\"0000052ba3ac\",\"temperature\":26.6}", data->message);
		break;
	}
	check = 1;

	uv_stop(uv_default_loop());

	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_protocols_gpio_ds18s20(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	run = DS18S20;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	setenv("PILIGHT_DS18S20_PATH", "/tmp/", 1);

	ds18s20Init();

	char *add = "{\"test\":{\"protocol\":[\"ds18s20\"],\"id\":[{\"id\":\"0000052ba3ac\"}],\"temperature\":19.562,\"poll-interval\":1}}";

	unlink("/tmp/10-0000052ba3ac/w1_slave");
	rmdir("/tmp/10-0000052ba3ac");
	mkdir("/tmp/10-0000052ba3ac", 0777);

	FILE *fp = fopen("/tmp/10-0000052ba3ac/w1_slave", "w+");
  fprintf(fp, "aa 01 4b 46 7f ff 06 10 84 : crc=84 YES\n");
	fprintf(fp, "aa 01 4b 46 7f ff 06 10 84 t=26625");
	fclose(fp);

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	unlink("/tmp/10-0000052ba3ac/w1_slave");
	rmdir("/tmp/10-0000052ba3ac");

	protocol_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}
static void test_protocols_gpio_ds18b20(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	run = DS18B20;
	
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	setenv("PILIGHT_DS18B20_PATH", "/tmp/", 1);

	ds18b20Init();

	char *add = "{\"test\":{\"protocol\":[\"ds18b20\"],\"id\":[{\"id\":\"0000052ba3ac\"}],\"temperature\":19.562,\"poll-interval\":1}}";

	unlink("/tmp/28-0000052ba3ac/w1_slave");
	rmdir("/tmp/28-0000052ba3ac");
	mkdir("/tmp/28-0000052ba3ac", 0777);

	FILE *fp = fopen("/tmp/28-0000052ba3ac/w1_slave", "w+");
  fprintf(fp, "aa 01 4b 46 7f ff 06 10 84 : crc=84 YES\n");
	fprintf(fp, "aa 01 4b 46 7f ff 06 10 84 t=26625");
	fclose(fp);

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	unlink("/tmp/28-0000052ba3ac/w1_slave");
	rmdir("/tmp/28-0000052ba3ac");

	protocol_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_gpio_ds18x20(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_gpio_ds18s20);
	SUITE_ADD_TEST(suite, test_protocols_gpio_ds18b20);

	return suite;
}