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
#include "../libs/pilight/lua/lua.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/protocols/protocol.h"

#include "alltests.h"

static void test_event_function_date_add(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char *out = NULL;
	struct varcont_t v;
	memtrack();

	protocol_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_function.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_function_init();

	/*
	 * Invalid parameters
	 */
	{
		{
			/*
			 * No json parameters
			 */
			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", NULL, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

	/*
	 * Missing json parameters
	 */
	 {
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
	 }

	 {
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("FOO"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
	 }

	 {
			/*
			 * Invalid unit number
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("a DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
	 }

	 {
			/*
			 * Invalid unit parameter
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("FOO"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
	 }

	 {
			/*
			 * Invalid unit type
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 FOO"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
	 }

	 {
			/*
			 * Invalid input dateformat
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017/02/28 23.59.59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}
	}

	/*
	 * Valid input parameters
	 */
	{
		{
			/*
			 * Year interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 YEAR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2017-03-01 21:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Year interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 YEAR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2015-03-01 21:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Month interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-01-31 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 MONTH"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2016-03-02 21:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Month interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-01-31 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 MONTH"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2015-12-31 21:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Day interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2017-03-01 21:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Day interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2017-02-27 21:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Hour interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 HOUR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2017-03-01 00:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Hour interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-03-01 00:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 HOUR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2016-02-29 23:49:49", out);
			FREE(out);
		}

		{
			/*
			 * Minute interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 MINUTE"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2017-03-01 00:00:49", out);
			FREE(out);
		}

		{
			/*
			 * Minute interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-03-01 00:00:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 MINUTE"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2016-02-29 23:59:49", out);
			FREE(out);
		}

		{
			/*
			 * Second interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2017-03-01 00:00:00", out);
			FREE(out);
		}

		{
			/*
			 * Second interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-03-01 00:00:00"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 SECOND"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2016-02-29 23:59:59", out);
			FREE(out);
		}

		{
			/*
			 * Get time from datetime protocol
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &out));
			CuAssertStrEquals(tc, "2015-01-28 14:37:08", out);
			FREE(out);
		}
	}

	protocol_gc();
	storage_gc();
	event_function_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_function_date_format(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char *out = NULL;
	struct varcont_t v;
	memtrack();

	protocol_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_function.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_function_init();

	/*
	 * Invalid parameters
	 */
	{
		{
			/*
			 * No json parameters
			 */
			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", NULL, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

		{
			/*
			 * Missing parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

		{
			/*
			 * Missing parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y-%m-%d %H:%M:%S"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

		{
			/*
			 * Invalid dateformat representation
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%m-%Y-%d %H:%M:%S"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%H.%M.%S %d/%m/%Y"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

		{
			/*
			 * Too many values with numeric
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1943-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y-%m-%d %H:%M:%S"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%H.%M.%S %d/%m/%Y"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1943"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}
	}

	/*
	 * Valid input parameters
	 */
	{
		{
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y-%m-%d %H:%M:%S"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%H.%M.%S %d/%m/%Y"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertStrEquals(tc, "21.49.49 29/02/2016", out);
			FREE(out);
		}

		{
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y-%m-%d %H:%M:%S"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%c"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertStrEquals(tc, "Mon Feb 29 21:49:49 2016", out);
			FREE(out);
		}

		{
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y-%m-%d"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%d"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertStrEquals(tc, "29", out);
			FREE(out);
		}

		{
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016,02,29"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y,%m,%d"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%d"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertStrEquals(tc, "29", out);
			FREE(out);
		}

		{
			/*
			 * Before valid epoch year (< 1970)
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1943-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y-%m-%d %H:%M:%S"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%H.%M.%S %d/%m/%Y"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertStrEquals(tc, "21.49.49 01/03/1943", out);
			FREE(out);
		}

		{
			/*
			 * Get time from datetime protocol
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%H.%M.%S %d/%m/%Y"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &out));
			CuAssertStrEquals(tc, "14.37.08 27/01/2015", out);
			FREE(out);
		}
	}

	protocol_gc();
	storage_gc();
	event_function_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_function_random(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);


	char *out = NULL;
	struct varcont_t v;
	unsigned int res[2][11] = { { 0 }, { 0 } };
	memtrack();

	protocol_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_function.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_function_init();

	/*
	 * Invalid parameters
	 */
	{
		{
			/*
			 * Missing json parameters
			 */
			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", NULL, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

		{
			/*
			 * Invalid json parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

		{
			/*
			 * Too many json parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}

		{
			/*
			 * Invalid json parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("a"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("b"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", args, &out));
			CuAssertPtrEquals(tc, NULL, out);
		}
	}

	/*
	 * Valid input parameters
	 */
	{
		int i = 0;
		for(i=0;i<10000;i++) {
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("10"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("RANDOM", args, &out));
			CuAssertPtrNotNull(tc, out);
			CuAssertTrue(tc, (atoi(out) >= 0 && atoi(out) <= 10));
			res[0][atoi(out)]++;
			FREE(out);
		}

		for(i=0;i<10;i++) {
			CuAssertTrue(tc, (res[0][i] > 600) && (res[0][i] < 1200));
		}

		i = 0;
		for(i=0;i<10000;i++) {
			struct event_function_args_t *args = NULL;
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("10"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("RANDOM", args, &out));
			CuAssertPtrNotNull(tc, out);
			CuAssertTrue(tc, (atoi(out) >= 0 && atoi(out) <= 10));
			res[1][atoi(out)]++;
			FREE(out);
		}

		for(i=0;i<10;i++) {
			CuAssertTrue(tc, (res[1][i] > 600) && (res[1][i] < 1200));
		}
	}

	protocol_gc();
	storage_gc();
	event_function_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_functions(void) {
	CuSuite *suite = CuSuiteNew();

	char config[1024] = "{\"devices\":{\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"year\":2015,\"month\":1,\"day\":27,\"hour\":14,\"minute\":37,\"second\":8,\"weekday\":3,\"dst\":1}},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"functions-root\":\"%s../libs/pilight/events/functions/\"},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_functions.c", "", &file);

	FILE *f = fopen("event_function.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	SUITE_ADD_TEST(suite, test_event_function_date_add);
	SUITE_ADD_TEST(suite, test_event_function_date_format);
	SUITE_ADD_TEST(suite, test_event_function_random);

	return suite;
}
