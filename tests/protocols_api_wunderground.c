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
#include "../libs/pilight/protocols/API/wunderground.h"

#include "alltests.h"

#include "wunderground.h"

#define BUFSIZE 1024*1024

#define UPDATE		1
#define SUNRISE		2
#define SUNSET		3
#define MIDNIGHT	4

static char add[1024];
static char adapt[1024];
static CuTest *gtc = NULL;
static int steps = 0;
static int run = UPDATE;
static int request = 0;
static uv_buf_t buf1;
static uv_buf_t buf2;

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

struct timestamp_t timestamp;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void *received(int reason, void *param, void *userdata) {
	struct reason_code_received_t *data = param;

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	int duration = (int)((int)timestamp.second-(int)timestamp.first);
	duration = (int)round((double)duration/1000000);

	request = 0;

	switch(run) {
		case UPDATE:
			switch(steps) {
				case 0: {
					CuAssertIntEquals(gtc, 1, duration);
					char message[1024];
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"update\":1}",
						data->message);
					steps = 1;
					CuAssertPtrNotNull(gtc, wunderground->createCode);
					struct JsonNode *code = json_decode("{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"update\":1}");
					char *p = message;
					wunderground->createCode(code, &p);
					json_delete(code);
					printf("[ %-48s ]\n", "- waiting for wunderground notification");
					fflush(stdout);
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 0, duration);
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":2.40,\"humidity\":99,\"update\":0,\"sunrise\":8.48,\"sunset\":16.46,\"sun\":\"rise\"}",
						data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		break;
		case SUNRISE:
			switch(steps) {
				case 0: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * Ignore the first update message.
					 */
					steps = 1;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 2, duration);
					/*
					 * After two seconds the first message should be received
					 */
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":2.40,\"humidity\":99,\"update\":0,\"sunrise\":8.48,\"sunset\":16.46,\"sun\":\"set\"}",
						data->message);
					/*
					 * Shift time to after sunrise and check if we get a
					 * sunrise from wunderground as well.
					 */
					strcpy(adapt, "{\"wunderground\":{\"time-override\":1483778881}}");
					eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
					steps = 2;
					printf("[ %-48s ]\n", "- waiting for wunderground sunrise");
					fflush(stdout);	
				} break;
				case 2: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * After one second the sunrise notification should be sent
					 */
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":2.40,\"humidity\":99,\"update\":0,\"sunrise\":8.48,\"sunset\":16.46,\"sun\":\"rise\"}",
						data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		break;
		case SUNSET:
			switch(steps) {
				case 0: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * Ignore the first update message.
					 */
					steps = 1;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 2, duration);
					/*
					 * After two seconds the first message should be received
					 */
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":2.40,\"humidity\":99,\"update\":0,\"sunrise\":8.48,\"sunset\":16.46,\"sun\":\"rise\"}",
						data->message);
					/*
					 * Shift time to after sunset and check if we get a
					 * sunset from wunderground as well.
					 */
					strcpy(adapt, "{\"wunderground\":{\"time-override\":1483807561}}");
					eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
					steps = 2;
					printf("[ %-48s ]\n", "- waiting for wunderground sunset");
					fflush(stdout);	
				} break;
				case 2: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * After one second the sunrise notification should be sent
					 */
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":2.40,\"humidity\":99,\"update\":0,\"sunrise\":8.48,\"sunset\":16.46,\"sun\":\"set\"}",
						data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		break;
		case MIDNIGHT:
			switch(steps) {
				case 0: {
					/*
					 * Ignore the first update message.
					 */
					steps = 1;
					CuAssertIntEquals(gtc, 1, duration);
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 2, duration);
					/*
					 * Directly after, the first message should be received
					 */
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":2.40,\"humidity\":99,\"update\":0,\"sunrise\":8.48,\"sunset\":16.46,\"sun\":\"set\"}",
						data->message);

					steps = 2;
					printf("[ %-48s ]\n", "- waiting for wunderground midnight");
					fflush(stdout);	
				} break;
				case 2: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * After one second the sunrise notification should be sent
					 */
					CuAssertStrEquals(gtc,
						"{\"api\":\"abcdef123456\",\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":2.40,\"humidity\":99,\"update\":0,\"sunrise\":8.48,\"sunset\":16.46,\"sun\":\"set\"}",
						data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		break;
	}
	
	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	buf->base = malloc(len);
	CuAssertPtrNotNull(gtc, buf->base);
	memset(buf->base, 0, len);
}

