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
#include <unistd.h>
#include <wiringx.h>
#include <stdio.h>
#include <stdarg.h>

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/hardware/hardware.h"
#include "../libs/pilight/hardware/433gpio.h"

#include "alltests.h"

void test_hardware_433gpio_send(CuTest *tc);
void test_hardware_433gpio_receive_too_large_pulse(CuTest *tc);

static uv_thread_t pth[2];
static uv_pipe_t *pipe_req = NULL;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;
static int check = 0;

static int nr = 0;
static int pulses[] = {
	300, 3000,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 1200, 300, 300, 300, 300, 300, 1200,
	300, 300, 300, 300, 300, 300, 300, 1200, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 1200, 300, 300, 300, 300, 300, 1200,
	300, 300, 300, 1200, 300, 300, 300, 1200, 300, 1200, 300, 300,
	300, 10200
};

static void stop(uv_timer_t *req) {
	uv_stop(uv_default_loop());
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void thread1(void *arg) {
	uv_connect_t *req = arg;
	uv_os_fd_t fd = 0;
	uv_fileno((uv_handle_t *)req, &fd);

	int i = 0;
	for(i=0;i<10;i++) {
		nr = 0;
		while(nr < 148) {
			send(fd, "a", 1, 0);
			usleep(pulses[nr++]*0.75);
		}
	}
}

static void connect_cb1(uv_stream_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);

	uv_os_fd_t fd = 0;
	uv_pipe_t *client_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, client_req);

	int r = uv_pipe_init(uv_default_loop(), client_req, 0);

	r = uv_accept(req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_fileno((uv_handle_t *)client_req, &fd);

	uv_thread_create(&pth[1], thread1, client_req);
	return;
}

static void start(void) {
	int r = 0;

	pipe_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, pipe_req);

	r = uv_pipe_init(uv_default_loop(), pipe_req, 1);
	CuAssertIntEquals(gtc, 0, r);

	uv_fs_t file_req;
	uv_fs_unlink(uv_default_loop(), &file_req, "/dev/gpio1", NULL);

	r = uv_pipe_bind(pipe_req, "/dev/gpio1");
	CuAssertIntEquals(gtc, 0, r);

	r = uv_listen((uv_stream_t *)pipe_req, 128, connect_cb1);
	CuAssertIntEquals(gtc, 0, r);
}

static void thread(void *arg) {
	char *settings = "{\"433gpio\":{\"receiver\":1,\"sender\":-1}}";
	struct JsonNode *jsettings = json_decode(settings);
	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "433gpio", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jreceiver = json_first_child(jchild);
				struct JsonNode *jsender = jreceiver->next;
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jreceiver));
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jsender));
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->init());
			}
		}
	}
	json_delete(jsettings);
}

static void *receivePulseTrain(int reason, void *param, void *userdata) {
	struct reason_received_pulsetrain_t *data = param;
	int i = 0, x = 0, validate = 1;

	if(data->hardware != NULL && data->pulses != NULL && data->length == 148) {
		for(i=0;i<data->length;i++) {
			if((int)((double)pulses[i]*(double)1.33) > data->pulses[i] || data->pulses[i] < (int)((double)pulses[i]*(double)0.66)) {
				if(i > 0 && i-x != 1) {
					validate = 0;
				}
				x = i;
			}
		}
	}
	if(validate == 1) {
		check++;
	}

	return (void *)NULL;
}

static void foo(int prio, char *file, int line, const char *format_str, ...) {
}

static void test_hardware_433gpio_receive(CuTest *tc) {
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

	start();

	FILE *f = fopen("hardware_433gpio.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	setenv("PILIGHT_433GPIO_READ", "1", 1);

	hardware_init();
	gpio433Init();
	gpio433->minrawlen = 0;
	gpio433->maxrawlen = 300;

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	plua_init();

	test_set_plua_path(tc, __FILE__, "hardware_433gpio_receive.c");

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));
	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain, NULL);
	uv_thread_create(&pth[0], thread, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth[0]);
	uv_thread_join(&pth[1]);

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertTrue(tc, check >= 3);
	CuAssertIntEquals(tc, 0, xfree());
}


