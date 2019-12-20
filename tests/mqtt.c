/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/mqtt.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"

void test_mqtt_decode(CuTest *tc);
void test_mqtt_encode(CuTest *tc);
void test_mqtt_server_client(CuTest *tc);
void test_mqtt_blacklist(CuTest *tc);

CuSuite *suite_mqtt(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_mqtt_decode);
	SUITE_ADD_TEST(suite, test_mqtt_encode);
	SUITE_ADD_TEST(suite, test_mqtt_server_client);
	SUITE_ADD_TEST(suite, test_mqtt_blacklist);

	return suite;
}