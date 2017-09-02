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
#include "../libs/pilight/events/operator.h"

#include "alltests.h"

static void test_event_operator_and(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = empty; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = _false; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = zero; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_divide(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.500000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "-0.500000", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "4.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_eq(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char empty[] = "", foo[] = "foo", one[] = "1";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123456; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123457; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = empty; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_ge(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_gt(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_intdivide(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char foo[] = "foo", two[] = "2", one[] = "1";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 4; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "-0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 10; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "3.000000", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = one; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

// static void test_event_operator_is(CuTest *tc) {
	// printf("[ %-48s ]\n", __FUNCTION__);
	// fflush(stdout);

	// char ret[255], *p = ret;

	// memtrack();

	// operatorIsInit();
	// CuAssertStrEquals(tc, "IS", operator_is->name);
	// CuAssertPtrEquals(tc, NULL, operator_is->callback_number);

	// operator_is->callback_string("a", "b", &p);
	// CuAssertStrEquals(tc, "0", p);

	// operator_is->callback_string("a", "a", &p);
	// CuAssertStrEquals(tc, "1", p);

	// operator_is->callback_string("foo", "bar", &p);
	// CuAssertStrEquals(tc, "0", p);

	// operator_is->callback_string("Foo", "foo", &p);
	// CuAssertStrEquals(tc, "0", p);

	// operator_is->callback_string("foo", "foo", &p);
	// CuAssertStrEquals(tc, "1", p);

	// operator_is->callback_string("Hello World", "Hello World", &p);
	// CuAssertStrEquals(tc, "1", p);

	// event_operator_gc();
	// plua_gc();

	// CuAssertIntEquals(tc, 0, xfree());
// }

static void test_event_operator_le(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_lt(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_minus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "-2.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "-6.000000", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "-2.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "6.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.500000", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_modulus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "-2.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 27; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 16; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "11.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 30; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 35; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.012345", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_multiply(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "8.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "-8.000000", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "4.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "16.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.500000", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_ne(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char empty[] = "", foo[] = "foo", one[] = "1";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123456; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123457; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = empty; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_or(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = empty; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = _false; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = zero; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_plus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;
	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "6.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "4.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "2.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "10.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &p));
	CuAssertStrEquals(tc, "1.500000", p);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_operators(void) {
	CuSuite *suite = CuSuiteNew();

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"operators-root\":\"%s../libs/pilight/events/operators/\"},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_operators.c", "", &file);

	FILE *f = fopen("event_operator.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	SUITE_ADD_TEST(suite, test_event_operator_and);
	SUITE_ADD_TEST(suite, test_event_operator_divide);
	SUITE_ADD_TEST(suite, test_event_operator_eq);
	SUITE_ADD_TEST(suite, test_event_operator_ge);
	SUITE_ADD_TEST(suite, test_event_operator_gt);
	SUITE_ADD_TEST(suite, test_event_operator_intdivide);
	// SUITE_ADD_TEST(suite, test_event_operator_is);
	SUITE_ADD_TEST(suite, test_event_operator_le);
	SUITE_ADD_TEST(suite, test_event_operator_lt);
	SUITE_ADD_TEST(suite, test_event_operator_minus);
	SUITE_ADD_TEST(suite, test_event_operator_modulus);
	SUITE_ADD_TEST(suite, test_event_operator_multiply);
	SUITE_ADD_TEST(suite, test_event_operator_ne);
	SUITE_ADD_TEST(suite, test_event_operator_or);
	SUITE_ADD_TEST(suite, test_event_operator_plus);

	return suite;
}