/*
  This is already checked in the config

static void test_hardware_433gpio_param(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	hardware_init();
	gpio433Init();

	FILE *f = fopen("hardware_433gpio.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"foo\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);
	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));

	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "433gpio", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jsettings = json_decode( "{\"433gpio\":{\"receiver\":\"a\",\"sender\":\"b\"}}");

				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jreceiver = json_first_child(jchild);
				struct JsonNode *jsender = jreceiver->next;

				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->settings(jreceiver));
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->settings(jsender));
				json_delete(jsettings);
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->init());
			}
		}
	}

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}*/

static void test_hardware_433gpio_param1(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	hardware_init();
	gpio433Init();

	FILE *f = fopen("hardware_433gpio.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	plua_init();

	test_set_plua_path(tc, __FILE__, "hardware_433gpio_receive.c");

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));

	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "433gpio", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->init());
			}
		}
	}

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_hardware_433gpio_param2(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	hardware_init();
	gpio433Init();

	FILE *f = fopen("hardware_433gpio.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);
	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "hardware_433gpio_receive.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));

	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "433gpio", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jsettings = json_decode( "{\"433gpio\":{\"receiver\":100,\"sender\":0}}");

				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jreceiver = json_first_child(jchild);
				struct JsonNode *jsender = jreceiver->next;

				CuAssertIntEquals(tc, EXIT_SUCCESS, hardware->settings(jreceiver));
				CuAssertIntEquals(tc, EXIT_SUCCESS, hardware->settings(jsender));
				json_delete(jsettings);
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->init());
			}
		}
	}

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_hardware_433gpio_param3(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	hardware_init();
	gpio433Init();

	FILE *f = fopen("hardware_433gpio.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);
	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "hardware_433gpio_receive.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));

	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "433gpio", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jsettings = json_decode( "{\"433gpio\":{\"receiver\":1,\"sender\":100}}");

				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jreceiver = json_first_child(jchild);
				struct JsonNode *jsender = jreceiver->next;

				CuAssertIntEquals(tc, EXIT_SUCCESS, hardware->settings(jreceiver));
				CuAssertIntEquals(tc, EXIT_SUCCESS, hardware->settings(jsender));
				json_delete(jsettings);
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->init());
			}
		}
	}

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_hardware_433gpio_param4(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	hardware_init();
	gpio433Init();

	FILE *f = fopen("hardware_433gpio.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);
	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "hardware_433gpio_receive.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));

	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "433gpio", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jsettings = json_decode( "{\"433gpio\":{\"receiver\":2,\"sender\":0}}");

				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jreceiver = json_first_child(jchild);
				struct JsonNode *jsender = jreceiver->next;

				CuAssertIntEquals(tc, EXIT_SUCCESS, hardware->settings(jreceiver));
				CuAssertIntEquals(tc, EXIT_SUCCESS, hardware->settings(jsender));
				json_delete(jsettings);
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->init());
			}
		}
	}

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_hardware_433gpio_param5(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	hardware_init();
	gpio433Init();

	FILE *f = fopen("hardware_433gpio.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);
	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "hardware_433gpio_receive.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));

	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "433gpio", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jsettings = json_decode( "{\"433gpio\":{\"receiver\":2,\"sender\":2}}");

				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jreceiver = json_first_child(jchild);
				struct JsonNode *jsender = jreceiver->next;

				CuAssertIntEquals(tc, EXIT_SUCCESS, hardware->settings(jreceiver));
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->settings(jsender));
				json_delete(jsettings);
				CuAssertIntEquals(tc, EXIT_FAILURE, hardware->init());
			}
		}
	}

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_hardware_433gpio(void) {
	CuSuite *suite = CuSuiteNew();

	// SUITE_ADD_TEST(suite, test_hardware_433gpio_param);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_param1);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_param2);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_param3);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_param4);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_param5);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_receive);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_receive_too_large_pulse);
	SUITE_ADD_TEST(suite, test_hardware_433gpio_send);

	return suite;
}
