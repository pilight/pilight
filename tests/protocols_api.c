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
#include "../libs/pilight/protocols/API/cpu_temp.h"
#include "../libs/pilight/protocols/API/datetime.h"
#include "../libs/pilight/protocols/API/sunriseset.h"

#include "alltests.h"

#define BUFSIZE 1024*1024

#define CPU_TEMP 						0
#define DATETIME						1
#define SUNRISESET_AMS			2
#define SUNRISESET_NY				3
#define SUNRISESET_BEIJING	4
#define SUNRISESET_RISE			5
#define SUNRISESET_SET			6
#define SUNRISESET_MIDNIGHT	7

static char add[1024];
static char adapt[1024];
static CuTest *gtc = NULL;
static int device = CPU_TEMP;
static int steps = 0;

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

static void *received(int reason, void *param) {
	struct reason_code_received_t *data = param;

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	int duration = (int)((int)timestamp.second-(int)timestamp.first);
	duration = (int)round((double)duration/1000000);

	switch(device) {
		case CPU_TEMP:
			CuAssertStrEquals(gtc, "{\"id\":1,\"temperature\":46.540}", data->message);
		break;
		case DATETIME:
			CuAssertStrEquals(gtc,
				"{\"longitude\":4.895168,\"latitude\":52.370216,\"year\":2017,\"month\":1,\"day\":6,\"weekday\":6,\"hour\":13,\"minute\":15,\"second\":5,\"dst\":0}",
				data->message);
		break;
		case SUNRISESET_AMS:
			CuAssertStrEquals(gtc,
				"{\"longitude\":4.895168,\"latitude\":52.370216,\"sun\":\"rise\",\"sunrise\":8.49,\"sunset\":16.46}",
				data->message);
		break;
		case SUNRISESET_NY:
			CuAssertStrEquals(gtc,
				"{\"longitude\":-73.935242,\"latitude\":40.730610,\"sun\":\"rise\",\"sunrise\":7.20,\"sunset\":16.46}",
				data->message);
		break;
		case SUNRISESET_BEIJING:
			CuAssertStrEquals(gtc,
				"{\"longitude\":116.363625,\"latitude\":39.913818,\"sun\":\"set\",\"sunrise\":7.37,\"sunset\":17.07}",
				data->message);
		break;
		case SUNRISESET_RISE:
			switch(steps) {
				case 0:
					CuAssertIntEquals(gtc, 0, duration);
					CuAssertStrEquals(gtc,
						"{\"longitude\":4.895168,\"latitude\":52.370216,\"sun\":\"set\",\"sunrise\":8.49,\"sunset\":16.48}",
						data->message);
					/*
					 * Set time one second after sunrise
					 */
					strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483861741}}");
					eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
					steps = 1;
				break;
				case 1:
					CuAssertIntEquals(gtc, 1, duration);
					CuAssertStrEquals(gtc,
						"{\"longitude\":4.895168,\"latitude\":52.370216,\"sun\":\"rise\",\"sunrise\":8.49,\"sunset\":16.48}",
						data->message);
					uv_stop(uv_default_loop());
				break;
			}
		break;
		case SUNRISESET_SET:
			switch(steps) {
				case 0:
					CuAssertIntEquals(gtc, 0, duration);
					CuAssertStrEquals(gtc,
						"{\"longitude\":4.895168,\"latitude\":52.370216,\"sun\":\"rise\",\"sunrise\":8.49,\"sunset\":16.48}",
						data->message);
					/*
					 * Set time one second after sunset
					 */
					strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483890481}}");
					eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
					steps = 1;
				break;
				case 1:
					CuAssertIntEquals(gtc, 1, duration);
					CuAssertStrEquals(gtc,
						"{\"longitude\":4.895168,\"latitude\":52.370216,\"sun\":\"set\",\"sunrise\":8.49,\"sunset\":16.48}",
						data->message);
					uv_stop(uv_default_loop());
				break;
			}
		break;
		case SUNRISESET_MIDNIGHT:
			switch(steps) {
				case 0:
					CuAssertIntEquals(gtc, 0, duration);
					CuAssertStrEquals(gtc,
						"{\"longitude\":4.895168,\"latitude\":52.370216,\"sun\":\"set\",\"sunrise\":8.49,\"sunset\":16.48}",
						data->message);
					/*
					 * Set time one second after midnight
					 */
					strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483916400}}");
					eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
					steps = 1;
				break;
				case 1:
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * After midnight the sunset and sunrise times should be updated as well
					 */
					CuAssertStrEquals(gtc,
						"{\"longitude\":4.895168,\"latitude\":52.370216,\"sun\":\"set\",\"sunrise\":8.48,\"sunset\":16.49}",
						data->message);
					uv_stop(uv_default_loop());
				break;
			}
		break;
	}

	if(device != SUNRISESET_RISE &&
		 device != SUNRISESET_MIDNIGHT &&
		 device != SUNRISESET_SET) {
		uv_stop(uv_default_loop());
	}

	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_protocols_api(CuTest *tc) {
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

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
	eventpool_callback(REASON_CODE_RECEIVED, received);

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

static void test_protocols_api_cpu_temp(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();	

	cpuTempInit();

	FILE *f = fopen("cpu_temp.txt", "w");
	CuAssertPtrNotNull(tc, f);
	fprintf(f, "%d", 46540);
	fclose(f);	

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(add, "{\"test\":{\"protocol\":[\"cpu_temp\"],\"id\":[{\"id\":1}],\"temperature\":0,\"poll-interval\":1}}");
	strcpy(adapt, "{\"cpu_temp\":{\"path\":\"cpu_temp.txt\"}}");

	device = CPU_TEMP;

	test_protocols_api(tc);
}

static void test_protocols_api_datetime(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	datetimeInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(add, "{\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"year\":2015,\"month\":1,\"day\":27,\"hour\":14,\"minute\":37,\"second\":8,\"weekday\":3,\"dst\":1}}");
	strcpy(adapt, "{\"datetime\":{\"time-override\":1483704905}}");

	device = DATETIME;

	test_protocols_api(tc);
}

static void test_protocols_api_sunriseset_amsterdam(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	sunRiseSetInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(add, "{\"test\":{\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"sunrise\":8.16,\"sunset\":16.30,\"sun\":\"set\"}}");
	strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483795539}}");

	device = SUNRISESET_AMS;

	test_protocols_api(tc);
}

