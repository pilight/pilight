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
#include "../libs/pilight/core/datetime.h"

static void test_coord2tz(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	datetime_init();
	/*
	 * Amsterdam
	 */
	CuAssertStrEquals(tc, "Europe/Amsterdam", coord2tz(4.895168, 52.370216));

	/*
	 * New-York
	 */
	CuAssertStrEquals(tc, "America/New_York", coord2tz(-74.005941, 40.712784));

	/*
	 * Beijing
	 */
	CuAssertStrEquals(tc, "Asia/Shanghai", coord2tz(116.407395, 39.904211));

	/*
	 * Johannesburg
	 */
	CuAssertStrEquals(tc, "Africa/Mbabane", coord2tz(28.047305, -26.204103));

	/*
	 * Berlin
	 */
	CuAssertStrEquals(tc, "Europe/Berlin", coord2tz(13.404954, 52.520007));

	/*
	 * London
	 */
	CuAssertStrEquals(tc, "Europe/London", coord2tz(-0.127758, 51.507351));

	/*
	 * Invalid
	 */
	CuAssertPtrEquals(tc, NULL, coord2tz(-100, 100));
	CuAssertPtrEquals(tc, NULL, coord2tz(0, 0));

	datetime_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_datetime2ts(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	datetime_init();

	time_t t = 0;
	t = datetime2ts(1970, 1, 1, 0, 0, 0);
	CuAssertIntEquals(tc, 0, t);

	t = datetime2ts(1970, 1, 1, 8, 0, 0);
	CuAssertIntEquals(tc, 28800, t);

	t = datetime2ts(2015, 6, 18, 4, 21, 10);
	CuAssertIntEquals(tc, 1434601270, t);

	t = datetime2ts(1969, 12, 31, 23, 59, 59);
	CuAssertIntEquals(tc, -1, t);
	
	t = datetime2ts(-1, -1, -1, -1, -1, -1);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(1969, 2, 9, 16, 15, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2016, 0, 9, 16, 15, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2015, 1, 32, 16, 15, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2015, 2, 31, 16, 15, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2015, 2, 29, 16, 15, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2016, 2, 0, 16, 15, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2016, 2, 9, 25, 15, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2016, 2, 9, 16, 61, 34);	
	CuAssertIntEquals(tc, -1, t);

	t = datetime2ts(2016, 2, 9, 16, 15, 61);	
	CuAssertIntEquals(tc, -1, t);
	
	datetime_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_localtime_l(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	datetime_init();

	time_t t = 0;
	struct tm tm;
	char *out = NULL;

	/*
	 * Amsterdam
	 * - 2016-03-27 02:00 DST
	 * - 2016-10-30 03:00 Normal
	 * - 2017-03-26 02:00 DST
	 * - 2017-10-29 03:00 Normal
	 */

	/*
	 * UTC - Amsterdam +1
	 * Standard +0
	 */
	CuAssertTrue(tc, (t = datetime2ts(2016, 3, 27, 0, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 27 01:00:00 2016\n", out);

	/*
	 * UTC - Amsterdam +1
	 * DST +1
	 */
	CuAssertTrue(tc, (t = datetime2ts(2016, 3, 27, 1, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 27 03:00:00 2016\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2016, 3, 27, 2, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 27 04:00:00 2016\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2016, 10, 30, 0, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Oct 30 02:00:00 2016\n", out);

	/*
	 * UTC - Amsterdam +1
	 * Standard +0
	 */
	CuAssertTrue(tc, (t = datetime2ts(2016, 10, 30, 1, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Oct 30 02:00:00 2016\n", out);
	
	CuAssertTrue(tc, (t = datetime2ts(2016, 10, 30, 2, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Oct 30 03:00:00 2016\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2017, 3, 26, 0, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 26 01:00:00 2017\n", out);

	/*
	 * UTC - Amsterdam +1
	 * DST +1
	 */
	CuAssertTrue(tc, (t = datetime2ts(2017, 3, 26, 1, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 26 03:00:00 2017\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2017, 3, 26, 2, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 26 04:00:00 2017\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2017, 10, 29, 0, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Oct 29 02:00:00 2017\n", out);

	/*
	 * UTC - Amsterdam +1
	 * Standard +0
	 */
	CuAssertTrue(tc, (t = datetime2ts(2017, 10, 29, 1, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Oct 29 02:00:00 2017\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2017, 10, 29, 2, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "Europe/Amsterdam"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Oct 29 03:00:00 2017\n", out);

	/*
	 * New York
	 * - 2016-03-13 02:00 DST
	 * - 2016-11-06 02:00 Normal
	 * - 2017-03-12 02:00 DST
	 * - 2017-11-05 02:00 Normal
	 */

	/*
	 * UTC - New York -5
	 * Standard +0
	 */
	CuAssertTrue(tc, (t = datetime2ts(2016, 3, 13, 5, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 13 00:00:00 2016\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2016, 3, 13, 6, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 13 01:00:00 2016\n", out);

	/*
	 * UTC - New York -5
	 * DST +1
	 */
	CuAssertTrue(tc, (t = datetime2ts(2016, 3, 13, 7, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 13 03:00:00 2016\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2016, 3, 13, 8, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 13 04:00:00 2016\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 0, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__	
	CuAssertStrEquals(tc, "Sat Nov 05 20:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  5 20:00:00 2016\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 1, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sat Nov 05 21:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  5 21:00:00 2016\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 2, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sat Nov 05 22:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  5 22:00:00 2016\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 3, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sat Nov 05 23:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  5 23:00:00 2016\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 4, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sun Nov 06 00:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  6 00:00:00 2016\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 5, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sun Nov 06 01:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  6 01:00:00 2016\n", out);
#endif

	/*
	 * UTC - New York -5
	 * Standard +0
	 */
	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 6, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__	
	CuAssertStrEquals(tc, "Sun Nov 06 01:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  6 01:00:00 2016\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2016, 11, 6, 7, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sun Nov 06 02:00:00 2016\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  6 02:00:00 2016\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2017, 3, 12, 5, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 12 00:00:00 2017\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2017, 3, 12, 6, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 12 01:00:00 2017\n", out);

	/*
	 * UTC - New York -5
	 * DST +1
	 */
	CuAssertTrue(tc, (t = datetime2ts(2017, 3, 12, 7, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 12 03:00:00 2017\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2017, 3, 12, 8, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
	CuAssertStrEquals(tc, "Sun Mar 12 04:00:00 2017\n", out);

	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 0, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sat Nov 04 20:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  4 20:00:00 2017\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 1, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sat Nov 04 21:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  4 21:00:00 2017\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 2, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__	
	CuAssertStrEquals(tc, "Sat Nov 04 22:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  4 22:00:00 2017\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 3, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sat Nov 04 23:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sat Nov  4 23:00:00 2017\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 4, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sun Nov 05 00:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  5 00:00:00 2017\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 5, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sun Nov 05 01:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  5 01:00:00 2017\n", out);
#endif

	/*
	 * UTC - New York -5
	 * Standard +0
	 */
	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 6, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sun Nov 05 01:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  5 01:00:00 2017\n", out);
#endif

	CuAssertTrue(tc, (t = datetime2ts(2017, 11, 5, 7, 0, 0)) >= 0);
	CuAssertIntEquals(tc, 0, localtime_l(t, &tm, "America/New_York"));
	out = asctime(&tm);
#ifdef __MINGW32__
	CuAssertStrEquals(tc, "Sun Nov 05 02:00:00 2017\n", out);
#else
	CuAssertStrEquals(tc, "Sun Nov  5 02:00:00 2017\n", out);
#endif

	datetime_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_datefix(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	datetime_init();

	int year = 0, month = 0, day = 0;
	int hour = 0, minute = 0, second = 0;
	char result[255];

	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "-1 11 30 0 0 0", result);

	year = 2016; month = 9; day = 27; hour = 21; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 9 27 21 28 59", result);

	year = 2016; month = 13; day = 27; hour = 21; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2017 1 27 21 28 59", result);

	year = 2016; month = 12; day = 32; hour = 21; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2017 1 1 21 28 59", result);

	year = 2015; month = 2; day = 29; hour = 21; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2015 3 1 21 28 59", result);

	year = 2016; month = 9; day = 27; hour = 25; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 9 28 1 28 59", result);

	year = 2016; month = 9; day = 27; hour = 21; minute = 61; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 9 27 22 1 59", result);

	year = 2016; month = 9; day = 27; hour = 21; minute = 28; second = 61;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 9 27 21 29 1", result);

	year = 2016; month = -1; day = 27; hour = 21; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2015 11 27 21 28 59", result);

	year = 2016; month = 9; day = -10; hour = 21; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 8 21 21 28 59", result);

	year = 2016; month = 9; day = -365; hour = 21; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2015 9 1 21 28 59", result);

	year = 2016; month = 9; day = 27; hour = -128; minute = 28; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 9 21 16 28 59", result);

	year = 2016; month = 9; day = 27; hour = 21; minute = -1; second = 59;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 9 27 20 59 59", result);

	year = 2016; month = 9; day = 27; hour = 21; minute = 28; second = -1;
	datefix(&year, &month, &day, &hour, &minute, &second);
	sprintf(result, "%d %d %d %d %d %d", year, month, day, hour, minute, second);
	CuAssertStrEquals(tc, "2016 9 27 21 27 59", result);

	datetime_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_datetime(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_coord2tz);
	SUITE_ADD_TEST(suite, test_datetime2ts);
	SUITE_ADD_TEST(suite, test_localtime_l);
	SUITE_ADD_TEST(suite, test_datefix);

	return suite;
}