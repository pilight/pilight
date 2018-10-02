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
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/protocols/protocol.h"

#include "alltests.h"

static void test_event_function_date_add(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	struct varcont_t ret;
	struct varcont_t v;
	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_functions.c");

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
			memset(&ret, 0, sizeof(struct varcont_t));
			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", NULL, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

	 {
			/*
			 * Missing json parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
	 }

	 {
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
	 }

	 {
			/*
			 * Invalid unit number
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("a DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
	 }

	 {
			/*
			 * Invalid unit parameter
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("FOO"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
	 }

	 {
			/*
			 * Invalid unit type
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 FOO"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
	 }

	 {
			/*
			 * Invalid input dateformat
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017/02/28 23.59.59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
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
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 YEAR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2017-03-01 21:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Year interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 YEAR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2015-03-01 21:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Month interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-01-31 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 MONTH"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2016-03-02 21:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Month interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-01-31 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 MONTH"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2015-12-31 21:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Day interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2017-03-01 21:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Day interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2017-02-27 21:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Hour interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 HOUR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2017-03-01 00:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Hour interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-03-01 00:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 HOUR"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2016-02-29 23:49:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Minute interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 MINUTE"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2017-03-01 00:00:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Minute interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-03-01 00:00:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 MINUTE"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2016-02-29 23:59:49", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Second interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2017-02-28 23:59:59"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2017-03-01 00:00:00", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Second interval
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-03-01 00:00:00"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("-1 SECOND"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2016-02-29 23:59:59", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Get time from datetime protocol
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 DAY"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_ADD", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "2015-01-28 14:37:08", ret.string_);
			FREE(ret.string_);
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

	struct varcont_t v;
	struct varcont_t ret;
	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_functions.c");

	protocol_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_function.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_function_init();

	/*
	 * Invalid parameters
	 */
	{
		/*
		 * No json parameters
		 */
		{
			memset(&ret, 0, sizeof(struct varcont_t));
			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", NULL, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Missing parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Missing parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("2016-02-29 21:49:49"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%Y-%m-%d %H:%M:%S"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Invalid dateformat representation
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Too many values with numeric
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, -1, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}
	}

	/*
	 * Valid input parameters
	 */
	{
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "21.49.49 29/02/2016", ret.string_);
			FREE(ret.string_);
		}

		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "Mon Feb 29 21:49:49 2016", ret.string_);
			FREE(ret.string_);
		}

		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, JSON_NUMBER, ret.type_);
			CuAssertIntEquals(tc, 29, ret.number_);
			CuAssertIntEquals(tc, 0, ret.decimals_);
		}

		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, JSON_NUMBER, ret.type_);
			CuAssertIntEquals(tc, 29, ret.number_);
			CuAssertIntEquals(tc, 0, ret.decimals_);
		}

		{
			/*
			 * Before valid epoch year (< 1970)
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "21.49.49 01/03/1943", ret.string_);
			FREE(ret.string_);
		}

		{
			/*
			 * Get time from datetime protocol
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("%H.%M.%S %d/%m/%Y"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("DATE_FORMAT", args, &ret));
			CuAssertIntEquals(tc, JSON_STRING, ret.type_);
			CuAssertStrEquals(tc, "14.37.08 27/01/2015", ret.string_);
			FREE(ret.string_);
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


	struct varcont_t v;
	struct varcont_t ret;
	unsigned int res[2][11] = { { 0 }, { 0 } };
	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_functions.c");

	protocol_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_function.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_function_init();

	/*
	 * Invalid parameters
	 */
	{
		/*
		 * Missing json parameters
		 */
		{
			memset(&ret, 0, sizeof(struct varcont_t));
			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", NULL, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Invalid json parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		{
			/*
			 * Too many json parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Invalid json parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("a"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("b"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("RANDOM", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}
	}

	/*
	 * Valid input parameters
	 */
	{
		int i = 0;
		for(i=0;i<10000;i++) {
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("10"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("RANDOM", args, &ret));
			CuAssertIntEquals(tc, JSON_NUMBER, ret.type_);
			CuAssertIntEquals(tc, 0, ret.decimals_);
			CuAssertTrue(tc, (ret.number_ >= 0 && ret.number_ <= 10));
			res[0][(int)ret.number_]++;
		}

		for(i=0;i<10;i++) {
			CuAssertTrue(tc, (res[0][i] > 600) && (res[0][i] < 1200));
		}

		i = 0;
		for(i=0;i<10000;i++) {
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("10"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, 0, event_function_callback("RANDOM", args, &ret));
			CuAssertIntEquals(tc, JSON_NUMBER, ret.type_);
			CuAssertIntEquals(tc, 0, ret.decimals_);
			CuAssertTrue(tc, (ret.number_ >= 0 && ret.number_ <= 10));
			res[1][(int)ret.number_]++;
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

static void test_event_function_min(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	struct varcont_t v;
	struct varcont_t ret;
	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_functions.c");

	protocol_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_function.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_function_init();

	/*
	 * Invalid parameters
	 */
	{
		/*
		 * Missing json parameters
		 */
		{
			memset(&ret, 0, sizeof(struct varcont_t));
			CuAssertIntEquals(tc, -1, event_function_callback("MIN", NULL, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Invalid json parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("MIN", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		{
			/*
			 * Too many json parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, -1, event_function_callback("MIN", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Invalid json parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("a"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("b"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("MIN", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}
	}

	/*
	 * Valid input parameters
	 */
	{
		struct event_function_args_t *args = NULL;
		memset(&ret, 0, sizeof(struct varcont_t));
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
		args = event_function_add_argument(&v, NULL);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("10"); v.type_ = JSON_STRING;
		args = event_function_add_argument(&v, args);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_function_callback("MIN", args, &ret));
		CuAssertIntEquals(tc, JSON_NUMBER, ret.type_);
		CuAssertIntEquals(tc, 0, ret.decimals_);
		CuAssertIntEquals(tc, 0, ret.number_);
	}

	protocol_gc();
	storage_gc();
	event_function_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_function_max(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	struct varcont_t v;
	struct varcont_t ret;
	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_functions.c");

	protocol_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_function.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_function_init();

	/*
	 * Invalid parameters
	 */
	{
		/*
		 * Missing json parameters
		 */
		{
			memset(&ret, 0, sizeof(struct varcont_t));
			CuAssertIntEquals(tc, -1, event_function_callback("MAX", NULL, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Invalid json parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("MAX", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		{
			/*
			 * Too many json parameters
			 */
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
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

			CuAssertIntEquals(tc, -1, event_function_callback("MAX", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}

		/*
		 * Invalid json parameters
		 */
		{
			struct event_function_args_t *args = NULL;
			memset(&ret, 0, sizeof(struct varcont_t));
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("a"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, NULL);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("b"); v.type_ = JSON_STRING;
			args = event_function_add_argument(&v, args);
			FREE(v.string_);

			CuAssertIntEquals(tc, -1, event_function_callback("MAX", args, &ret));
			CuAssertIntEquals(tc, 0, ret.type_);
		}
	}

	/*
	 * Valid input parameters
	 */
	{
		struct event_function_args_t *args = NULL;
		memset(&ret, 0, sizeof(struct varcont_t));
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("0"); v.type_ = JSON_STRING;
		args = event_function_add_argument(&v, NULL);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("10"); v.type_ = JSON_STRING;
		args = event_function_add_argument(&v, args);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_function_callback("MAX", args, &ret));
		CuAssertIntEquals(tc, JSON_NUMBER, ret.type_);
		CuAssertIntEquals(tc, 0, ret.decimals_);
		CuAssertIntEquals(tc, 10, ret.number_);
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
	SUITE_ADD_TEST(suite, test_event_function_min);
	SUITE_ADD_TEST(suite, test_event_function_max);

	return suite;
}
