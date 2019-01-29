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

#include "../libs/pilight/core/CuTest.h"

void test_lua_hardware_433gpio_param1(CuTest *tc);
void test_lua_hardware_433gpio_param2(CuTest *tc);
void test_lua_hardware_433gpio_param3(CuTest *tc);
void test_lua_hardware_433gpio_param4(CuTest *tc);
void test_lua_hardware_433gpio_param5(CuTest *tc);
void test_lua_hardware_433gpio_receive(CuTest *tc);
void test_lua_hardware_433gpio_receive_large_pulse(CuTest *tc);
void test_lua_hardware_433gpio_send(CuTest *tc);

CuSuite *suite_lua_hardware(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_param1);
	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_param2);
	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_param3);
	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_param4);
	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_param5);
	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_receive);
	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_receive_large_pulse);
	SUITE_ADD_TEST(suite, test_lua_hardware_433gpio_send);

	return suite;
}
