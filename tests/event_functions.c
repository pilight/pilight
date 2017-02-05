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
#include "../libs/pilight/core/binary.h"
#include "../libs/pilight/events/functions/date_add.h"
#include "../libs/pilight/events/functions/date_format.h"
#include "../libs/pilight/events/functions/random.h"

#include "alltests.h"

static void test_event_function_date_add(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	functionDateAddInit();

	/*
	 * Invalid parameters
	 */
	{
		/*
		 * No json parameters
		 */
		CuAssertIntEquals(tc, -1, function_date_add->run(NULL, NULL, &p, 0));

		struct JsonNode *json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:59:59"));

		CuAssertIntEquals(tc, -1, function_date_add->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Missing json parameters
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:59:59"));
		json_append_element(json, json_mkstring("1 DAY"));
		json_append_element(json, json_mkstring("FOO"));

		CuAssertIntEquals(tc, -1, function_date_add->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid unit number
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:59:59"));
		json_append_element(json, json_mkstring("a DAY"));

		CuAssertIntEquals(tc, -1, function_date_add->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid unit parameter
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:59:59"));
		json_append_element(json, json_mkstring("FOO"));

		CuAssertIntEquals(tc, -1, function_date_add->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid unit type
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:59:59"));
		json_append_element(json, json_mkstring("1 FOO"));

		CuAssertIntEquals(tc, -1, function_date_add->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid input dateformat
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017/02/28 23.59.59"));
		json_append_element(json, json_mkstring("1 DAY"));

		CuAssertIntEquals(tc, -1, function_date_add->run(NULL, json, &p, 0));
		json_delete(json);
	}

	/*
	 * Valid input parameters
	 */
	{
		/*
		 * Year interval
		 */
		struct JsonNode *json = json_mkarray();
		json_append_element(json, json_mkstring("2016-02-29 21:49:49"));
		json_append_element(json, json_mkstring("1 YEAR"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2017-03-01 21:49:49\"", ret);
		json_delete(json);

		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-02-29 21:49:49"));
		json_append_element(json, json_mkstring("-1 YEAR"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2015-03-01 21:49:49\"", ret);
		json_delete(json);

		/*
		 * Month interval
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-01-31 21:49:49"));
		json_append_element(json, json_mkstring("1 MONTH"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2016-03-02 21:49:49\"", ret);
		json_delete(json);

		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-01-31 21:49:49"));
		json_append_element(json, json_mkstring("-1 MONTH"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2015-12-31 21:49:49\"", ret);
		json_delete(json);

		/*
		 * Day interval
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 21:49:49"));
		json_append_element(json, json_mkstring("1 DAY"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2017-03-01 21:49:49\"", ret);
		json_delete(json);

		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 21:49:49"));
		json_append_element(json, json_mkstring("-1 DAY"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2017-02-27 21:49:49\"", ret);
		json_delete(json);

		/*
		 * Hour interval
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:49:49"));
		json_append_element(json, json_mkstring("1 HOUR"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2017-03-01 00:49:49\"", ret);
		json_delete(json);

		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-03-01 00:49:49"));
		json_append_element(json, json_mkstring("-1 HOUR"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2016-02-29 23:49:49\"", ret);
		json_delete(json);

		/*
		 * Minute interval
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:59:49"));
		json_append_element(json, json_mkstring("1 MINUTE"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2017-03-01 00:00:49\"", ret);
		json_delete(json);

		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-03-01 00:00:49"));
		json_append_element(json, json_mkstring("-1 MINUTE"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2016-02-29 23:59:49\"", ret);
		json_delete(json);

		/*
		 * Second interval
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2017-02-28 23:59:59"));
		json_append_element(json, json_mkstring("1 SECOND"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2017-03-01 00:00:00\"", ret);
		json_delete(json);

		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-03-01 00:00:00"));
		json_append_element(json, json_mkstring("-1 SECOND"));

		CuAssertIntEquals(tc, 0, function_date_add->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "\"2016-02-29 23:59:59\"", ret);
		json_delete(json);
	}

	event_function_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_function_date_format(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	functionDateFormatInit();

	/*
	 * Invalid parameters
	 */
	{
		/*
		 * No json parameters
		 */
		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, NULL, &p, 0));

		/*
		 * Missing json parameters
		 */
		struct JsonNode *json = json_mkarray();
		json_append_element(json, json_mkstring("2016-02-29 21:49:49"));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Missing json parameters
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-02-29 21:49:49"));
		json_append_element(json, json_mkstring("%Y-%m-%d %H:%M:%S"));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid dateformat representation
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-02-29 21:49:49"));
		json_append_element(json, json_mkstring("%m-%Y-%d %H:%M:%S"));
		json_append_element(json, json_mkstring("%H.%M.%S %d/%m/%Y"));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid epoch year (< 1970)
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("1943-02-29 21:49:49"));
		json_append_element(json, json_mkstring("%Y-%m-%d %H:%M:%S"));
		json_append_element(json, json_mkstring("%H.%M.%S %d/%m/%Y"));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);
	}

	/*
	 * Valid input parameters
	 */
	{
		struct JsonNode *json = json_mkarray();
		json_append_element(json, json_mkstring("2016-02-29 21:49:49"));
		json_append_element(json, json_mkstring("%Y-%m-%d %H:%M:%S"));
		json_append_element(json, json_mkstring("%H.%M.%S %d/%m/%Y"));

		CuAssertIntEquals(tc, 0, function_date_format->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "21.49.49 29/02/2016", ret);
		json_delete(json);

		json = json_mkarray();
		json_append_element(json, json_mkstring("2016-02-29 21:49:49"));
		json_append_element(json, json_mkstring("%Y-%m-%d %H:%M:%S"));
		json_append_element(json, json_mkstring("%c"));

		CuAssertIntEquals(tc, 0, function_date_format->run(NULL, json, &p, 0));
		CuAssertStrEquals(tc, "Mon Feb 29 21:49:49 2016", ret);
		json_delete(json);
	}

	event_function_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_function_random(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	int res[2][10] = { { 0 }, { 0 } };

	memtrack();

	functionRandomInit();

	/*
	 * Invalid parameters
	 */
	{
		/*
		 * Missing json parameters
		 */
		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, NULL, &p, 0));

		/*
		 * Invalid json parameters
		 */
		struct JsonNode *json = json_mkarray();
		json_append_element(json, json_mkstring("0"));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Too many json parameters
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("0"));
		json_append_element(json, json_mkstring("1"));
		json_append_element(json, json_mkstring("2"));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid json parameters
		 */
		json = json_mkarray();
		json_append_element(json, json_mknumber(0, 0));
		json_append_element(json, json_mknumber(1, 0));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);

		/*
		 * Invalid json parameters
		 */
		json = json_mkarray();
		json_append_element(json, json_mkstring("a"));
		json_append_element(json, json_mkstring("b"));

		CuAssertIntEquals(tc, -1, function_date_format->run(NULL, json, &p, 0));
		json_delete(json);
	}

	/*
	 * Valid input parameters
	 */
	{
		int i = 0;
		for(i=0;i<10000;i++) {
			struct JsonNode *json = json_mkarray();
			json_append_element(json, json_mkstring("0"));
			json_append_element(json, json_mkstring("10"));

			CuAssertIntEquals(tc, 0, function_date_format->run(NULL, json, &p, 0));
			CuAssertTrue(tc, (atoi(p) >= 0 && atoi(p) <= 10));
			res[0][atoi(p)]++;
			json_delete(json);
		}

		for(i=0;i<10;i++) {
			CuAssertTrue(tc, (res[0][i] > 800) && (res[0][1] < 1000));
		}

		i = 0;
		for(i=0;i<10000;i++) {
			struct JsonNode *json = json_mkarray();
			json_append_element(json, json_mkstring("0"));
			json_append_element(json, json_mkstring("10"));

			CuAssertIntEquals(tc, 0, function_date_format->run(NULL, json, &p, 0));
			CuAssertTrue(tc, (atoi(p) >= 0 && atoi(p) <= 10));
			res[1][atoi(p)]++;
			json_delete(json);
		}

		for(i=0;i<10;i++) {
			CuAssertTrue(tc, (res[1][i] > 800) && (res[1][1] < 1000));
		}
	}

	event_function_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_functions(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_event_function_date_add);
	SUITE_ADD_TEST(suite, test_event_function_date_format);
	SUITE_ADD_TEST(suite, test_event_function_random);

	return suite;
}