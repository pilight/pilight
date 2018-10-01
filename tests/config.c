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

#include "../libs/pilight/core/CuTest.h"

void test_config_settings(CuTest *tc);
void test_config_registry(CuTest *tc);

CuSuite *suite_config(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_config_settings);

	return suite;
}


