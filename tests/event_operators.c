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
	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t v1;
	struct varcont_t v2;

	memtrack();

	operatorAndInit();
	CuAssertStrEquals(tc, "AND", operator_and->name);
	CuAssertPtrNotNull(tc, operator_and->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = empty; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = _false; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = zero; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);		

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	operator_and->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	
	
	event_operator_gc();

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

	operatorDivideInit();
	CuAssertStrEquals(tc, "/", operator_divide->name);
	CuAssertPtrNotNull(tc, operator_divide->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.500000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "-0.500000", p);	
	
	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "4.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	operator_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);
	event_operator_gc();	

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

	operatorEqInit();
	CuAssertStrEquals(tc, "==", operator_eq->name);
	CuAssertPtrNotNull(tc, operator_eq->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123456; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123457; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = empty; v2.type_ = JSON_STRING;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);		

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	operator_eq->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);		

	event_operator_gc();

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

	operatorGeInit();
	CuAssertStrEquals(tc, ">=", operator_ge->name);
	CuAssertPtrNotNull(tc, operator_ge->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_ge->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	event_operator_gc();

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

	operatorGtInit();
	CuAssertStrEquals(tc, ">", operator_gt->name);
	CuAssertPtrNotNull(tc, operator_gt->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_gt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	event_operator_gc();

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

	operatorIntDivideInit();
	CuAssertStrEquals(tc, "\\", operator_int_divide->name);
	CuAssertPtrNotNull(tc, operator_int_divide->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 4; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "-0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 10; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "3.000000", p);	

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = one; v2.type_ = JSON_STRING;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = one; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);		

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	operator_int_divide->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);		

	event_operator_gc();

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

	operatorLeInit();
	CuAssertStrEquals(tc, "<=", operator_le->name);
	CuAssertPtrNotNull(tc, operator_le->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_le->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	event_operator_gc();

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

	operatorLtInit();
	CuAssertStrEquals(tc, "<", operator_lt->name);
	CuAssertPtrNotNull(tc, operator_lt->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v1.decimals_ = 0;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v1.decimals_ = 6;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo1; v2.type_ = JSON_STRING;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo1; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = food; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = food; v2.type_ = JSON_STRING;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = zero; v2.type_ = JSON_STRING;
	operator_lt->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);
	
	event_operator_gc();

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

	operatorMinusInit();
	CuAssertStrEquals(tc, "-", operator_minus->name);
	CuAssertPtrNotNull(tc, operator_minus->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "-2.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "-6.000000", p);	

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "-2.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "6.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	operator_minus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.500000", p);

	event_operator_gc();	

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

	operatorModulusInit();
	CuAssertStrEquals(tc, "%", operator_modulus->name);
	CuAssertPtrNotNull(tc, operator_modulus->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "-2.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 27; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 16; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "11.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 30; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 35; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 3; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2.012346; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.012345", p);	

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	operator_modulus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	event_operator_gc();

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

	operatorMultiplyInit();
	CuAssertStrEquals(tc, "*", operator_multiply->name);
	CuAssertPtrNotNull(tc, operator_multiply->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "8.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "-8.000000", p);	
	
	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "4.000000", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "16.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	operator_multiply->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.500000", p);

	event_operator_gc();	

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

	operatorNeInit();
	CuAssertStrEquals(tc, "!=", operator_ne->name);
	CuAssertPtrNotNull(tc, operator_ne->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.012345; v1.type_ = JSON_NUMBER; v1.decimals_ = 6;
	v2.number_ = 2.012345; v2.type_ = JSON_NUMBER; v2.decimals_ = 6;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123456; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2.0123456; v1.type_ = JSON_NUMBER; v1.decimals_ = 7;
	v2.number_ = 2.0123457; v2.type_ = JSON_NUMBER; v2.decimals_ = 7;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = empty; v2.type_ = JSON_STRING;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);		

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = one; v2.type_ = JSON_STRING;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	operator_ne->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);		

	event_operator_gc();

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

	operatorOrInit();
	CuAssertStrEquals(tc, "OR", operator_or->name);
	CuAssertPtrNotNull(tc, operator_or->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = -1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 2; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;	
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = empty; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = _false; v1.type_ = JSON_STRING;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = zero; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);		

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = foo; v2.type_ = JSON_STRING;
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 123; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.bool_ = 1; v2.type_ = JSON_BOOL;
	operator_or->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1", p);	
	
	event_operator_gc();

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

	operatorPlusInit();
	CuAssertStrEquals(tc, "+", operator_plus->name);
	CuAssertPtrNotNull(tc, operator_plus->callback);

	/*
	 * Numbers
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 1; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 1; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 0; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 0; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "0.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "6.000000", p);	

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = -2; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.number_ = 4; v2.type_ = JSON_NUMBER; v2.decimals_ = 0;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);	
	
	/*
	 * Strings
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = two; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "4.000000", p);		

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.string_ = foo; v1.type_ = JSON_STRING;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "2.000000", p);

	/*
	 * Mixed variable types
	 */
	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.number_ = 8; v1.type_ = JSON_NUMBER; v1.decimals_ = 0;
	v2.string_ = two; v2.type_ = JSON_STRING;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "10.000000", p);

	memset(&v1, 0, sizeof(struct varcont_t));
	memset(&v2, 0, sizeof(struct varcont_t));
	v1.bool_ = 2; v1.type_ = JSON_BOOL;
	v2.string_ = half; v2.type_ = JSON_STRING;
	operator_plus->callback(&v1, &v2, &p);
	CuAssertStrEquals(tc, "1.500000", p);

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