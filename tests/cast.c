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
#include "../libs/pilight/core/cast.h"

#include "alltests.h"

static void test_cast_bool(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t v;
	struct varcont_t *p = (void *)&v;

	memtrack();

	/*
	 * Numbers
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 1, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 0; v.type_ = JSON_NUMBER; v.decimals_ = 0;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 0, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 1.12345, v.type_ = JSON_NUMBER; v.decimals_ = 5;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 1, v.bool_);

	/*
	 * Strings
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("1"); v.type_ = JSON_STRING, v.free_ = 1;
	CuAssertPtrNotNull(tc, v.string_);
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 1, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = empty; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 0, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = two; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 1, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = foo; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 1, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = _false; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 1, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = zero; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 0, v.bool_);

	/*
	 * Boolean
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.bool_ = 1; v.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 1, v.bool_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.bool_ = 0; v.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, cast2bool(&p));
	CuAssertIntEquals(tc, JSON_BOOL, v.type_);
	CuAssertIntEquals(tc, 0, v.bool_);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_cast_string(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t v;
	struct varcont_t *p = (void *)&v;

	memtrack();

	/*
	 * Numbers
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "1", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 0; v.type_ = JSON_NUMBER; v.decimals_ = 0;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "0", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 1.12345, v.type_ = JSON_NUMBER; v.decimals_ = 5;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "1.12345", v.string_);
	FREE(v.string_);

	/*
	 * Strings
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("a"); v.type_ = JSON_STRING, v.free_ = 1;
	CuAssertPtrNotNull(tc, v.string_);
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "a", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = empty; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = two; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "2", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = foo; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "foo", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = _false; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "false", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = zero; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "0", v.string_);
	FREE(v.string_);

	/*
	 * Boolean
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.bool_ = 1; v.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "1", v.string_);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.bool_ = 0; v.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, cast2str(&p));
	CuAssertIntEquals(tc, JSON_STRING, v.type_);
	CuAssertStrEquals(tc, "0", v.string_);
	FREE(v.string_);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_cast_int(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char empty[] = "", foo[] = "foo", _false[] = "false";
	char zero[] = "0", two[] = "2";
	struct varcont_t v;
	struct varcont_t *p = (void *)&v;

	memtrack();

	/*
	 * Numbers
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 1, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 0; v.type_ = JSON_NUMBER; v.decimals_ = 0;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 0, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.number_ = 1.12345, v.type_ = JSON_NUMBER; v.decimals_ = 5;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertDblEquals(tc, 1.12345, v.number_, EPSILON);
	CuAssertIntEquals(tc, 5, v.decimals_);

	/*
	 * Strings
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("1"); v.type_ = JSON_STRING, v.free_ = 1;
	CuAssertPtrNotNull(tc, v.string_);
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 1, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = empty; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 0, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = two; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 2, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = foo; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 0, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = _false; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 0, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = zero; v.type_ = JSON_STRING;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 0, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	/*
	 * Boolean
	 */
	memset(&v, 0, sizeof(struct varcont_t));
	v.bool_ = 1; v.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 1, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.bool_ = 0; v.type_ = JSON_BOOL;
	CuAssertIntEquals(tc, 0, cast2int(&p));
	CuAssertIntEquals(tc, JSON_NUMBER, v.type_);
	CuAssertIntEquals(tc, 0, v.number_);
	CuAssertIntEquals(tc, 0, v.decimals_);

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_cast(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_cast_bool);
	SUITE_ADD_TEST(suite, test_cast_string);
	SUITE_ADD_TEST(suite, test_cast_int);

	return suite;
}