static void test_protocols_api_sunriseset_new_york(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	sunRiseSetInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(add, "{\"test\":{\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":-73.935242,\"latitude\":40.730610}],\"sunrise\":8.16,\"sunset\":16.30,\"sun\":\"set\"}}");
	strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483795539}}");

	device = SUNRISESET_NY;

	test_protocols_api(tc);
}

static void test_protocols_api_sunriseset_beijing(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	sunRiseSetInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(add, "{\"test\":{\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":116.363625,\"latitude\":39.913818}],\"sunrise\":8.16,\"sunset\":16.30,\"sun\":\"set\"}}");
	strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483795539}}");

	device = SUNRISESET_BEIJING;

	test_protocols_api(tc);
}

static void test_protocols_api_sunriseset_rise(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	steps = 0;

	sunRiseSetInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	/*
	 * Set time one second before sunrise
	 */
	strcpy(add, "{\"test\":{\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"sunrise\":8.16,\"sunset\":16.30,\"sun\":\"set\"}}");
	strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483861739}}");

	device = SUNRISESET_RISE;

	test_protocols_api(tc);
}

static void test_protocols_api_sunriseset_set(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	steps = 0;

	sunRiseSetInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	/*
	 * Set time one second before sunset
	 */
	strcpy(add, "{\"test\":{\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"sunrise\":8.16,\"sunset\":16.30,\"sun\":\"set\"}}");
	strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483890479}}");

	device = SUNRISESET_SET;

	test_protocols_api(tc);
}

static void test_protocols_api_sunriseset_midnight(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	steps = 0;

	sunRiseSetInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	/*
	 * Set time one second before midnight
	 */
	strcpy(add, "{\"test\":{\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"sunrise\":8.16,\"sunset\":16.30,\"sun\":\"set\"}}");
	strcpy(adapt, "{\"sunriseset\":{\"time-override\":1483916399}}");

	device = SUNRISESET_MIDNIGHT;

	test_protocols_api(tc);
}

CuSuite *suite_protocols_api(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_api_cpu_temp);
	SUITE_ADD_TEST(suite, test_protocols_api_datetime);
	SUITE_ADD_TEST(suite, test_protocols_api_sunriseset_amsterdam);
	SUITE_ADD_TEST(suite, test_protocols_api_sunriseset_new_york);
	SUITE_ADD_TEST(suite, test_protocols_api_sunriseset_beijing);
	SUITE_ADD_TEST(suite, test_protocols_api_sunriseset_rise);
	SUITE_ADD_TEST(suite, test_protocols_api_sunriseset_set);
	SUITE_ADD_TEST(suite, test_protocols_api_sunriseset_midnight);

	return suite;
}