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
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/sha256cache.h"

#include "alltests.h"

static void test_sha256cache(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	CuAssertIntEquals(tc, 25000, SHA256_ITERATIONS);
	CuAssertIntEquals(tc, 0, sha256cache_add("hello world"));
	CuAssertStrEquals(tc, "94c6dd436f81d8f3a49787092338505ebc09317d4579f2e72ba865c4e44d92a2", sha256cache_get_hash("hello world"));
	CuAssertIntEquals(tc, 0, sha256cache_rm("hello world"));

	sha256cache_gc();
	log_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_sha256cache(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_sha256cache);

	return suite;
}