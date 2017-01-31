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
#include "../libs/pilight/events/operators/and.h"
#include "../libs/pilight/events/operators/divide.h"
#include "../libs/pilight/events/operators/eq.h"
#include "../libs/pilight/events/operators/ge.h"
#include "../libs/pilight/events/operators/gt.h"
#include "../libs/pilight/events/operators/intdivide.h"
#include "../libs/pilight/events/operators/is.h"
#include "../libs/pilight/events/operators/le.h"
#include "../libs/pilight/events/operators/lt.h"
#include "../libs/pilight/events/operators/minus.h"
#include "../libs/pilight/events/operators/modulus.h"
#include "../libs/pilight/events/operators/multiply.h"
#include "../libs/pilight/events/operators/ne.h"
#include "../libs/pilight/events/operators/plus.h"
#include "../libs/pilight/events/operators/or.h"

#include "alltests.h"

static void test_event_operator_and(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorAndInit();
	CuAssertStrEquals(tc, "AND", operator_and->name);
	CuAssertPtrEquals(tc, NULL, operator_and->callback_string);

	operator_and->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_and->callback_number(0, 1, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_and->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_and->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_and->callback_number(2, -1, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_and->callback_number(2, 2, &p);
	CuAssertStrEquals(tc, "1", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_divide(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorDivideInit();
	CuAssertStrEquals(tc, "/", operator_divide->name);
	CuAssertPtrEquals(tc, NULL, operator_divide->callback_string);

	operator_divide->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "1.000000", p);

	operator_divide->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_divide->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_divide->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "0.500000", p);

	operator_divide->callback_number(-2, 4, &p);
	CuAssertStrEquals(tc, "-0.500000", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_eq(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorEqInit();
	CuAssertStrEquals(tc, "==", operator_eq->name);
	CuAssertPtrEquals(tc, NULL, operator_eq->callback_string);

	operator_eq->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_eq->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_eq->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_eq->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_eq->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_eq->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_eq->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_eq->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "0", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_ge(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorGeInit();
	CuAssertStrEquals(tc, ">=", operator_ge->name);
	CuAssertPtrEquals(tc, NULL, operator_ge->callback_string);

	operator_ge->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ge->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ge->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_ge->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ge->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ge->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ge->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "0", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_gt(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorGtInit();
	CuAssertStrEquals(tc, ">", operator_gt->name);
	CuAssertPtrEquals(tc, NULL, operator_gt->callback_string);

	operator_gt->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_gt->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_gt->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_gt->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_gt->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_gt->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_gt->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "0", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_intdivide(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorIntDivideInit();
	CuAssertStrEquals(tc, "\\", operator_int_divide->name);
	CuAssertPtrEquals(tc, NULL, operator_int_divide->callback_string);

	operator_int_divide->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "1.000000", p);

	operator_int_divide->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_int_divide->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_int_divide->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_int_divide->callback_number(-2, 4, &p);
	CuAssertStrEquals(tc, "-0.000000", p);

	operator_int_divide->callback_number(10, 3, &p);
	CuAssertStrEquals(tc, "3.000000", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_is(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorIsInit();
	CuAssertStrEquals(tc, "IS", operator_is->name);
	CuAssertPtrEquals(tc, NULL, operator_is->callback_number);

	operator_is->callback_string("a", "b", &p);
	CuAssertStrEquals(tc, "0", p);

	operator_is->callback_string("a", "a", &p);
	CuAssertStrEquals(tc, "1", p);

	operator_is->callback_string("foo", "bar", &p);
	CuAssertStrEquals(tc, "0", p);

	operator_is->callback_string("Foo", "foo", &p);
	CuAssertStrEquals(tc, "0", p);

	operator_is->callback_string("foo", "foo", &p);
	CuAssertStrEquals(tc, "1", p);

	operator_is->callback_string("Hello World", "Hello World", &p);
	CuAssertStrEquals(tc, "1", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_le(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorLeInit();
	CuAssertStrEquals(tc, "<=", operator_le->name);
	CuAssertPtrEquals(tc, NULL, operator_le->callback_string);

	operator_le->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_le->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_le->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_le->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_le->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_le->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_le->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "1", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_lt(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorLtInit();
	CuAssertStrEquals(tc, "<", operator_lt->name);
	CuAssertPtrEquals(tc, NULL, operator_lt->callback_string);

	operator_lt->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_lt->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_lt->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_lt->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_lt->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_lt->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_lt->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "1", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_minus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorMinusInit();
	CuAssertStrEquals(tc, "-", operator_minus->name);
	CuAssertPtrEquals(tc, NULL, operator_minus->callback_string);

	operator_minus->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "1.000000", p);

	operator_minus->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_minus->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "-2.000000", p);

	operator_minus->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "2.000000", p);

	operator_minus->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_minus->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_minus->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "-0.000001", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_modulus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorModulusInit();
	CuAssertStrEquals(tc, "%", operator_modulus->name);
	CuAssertPtrEquals(tc, NULL, operator_modulus->callback_string);

	operator_modulus->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "-1", p);

	operator_modulus->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "-1", p);

	operator_modulus->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "2.000000", p);

	operator_modulus->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "-1", p);

	operator_modulus->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "-1", p);

	operator_modulus->callback_number(27, 16, &p);
	CuAssertStrEquals(tc, "11.000000", p);

	operator_modulus->callback_number(30, 3, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_modulus->callback_number(35, 3, &p);
	CuAssertStrEquals(tc, "2.000000", p);

	operator_modulus->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_modulus->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "2.012345", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_multiply(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorMultiplyInit();
	CuAssertStrEquals(tc, "*", operator_multiply->name);
	CuAssertPtrEquals(tc, NULL, operator_multiply->callback_string);

	operator_multiply->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "1.000000", p);

	operator_multiply->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_multiply->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_multiply->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "8.000000", p);

	operator_multiply->callback_number(-2, 4, &p);
	CuAssertStrEquals(tc, "-8.000000", p);

	operator_multiply->callback_number(10, 3, &p);
	CuAssertStrEquals(tc, "30.000000", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_ne(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorNeInit();
	CuAssertStrEquals(tc, "!=", operator_ne->name);
	CuAssertPtrEquals(tc, NULL, operator_ne->callback_string);

	operator_ne->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_ne->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ne->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_ne->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ne->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_ne->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_ne->callback_number(2.012345, 2.012345, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_ne->callback_number(2.012345, 2.012346, &p);
	CuAssertStrEquals(tc, "1", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_or(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorOrInit();
	CuAssertStrEquals(tc, "OR", operator_or->name);
	CuAssertPtrEquals(tc, NULL, operator_or->callback_string);

	operator_or->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_or->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_or->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_or->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "1", p);

	operator_or->callback_number(-2, -4, &p);
	CuAssertStrEquals(tc, "0", p);

	operator_or->callback_number(-2, -2, &p);
	CuAssertStrEquals(tc, "0", p);


	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_operator_plus(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char ret[255], *p = ret;

	memtrack();

	operatorPlusInit();
	CuAssertStrEquals(tc, "+", operator_plus->name);
	CuAssertPtrEquals(tc, NULL, operator_plus->callback_string);

	operator_plus->callback_number(1, 1, &p);
	CuAssertStrEquals(tc, "2.000000", p);

	operator_plus->callback_number(1, 0, &p);
	CuAssertStrEquals(tc, "1.000000", p);

	operator_plus->callback_number(0, 0, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	operator_plus->callback_number(2, 4, &p);
	CuAssertStrEquals(tc, "6.000000", p);

	operator_plus->callback_number(-2, 4, &p);
	CuAssertStrEquals(tc, "2.000000", p);

	operator_plus->callback_number(10, 3, &p);
	CuAssertStrEquals(tc, "13.000000", p);

	event_operator_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_operators(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_event_operator_and);
	SUITE_ADD_TEST(suite, test_event_operator_divide);
	SUITE_ADD_TEST(suite, test_event_operator_eq);
	SUITE_ADD_TEST(suite, test_event_operator_ge);
	SUITE_ADD_TEST(suite, test_event_operator_gt);
	SUITE_ADD_TEST(suite, test_event_operator_intdivide);
	SUITE_ADD_TEST(suite, test_event_operator_is);
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