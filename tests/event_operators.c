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
#include "../libs/pilight/events/operator.h"

#include "alltests.h"

static void test_event_operator_and(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t ret;
	struct varcont_t v1;
	struct varcont_t v2;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("AND", &x));
	CuAssertIntEquals(tc, 20, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("AND", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = empty; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = _false; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = zero; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("AND", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_divide(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t ret;
	struct varcont_t v1;
	struct varcont_t v2;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("/", &x));
	CuAssertIntEquals(tc, 70, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("/", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertDblEquals(tc, ret.number_, 0.5, EPSILON);
	CuAssertIntEquals(tc, ret.decimals_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertDblEquals(tc, ret.number_, -0.5, EPSILON);
	CuAssertIntEquals(tc, ret.decimals_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -3.25; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 8; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertDblEquals(tc, ret.number_, -0.40625, EPSILON);
	CuAssertIntEquals(tc, ret.decimals_, 5);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertDblEquals(tc, ret.number_, 1, EPSILON);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertDblEquals(tc, ret.number_, 0, EPSILON);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertDblEquals(tc, ret.number_, 4, EPSILON);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("/", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertDblEquals(tc, ret.number_, 2, EPSILON);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_eq(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char empty[] = "", foo[] = "foo", one[] = "1";
	struct varcont_t v1;
	struct varcont_t v2;
	struct varcont_t ret;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("==", &x));
	CuAssertIntEquals(tc, 30, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("==", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123456; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123457; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = empty; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("==", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_ge(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t ret;
	struct varcont_t v1;
	struct varcont_t v2;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity(">=", &x));
	CuAssertIntEquals(tc, 30, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence(">=", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_gt(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t ret;
	struct varcont_t v1;
	struct varcont_t v2;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity(">", &x));
	CuAssertIntEquals(tc, 30, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence(">", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback(">", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_intdivide(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char foo[] = "foo", two[] = "2", one[] = "1";
	struct varcont_t ret;
	struct varcont_t v1;
	struct varcont_t v2;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("\\", &x));
	CuAssertIntEquals(tc, 70, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("\\", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 4; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 10; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 3);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = one; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("\\", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

// // static void test_event_operator_is(CuTest *tc) {
	// // printf("[ %-48s ]\n", __FUNCTION__);
	// // fflush(stdout);

	// // char ret[255], *p = ret;

	// // memtrack();

	// // operatorIsInit();
	// // CuAssertStrEquals(tc, "IS", operator_is->name);
	// // CuAssertPtrEquals(tc, NULL, operator_is->callback_number);

	// // operator_is->callback_string("a", "b", &p);
	// // CuAssertStrEquals(tc, "0", p);

	// // operator_is->callback_string("a", "a", &p);
	// // CuAssertStrEquals(tc, "1", p);

	// // operator_is->callback_string("foo", "bar", &p);
	// // CuAssertStrEquals(tc, "0", p);

	// // operator_is->callback_string("Foo", "foo", &p);
	// // CuAssertStrEquals(tc, "0", p);

	// // operator_is->callback_string("foo", "foo", &p);
	// // CuAssertStrEquals(tc, "1", p);

	// // operator_is->callback_string("Hello World", "Hello World", &p);
	// // CuAssertStrEquals(tc, "1", p);

	// // event_operator_gc();
	// // plua_gc();

	// // CuAssertIntEquals(tc, 0, xfree());
// // }

static void test_event_operator_le(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t ret;
	struct varcont_t v1;
	struct varcont_t v2;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("<=", &x));
	CuAssertIntEquals(tc, 30, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("<=", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_lt(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char foo[] = "foo", foo1[] = "Foo", food[] = "food";
	char zero[] = "0";
	struct varcont_t ret;
	struct varcont_t v1;
	struct varcont_t v2;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("<", &x));
	CuAssertIntEquals(tc, 30, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("<", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("<", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_minus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;
	struct varcont_t ret;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("-", &x));
	CuAssertIntEquals(tc, 60, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("-", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, -2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, -6);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, -2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 6);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("-", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0.5);
	CuAssertIntEquals(tc, ret.decimals_, 1);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_modulus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;
	struct varcont_t ret;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("%", &x));
	CuAssertIntEquals(tc, 70, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("%", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, -2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 27; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 16; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 11);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 30; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 35; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2.012345);
	CuAssertIntEquals(tc, ret.decimals_, 13);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("%", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_multiply(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;
	struct varcont_t ret;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("*", &x));
	CuAssertIntEquals(tc, 70, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("*", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2,&ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 8);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, -8);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 4);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 16);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("*", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0.5);
	CuAssertIntEquals(tc, ret.decimals_, 1);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_ne(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char empty[] = "", foo[] = "foo", one[] = "1";
	struct varcont_t v1;
	struct varcont_t v2;
	struct varcont_t ret;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("!=", &x));
	CuAssertIntEquals(tc, 30, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("!=", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123456; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123457; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = empty; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("!=", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_or(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t v1;
	struct varcont_t v2;
	struct varcont_t ret;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("OR", &x));
	CuAssertIntEquals(tc, 10, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("OR", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = empty; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = _false; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = zero; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2,&ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, event_operator_callback("OR", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_BOOL);
	CuAssertIntEquals(tc, ret.bool_, 1);

	storage_gc();
	event_operator_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_plus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char two[] = "2", half[] = "0.5", foo[] = "foo";
	struct varcont_t v1;
	struct varcont_t v2;
	struct varcont_t ret;
	int x = 0;

	memtrack();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_operators.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_operator.json", CONFIG_SETTINGS));
	event_operator_init();

	CuAssertIntEquals(tc, 0, event_operator_associativity("+", &x));
	CuAssertIntEquals(tc, 60, x);

	CuAssertIntEquals(tc, 0, event_operator_precedence("+", &x));
	CuAssertIntEquals(tc, 1, x);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 0);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 6);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 4);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 2);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 10);
	CuAssertIntEquals(tc, ret.decimals_, 0);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	memset(&ret, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, event_operator_callback("+", &v1, &v2, &ret));
	CuAssertIntEquals(tc, ret.type_, JSON_NUMBER);
	CuAssertIntEquals(tc, ret.number_, 1.5);
	CuAssertIntEquals(tc, ret.decimals_, 1);

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
	// // SUITE_ADD_TEST(suite, test_event_operator_is);
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
