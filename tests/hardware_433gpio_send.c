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
#include "../libs/pilight/hardware/hardware.h"
#include "../libs/pilight/hardware/433gpio.h"

#include "alltests.h"

void test_hardware_433gpio_send(CuTest *tc);

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

typedef struct data_t {
	int rbuffer[1024];
	int rptr;
} data_t;

static struct data_t data;

static uv_thread_t pth[2];
static uv_pipe_t *pipe_req = NULL;
static CuTest *gtc = NULL;
static uv_poll_t *poll_req = NULL;
static uv_timer_t *timer_req = NULL;
static struct timestamp_t timestamp;
static int check = 0;

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

static void *reason_received_pulsetrain_free(void *param) {
	struct reason_received_pulsetrain_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_send_code_free(void *param) {
	struct reason_send_code_t *data = param;
	FREE(data);
	return NULL;
}

static void stop(uv_timer_t *req) {
	uv_poll_stop(poll_req);
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

static void poll_cb(uv_poll_t *req, int status, int events) {
	int fd = req->io_watcher.fd;
	int duration = 0;

	if(events & UV_READABLE) {
		uint8_t c = 0;

		(void)read(fd, &c, 1);
		lseek(fd, 0, SEEK_SET);

		struct timeval tv;
		gettimeofday(&tv, NULL);
		timestamp.first = timestamp.second;
		timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

		duration = (int)((int)timestamp.second-(int)timestamp.first);
		if(duration > 0) {
			data.rbuffer[data.rptr++] = duration;
			if(data.rptr > MAXPULSESTREAMLENGTH-1) {
				data.rptr = 0;
			}
			if(duration > gpio433->mingaplen) {
				/* Let's do a little filtering here as well */
				if(data.rptr >= gpio433->minrawlen && data.rptr <= gpio433->maxrawlen) {
					struct reason_received_pulsetrain_t *data1 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
					if(data1 == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					data1->length = data.rptr;
					memcpy(data1->pulses, data.rbuffer, data.rptr*sizeof(int));
					data1->hardware = gpio433->id;

					eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data1);
				}
				data.rptr = 0;
			}
		}
	}
}

static void *receivePulseTrain(int reason, void *param) {
	struct reason_received_pulsetrain_t *data = param;
	int i = 0, x = 0, validate = 1;

	if(data->hardware != NULL && data->pulses != NULL && data->length == 148) {
		for(i=0;i<data->length;i++) {
			if((int)((double)pulses[i]*(double)1.75) > data->pulses[i] || data->pulses[i] < (int)((double)pulses[i]*(double)0.25)) {
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

static void thread1(void *param) {
	struct reason_send_code_t *data = MALLOC(sizeof(struct reason_send_code_t));
	CuAssertPtrNotNull(gtc, data);
	data->origin = 1;
	data->rawlen = sizeof(pulses)/sizeof(pulses[0]);
	memcpy(data->pulses, pulses, data->rawlen*sizeof(int));
	data->txrpt = 5;
	strncpy(data->protocol, "dummy", 255);
	data->hwtype = 1;
	memset(data->uuid, 0, UUID_LENGTH+1);
	eventpool_trigger(REASON_SEND_CODE, reason_send_code_free, data);
}

static void connect_cb(uv_stream_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);

	uv_os_fd_t fd = 0;
	uv_pipe_t *client_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, client_req);

	int r = uv_pipe_init(uv_default_loop(), client_req, 0);

	r = uv_accept(req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_fileno((uv_handle_t *)client_req, &fd);

	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	memset(data.rbuffer, '\0', sizeof(data.rbuffer));
	data.rptr = 0;

	uv_poll_init(uv_default_loop(), poll_req, fd);
	uv_poll_start(poll_req, UV_READABLE, poll_cb);

	uv_thread_create(&pth[1], thread1, NULL);

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

static void thread(void *arg) {
	char *settings = "{\"433gpio\":{\"receiver\":-1,\"sender\":0}}";
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

static void foo(int prio, char *file, int line, const char *format_str, ...) {
}

void test_hardware_433gpio_send(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
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

	hardware_init();
	gpio433Init();
	gpio433->minrawlen = 0;
	gpio433->maxrawlen = 300;

	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(tc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("hardware_433gpio.json", CONFIG_SETTINGS));
	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain);
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
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertTrue(tc, check >= 1);
	CuAssertIntEquals(tc, 0, xfree());
}