/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <wiringx.h>

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/GPIO/gpio_switch.h"

#include "alltests.h"

static uv_pipe_t *pipe_req = NULL;
static CuTest *gtc = NULL;
static int check = 0;
static int step = 0;
static uv_timer_t *stop_timer_req = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}


static void stop(uv_timer_t *req) {
	uv_stop(uv_default_loop());
}

static void *received(int reason, void *param, void *userdata) {
	struct reason_code_received_t *data = param;
	switch(step++) {
		case 0: {
			CuAssertStrEquals(gtc, "{\"gpio\":0,\"state\":\"on\"}", data->message);
			check = 1;
		}
		break;
		case 1: {
			CuAssertStrEquals(gtc, "{\"gpio\":0,\"state\":\"off\"}", data->message);
			check = 2;
		}
		break;
		case 2: {
			CuAssertStrEquals(gtc, "{\"gpio\":0,\"state\":\"on\"}", data->message);
			check = 3;
		}
		break;
		case 3: {
			CuAssertStrEquals(gtc, "{\"gpio\":0,\"state\":\"off\"}", data->message);
			check = 4;
			uv_stop(uv_default_loop());
		}
		break;
	}
	return NULL;
}

static void trigger(uv_timer_t *req) {
	uv_os_fd_t fd = 0;
	uv_pipe_t *client_req = req->data;
	int r = uv_fileno((uv_handle_t *)client_req, &fd);
	CuAssertIntEquals(gtc, 0, r);

	CuAssertIntEquals(gtc, 1, write(fd, "a", 1));

	if(!uv_is_closing((uv_handle_t *)client_req)) {
		uv_timer_stop(req);
		uv_close((uv_handle_t *)client_req, close_cb);
	}
}

static void connect_cb(uv_stream_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);

	uv_pipe_t *client_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, client_req);

	int r = uv_pipe_init(uv_default_loop(), client_req, 0);

	r = uv_accept(req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	uv_timer_t *timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	timer_req->data = client_req;
	uv_timer_stop(timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))trigger, 100, 100);

	return;
}

static void start(void) {
	int r = 0;

	pipe_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, pipe_req);

	r = uv_pipe_init(uv_default_loop(), pipe_req, 1);
	CuAssertIntEquals(gtc, 0, r);

	uv_fs_t file_req;
	uv_fs_unlink(uv_default_loop(), &file_req, "/dev/gpio0", NULL);

	r = uv_pipe_bind(pipe_req, "/dev/gpio0");
	CuAssertIntEquals(gtc, 0, r);

	r = uv_listen((uv_stream_t *)pipe_req, 128, connect_cb);
	CuAssertIntEquals(gtc, 0, r);
}

static void foo(int prio, char *file, int line, const char *format_str, ...) {
}

static void test_protocols_gpio_switch_param(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gpioSwitchInit();

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, NULL, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	protocol_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_gpio_switch_param1(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gpioSwitchInit();

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode("\"foo\""));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	protocol_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_gpio_switch_param2(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gpioSwitchInit();

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	char *add = "{\"test\":{\"protocol\":[\"generic_switch\", \"greneric_dimmer\"],\"id\":[{\"id\":0}],\"state\":\"off\",\"dimlevel\":0,\"readonly\":1}}";

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	protocol_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_gpio_switch_param3(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gpioSwitchInit();

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	char *add = "{\"test\":{\"protocol\":[\"gpio_switch\"],\"id\":[{\"gpio\":99}],\"state\":\"off\",\"resolution\":100,\"readonly\":1}}";

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	protocol_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_gpio_switch_param4(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	setenv("PILIGHT_GPIO_SWITCH_READ", "1", 1);

	FILE *f = fopen("protocols_gpio_switch.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	gpioSwitchInit();

	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "protocols_gpio_switch.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("protocols_gpio_switch.json", CONFIG_SETTINGS));

	struct JsonNode *jsettings = json_decode("{\"id\":[{\"gpio\":0}],\"readonly\":1}");
	CuAssertIntEquals(tc, -1, gpio_switch->checkValues(jsettings));
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	protocol_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

/*

  This is already validated in the config

static void test_protocols_gpio_switch_param5(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	setenv("PILIGHT_GPIO_SWITCH_READ", "1", 1);

	FILE *f = fopen("protocols_gpio_switch.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"foo\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	gpioSwitchInit();

	eventpool_init(EVENTPOOL_NO_THREADS);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("protocols_gpio_switch.json", CONFIG_SETTINGS));

	struct JsonNode *jsettings = json_decode("{\"id\":[{\"gpio\":0}],\"readonly\":1}");
	CuAssertIntEquals(tc, -1, gpio_switch->checkValues(jsettings));
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	protocol_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}
*/

static void test_protocols_gpio_switch_param6(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	setenv("PILIGHT_GPIO_SWITCH_READ", "1", 1);

	FILE *f = fopen("protocols_gpio_switch.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	gpioSwitchInit();

	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "protocols_gpio_switch.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("protocols_gpio_switch.json", CONFIG_SETTINGS));

	struct JsonNode *jsettings = json_decode("{\"id\":[{\"gpio\":99}],\"readonly\":1}");
	CuAssertIntEquals(tc, -1, gpio_switch->checkValues(jsettings));
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	protocol_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_gpio_switch_param7(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	setenv("PILIGHT_GPIO_SWITCH_READ", "1", 1);

	FILE *f = fopen("protocols_gpio_switch.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 10, 0);

	gpioSwitchInit();

	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "protocols_gpio_switch.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("protocols_gpio_switch.json", CONFIG_SETTINGS));

	struct JsonNode *jsettings = json_decode("{\"id\":[{\"gpio\":0}],\"readonly\":2}");
	CuAssertIntEquals(tc, -1, gpio_switch->checkValues(jsettings));
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	protocol_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_gpio_switch(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	setenv("PILIGHT_GPIO_SWITCH_READ", "1", 1);

	FILE *f = fopen("protocols_gpio_switch.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	start();

	stop_timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, stop_timer_req);
	uv_timer_init(uv_default_loop(), stop_timer_req);
	uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	gpioSwitchInit();

	char *add = "{\"test\":{\"protocol\":[\"gpio_switch\"],\"id\":[{\"gpio\":0}],\"state\":\"off\",\"resolution\":100,\"readonly\":1}}";

	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "protocols_gpio_switch.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("protocols_gpio_switch.json", CONFIG_SETTINGS));

	struct JsonNode *jsettings = json_decode("{\"id\":[{\"gpio\":0}],\"readonly\":1}");
	CuAssertIntEquals(tc, EXIT_SUCCESS, gpio_switch->checkValues(jsettings));
	json_delete(jsettings);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	protocol_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 4, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_gpio_switch(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param);
	SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param1);
	SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param2);
	SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param3);
	SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param4);
	// SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param5);
	SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param6);
	SUITE_ADD_TEST(suite, test_protocols_gpio_switch_param7);
	SUITE_ADD_TEST(suite, test_protocols_gpio_switch);

	return suite;
}