static void write_cb(uv_write_t *req, int status) {
	CuAssertIntEquals(gtc, 0, status);
	FREE(req);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	uv_read_stop(stream);
	buf->base[nread] = '\0';

	request++;
	if(request == 1) {
		CuAssertStrEquals(gtc, "GET /api/abcdef123456/geolookup/conditions/q/nl/amsterdam.json HTTP/1.1\r\nHost: 127.0.0.1\r\nUser-Agent: pilight\r\nConnection: close\r\n\r\n", buf->base);
	} else if(request == 2) {
		CuAssertStrEquals(gtc, "GET /api/abcdef123456/geolookup/astronomy/q/nl/amsterdam.json HTTP/1.1\r\nHost: 127.0.0.1\r\nUser-Agent: pilight\r\nConnection: close\r\n\r\n", buf->base);
	}

	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	int r = 0;
	if(request == 1) {
		buf1.base = conditions;
		buf1.len = strlen(conditions);
		r = uv_write(write_req, stream, &buf1, 1, write_cb);
	} else if(request == 2) {
		buf2.base = astronomy;
		buf2.len = strlen(astronomy);		
		r = uv_write(write_req, stream, &buf2, 1, write_cb);
	}
	CuAssertIntEquals(gtc, 0, r);
	free(buf->base);
}

static void connection_cb(uv_stream_t *server_req, int status) {
	uv_tcp_t *client_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, client_req);
	CuAssertIntEquals(gtc, 0, status);
	uv_tcp_init(uv_default_loop(), client_req);

	int r = uv_accept(server_req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);
	r = uv_read_start((uv_stream_t *)client_req, alloc, read_cb);
	CuAssertIntEquals(gtc, 0, r);
}

static void test_protocols_api_wunderground(CuTest *tc) {
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	uv_tcp_t *server_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(tc, server_req);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;	
	
	eventpool_init(EVENTPOOL_NO_THREADS);

	if(strlen(adapt) > 0) {
		eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
	}
	if(strlen(add) > 0) {
		eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	}
	eventpool_callback(REASON_CODE_RECEIVED, received, NULL);

	struct sockaddr_in addr;
	int r = uv_ip4_addr("127.0.0.1", 10080, &addr);
	CuAssertIntEquals(tc, r, 0);

	uv_tcp_init(uv_default_loop(), server_req);
	uv_tcp_bind(server_req, (const struct sockaddr *)&addr, 0);

	r = uv_listen((uv_stream_t *)server_req, 128, connection_cb);
	CuAssertIntEquals(tc, r, 0);

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

static void test_protocols_api_wunderground_update(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();	

	request = 0;
	steps = 0;

	wundergroundInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(adapt, "{\"wunderground\":{\"time-override\":1484393226,\"url\":{\"astronomy\":\"http://127.0.0.1:10080/api/%s/geolookup/astronomy/q/%s/%s.json\",\"conditions\":\"http://127.0.0.1:10080/api/%s/geolookup/conditions/q/%s/%s.json\"}}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"wunderground\"],\"id\":[{\"api\":\"abcdef123456\",\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	run = UPDATE;
	
	printf("[ %-48s ]\n", "- waiting for update message");
	fflush(stdout);

	test_protocols_api_wunderground(tc);
}

static void test_protocols_api_wunderground_sunrise(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	request = 0;
	steps = 0;

	wundergroundInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(adapt, "{\"wunderground\":{\"time-override\":1483778879,\"url\":{\"astronomy\":\"http://127.0.0.1:10080/api/%s/geolookup/astronomy/q/%s/%s.json\",\"conditions\":\"http://127.0.0.1:10080/api/%s/geolookup/conditions/q/%s/%s.json\"}}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"wunderground\"],\"id\":[{\"api\":\"abcdef123456\",\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	printf("[ %-48s ]\n", "- waiting for first wunderground notification");
	fflush(stdout);	
	
	run = SUNRISE;

	test_protocols_api_wunderground(tc);
}

static void test_protocols_api_wunderground_sunset(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	request = 0;
	steps = 0;

	wundergroundInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(adapt, "{\"wunderground\":{\"time-override\":1483807559,\"url\":{\"astronomy\":\"http://127.0.0.1:10080/api/%s/geolookup/astronomy/q/%s/%s.json\",\"conditions\":\"http://127.0.0.1:10080/api/%s/geolookup/conditions/q/%s/%s.json\"}}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"wunderground\"],\"id\":[{\"api\":\"abcdef123456\",\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	printf("[ %-48s ]\n", "- waiting for first wunderground notification");
	fflush(stdout);	

	run = SUNSET;

	test_protocols_api_wunderground(tc);
}

static void test_protocols_api_wunderground_midnight(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	request = 0;
	steps = 0;

	wundergroundInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(adapt, "{\"wunderground\":{\"time-override\":1483833599,\"url\":{\"astronomy\":\"http://127.0.0.1:10080/api/%s/geolookup/astronomy/q/%s/%s.json\",\"conditions\":\"http://127.0.0.1:10080/api/%s/geolookup/conditions/q/%s/%s.json\"}}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"wunderground\"],\"id\":[{\"api\":\"abcdef123456\",\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	printf("[ %-48s ]\n", "- waiting for first wunderground notification");
	fflush(stdout);	

	run = MIDNIGHT;

	test_protocols_api_wunderground(tc);
}

CuSuite *suite_protocols_api_wunderground(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_api_wunderground_update);
	SUITE_ADD_TEST(suite, test_protocols_api_wunderground_sunrise);
	SUITE_ADD_TEST(suite, test_protocols_api_wunderground_sunset);
	SUITE_ADD_TEST(suite, test_protocols_api_wunderground_midnight);

	return suite;
}