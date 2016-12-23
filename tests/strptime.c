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
#include "../libs/pilight/core/strptime.h"

#include "alltests.h"

static void test_strptime(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct tm tm;
	time_t t;

	CuAssertPtrNotNull(tc, strptime("6 Dec 2001 12:33:45", "%d %b %Y %H:%M:%S", &tm));
	tm.tm_isdst = -1;
	mktime(&tm);
	CuAssertIntEquals(tc, 101, tm.tm_year);
	CuAssertIntEquals(tc, 11, tm.tm_mon);
	CuAssertIntEquals(tc, 6, tm.tm_mday);
	CuAssertIntEquals(tc, 12, tm.tm_hour);
	CuAssertIntEquals(tc, 33, tm.tm_min);
	CuAssertIntEquals(tc, 45, tm.tm_sec);
	CuAssertIntEquals(tc, 4, tm.tm_wday);
	CuAssertIntEquals(tc, 339, tm.tm_yday);

	t = datetime2ts(tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	CuAssertIntEquals(tc, 1007642025, t);

	CuAssertPtrNotNull(tc, strptime("2016-10-16 21:04:36", "%Y-%m-%d %H:%M:%S", &tm));
	tm.tm_isdst = -1;
	mktime(&tm);
	CuAssertIntEquals(tc, 116, tm.tm_year);
	CuAssertIntEquals(tc, 9, tm.tm_mon);
	CuAssertIntEquals(tc, 16, tm.tm_mday);
	CuAssertIntEquals(tc, 21, tm.tm_hour);
	CuAssertIntEquals(tc, 4, tm.tm_min);
	CuAssertIntEquals(tc, 36, tm.tm_sec);
	CuAssertIntEquals(tc, 0, tm.tm_wday);
	CuAssertIntEquals(tc, 289, tm.tm_yday);

	t = datetime2ts(tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	CuAssertIntEquals(tc, 1476651876, t);

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_strptime(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_strptime);

	return suite;
}