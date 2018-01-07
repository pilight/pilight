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
#include "../libs/pilight/protocols/generic/generic_dimmer.h"
#include "../libs/pilight/protocols/generic/generic_label.h"
#include "../libs/pilight/protocols/generic/generic_screen.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_weather.h"
#include "../libs/pilight/protocols/generic/generic_webcam.h"

#include "alltests.h"

static void test_protocols_generic_dimmer(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char message[1024];

	genericDimmerInit();

	struct JsonNode *jcode = json_mkobject();
	/*json_append_member(jcode, "dimlevel-maximum", json_mknumber(10, 0));
	json_append_member(jcode, "dimlevel-minimum", json_mknumber(0, 0));*/
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "dimlevel", json_mknumber(5, 0));
	json_append_member(jcode, "on", json_mknumber(1, 0));

	char *p = message;
	/*CuAssertIntEquals(tc, 0, generic_dimmer->checkValues(jcode));*/
	CuAssertIntEquals(tc, 0, generic_dimmer->createCode(jcode, &p));
	CuAssertStrEquals(tc, "{\"id\":1,\"dimlevel\":5,\"state\":\"on\"}", message);

	json_delete(jcode);

	/*
	jcode = json_mkobject();
	json_append_member(jcode, "dimlevel-maximum", json_mknumber(20, 0));
	json_append_member(jcode, "dimlevel-minimum", json_mknumber(0, 0));
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "dimlevel", json_mknumber(16, 0));
	json_append_member(jcode, "on", json_mknumber(1, 0));

	CuAssertIntEquals(tc, 0, generic_dimmer->checkValues(jcode));
	json_delete(jcode);

	jcode = json_mkobject();
	json_append_member(jcode, "dimlevel", json_mknumber(16, 0));

	CuAssertIntEquals(tc, 0, generic_dimmer->checkValues(jcode));
	json_delete(jcode);
	*/

	jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "dimlevel", json_mknumber(16, 0));
	json_append_member(jcode, "on", json_mknumber(1, 0));

	CuAssertIntEquals(tc, 0, generic_dimmer->createCode(jcode, &p));
	CuAssertStrEquals(tc, "{\"id\":1,\"dimlevel\":16,\"state\":\"on\"}", message);

	json_delete(jcode);

	protocol_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_generic_label(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char message[1024];

	genericLabelInit();

	struct JsonNode *jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "label", json_mkstring("Foobar"));
	json_append_member(jcode, "color", json_mkstring("#FFFFFF"));

	char *p = message;
	CuAssertIntEquals(tc, 0, generic_label->createCode(jcode, &p));
	CuAssertStrEquals(tc, "{\"id\":1,\"label\":\"Foobar\",\"color\":\"#FFFFFF\"}", message);

	json_delete(jcode);

	protocol_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_generic_screen(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char message[1024];

	genericScreenInit();

	struct JsonNode *jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "down", json_mknumber(1, 0));

	char *p = message;
	CuAssertIntEquals(tc, 0, generic_screen->createCode(jcode, &p));
	CuAssertStrEquals(tc, "{\"id\":1,\"state\":\"down\"}", message);

	json_delete(jcode);

	protocol_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_generic_switch(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char message[1024];

	genericSwitchInit();

	struct JsonNode *jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "on", json_mknumber(1, 0));

	char *p = message;
	CuAssertIntEquals(tc, 0, generic_switch->createCode(jcode, &p));
	CuAssertStrEquals(tc, "{\"id\":1,\"state\":\"on\"}", message);

	json_delete(jcode);

	protocol_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_generic_weather(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char message[1024];

	genericWeatherInit();

	struct JsonNode *jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "humidity", json_mknumber(1, 0));
	json_append_member(jcode, "temperature", json_mknumber(1, 0));
	json_append_member(jcode, "battery", json_mknumber(1, 0));
	json_append_member(jcode, "temperature-decimals", json_mknumber(2, 0));
	json_append_member(jcode, "humidity-decimals", json_mknumber(2, 0));
	json_append_member(jcode, "show-humidity", json_mknumber(2, 0));
	json_append_member(jcode, "show-temperature", json_mknumber(2, 0));
	json_append_member(jcode, "show-battery", json_mknumber(2, 0));

	char *p = message;
	CuAssertIntEquals(tc, 0, generic_weather->createCode(jcode, &p));
	CuAssertStrEquals(tc, "{\"id\":1,\"temperature\":1.00,\"humidity\":1.00,\"battery\":1}", message);

	json_delete(jcode);

	protocol_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_generic_webcam(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	genericWebcamInit();

	struct JsonNode *jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "url", json_mkstring("foobar"));
	json_append_member(jcode, "image-width", json_mknumber(400, 0));
	json_append_member(jcode, "image-height", json_mknumber(300, 0));
	json_append_member(jcode, "show-webcam", json_mknumber(1, 0));
	json_append_member(jcode, "poll-interval", json_mknumber(1, 0));

	CuAssertIntEquals(tc, 0, generic_webcam->checkValues(jcode));
	json_delete(jcode);

	jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "url", json_mkstring("foobar"));
	json_append_member(jcode, "image-width", json_mknumber(400, 0));
	json_append_member(jcode, "image-height", json_mknumber(-1, 0));
	json_append_member(jcode, "show-webcam", json_mknumber(1, 0));
	json_append_member(jcode, "poll-interval", json_mknumber(1, 0));

	CuAssertIntEquals(tc, 1, generic_webcam->checkValues(jcode));
	json_delete(jcode);

	jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "url", json_mkstring("foobar"));
	json_append_member(jcode, "image-width", json_mknumber(400, 0));
	json_append_member(jcode, "image-height", json_mknumber(300, 0));
	json_append_member(jcode, "show-webcam", json_mknumber(-1, 0));
	json_append_member(jcode, "poll-interval", json_mknumber(1, 0));

	CuAssertIntEquals(tc, 1, generic_webcam->checkValues(jcode));
	json_delete(jcode);

	jcode = json_mkobject();
	json_append_member(jcode, "id", json_mknumber(1, 0));
	json_append_member(jcode, "url", json_mkstring("foobar"));
	json_append_member(jcode, "image-width", json_mknumber(400, 0));
	json_append_member(jcode, "image-height", json_mknumber(300, 0));
	json_append_member(jcode, "show-webcam", json_mknumber(1, 0));
	json_append_member(jcode, "poll-interval", json_mknumber(-1, 0));

	CuAssertIntEquals(tc, 1, generic_webcam->checkValues(jcode));
	json_delete(jcode);

	protocol_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_generic(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_generic_dimmer);
	SUITE_ADD_TEST(suite, test_protocols_generic_label);
	SUITE_ADD_TEST(suite, test_protocols_generic_screen);
	SUITE_ADD_TEST(suite, test_protocols_generic_switch);
	SUITE_ADD_TEST(suite, test_protocols_generic_weather);
	SUITE_ADD_TEST(suite, test_protocols_generic_webcam);

	return suite;
}