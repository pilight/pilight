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

#include "alltests.h"

char *z = NULL;
char *y = NULL;

static void test_unittest1(CuTest *tc) {
	CuFail(tc, "foo"); // fail
}

static void test_unittest2(CuTest *tc) {
	CuAssert(tc, "bar", 1 == 0); // fail
}

static void test_unittest3(CuTest *tc) {
	CuAssert(tc, "bar", 1 == 1); // success
}

static void test_unittest4(CuTest *tc) {
	CuAssertTrue(tc, 0); // fail
}

static void test_unittest5(CuTest *tc) {
	CuAssertTrue(tc, 1); // success
}

static void test_unittest6(CuTest *tc) {
	CuAssertTrue(tc, -1); // success
}

static void test_unittest7(CuTest *tc) {
	CuAssertStrEquals(tc, "a", "b"); // fail
}

static void test_unittest8(CuTest *tc) {
	CuAssertStrEquals(tc, "a", "a"); // success
}

static void test_unittest9(CuTest *tc) {
	CuAssertStrEquals_Msg(tc, "foo", "a", "a"); // success
}

static void test_unittest10(CuTest *tc) {
	CuAssertStrEquals_Msg(tc, "bar", "a", "b"); // fail
}

static void test_unittest11(CuTest *tc) {
	CuAssertIntEquals(tc, 0, 1); // fail
}

static void test_unittest12(CuTest *tc) {
	CuAssertIntEquals(tc, 1, 1); // success
}

static void test_unittest13(CuTest *tc) {
	CuAssertIntEquals_Msg(tc, "foo", 1, 1); // success
}

static void test_unittest14(CuTest *tc) {
	CuAssertIntEquals_Msg(tc, "bar", 1, 0); // fail
}

static void test_unittest15(CuTest *tc) {
	CuAssertULongEquals(tc, 4294967295, 4294967295); // success
}

static void test_unittest16(CuTest *tc) {
	CuAssertULongEquals(tc, 4294967295, 4294967294); // fail
}

static void test_unittest17(CuTest *tc) {
	CuAssertULongEquals_Msg(tc, "foo", 4294967295, 4294967295); // success
}

static void test_unittest18(CuTest *tc) {
	CuAssertULongEquals_Msg(tc, "bar", 4294967295, 4294967294); // fail
}

static void test_unittest19(CuTest *tc) {
	CuAssertDblEquals(tc, 0.00001, 0.00002, 0.000001); // fail
}

static void test_unittest20(CuTest *tc) {
	CuAssertDblEquals(tc, 0.00001, 0.00001, 0.00001); // success
}

static void test_unittest21(CuTest *tc) {
	CuAssertDblEquals(tc, 0.00001, 0.00002, 0.0001); // success
}

static void test_unittest22(CuTest *tc) {
	CuAssertDblEquals_Msg(tc, "foo", 0.00001, 0.00002, 0.000001); // fail
}

static void test_unittest23(CuTest *tc) {
	CuAssertDblEquals_Msg(tc, "bar", 0.00001, 0.00001, 0.00001); // success
}

static void test_unittest24(CuTest *tc) {
	CuAssertDblEquals_Msg(tc, "bar", 0.00001, 0.00002, 0.0001); // success
}

static void test_unittest25(CuTest *tc) {
	char *a = y, *b = y;
	CuAssertPtrEquals(tc, a, b); // success
}

static void test_unittest26(CuTest *tc) {
	char *a = z, *b = y;
	CuAssertPtrEquals(tc, a, b); // fail
}

static void test_unittest27(CuTest *tc) {
	char *a = y, *b = y;
	CuAssertPtrEquals_Msg(tc, "foo", a, b); // success
}

static void test_unittest28(CuTest *tc) {
	char *a = z, *b = y;
	CuAssertPtrEquals_Msg(tc, "bar", a, b); // fail
}

static void test_unittest29(CuTest *tc) {
	char *a = NULL;
	CuAssertPtrNotNull(tc, a); // fail
}

static void test_unittest30(CuTest *tc) {
	char *a = malloc(0);
	CuAssertPtrNotNull(tc, a); // success
	free(a);
}

static void test_unittest31(CuTest *tc) {
	char *a = NULL;
	CuAssertPtrNotNullMsg(tc, "foo", a); // fail
}

static void test_unittest32(CuTest *tc) {
	char *a = malloc(0);
	CuAssertPtrNotNullMsg(tc, "bar", a); // success
	free(a);
}

static void test_unittest33(CuTest *tc) {
	char x[512], *p = x;
	memset(p, '\0', 512);
	memset(p, 'a', 511);

	CuFail(tc, x); // fail
}

static void test_unittest34(CuTest *tc) {
	CuFail(tc, NULL); // fail
}

int test_unittest(void) {
	z = malloc(0), y = malloc(0);

	{
		CuString *tmp = CuStringNew();
		CuStringAppendChar(tmp, 'a');
		assert(tmp->size == 256);
		assert(strcmp(tmp->buffer, "a") == 0);
		CuStringDelete(tmp);
	}

	{
		CuString *tmp = CuStringNew();
		tmp->size = 2;
		CuStringInsert(tmp, "aabbcc", 12);
		assert(tmp->size == 263);
		CuStringInsert(tmp, "ccddee", 12);
		assert(strncmp(tmp->buffer,  "aabbccccddee", 12) == 0);
		assert(tmp->size == 263);
		CuStringDelete(tmp);
	}

	{
		CuString *output = NULL;
		CuSuite *suite = CuSuiteNew();
		CuSuite *suite1 = CuSuiteNew();
		SUITE_ADD_TEST(suite, test_unittest3);
		CuSuiteAddSuite(suite1, suite);

		CuSuiteRun(suite1);
		output = CuStringNew();
		CuSuiteDetails(suite1, output);
		assert(strcmp(output->buffer, "OK (1 test)\n") == 0);
		CuStringDelete(output);
		CuSuiteDelete(suite);
		free(suite1);
	}

	{
		CuSuite *suite = CuSuiteNew();
		CuString *output = CuStringNew();
		SUITE_ADD_TEST(suite, test_unittest1);

		CuSuiteRun(suite);
		CuSuiteDetails(suite, output);

		char a[512];
		memset(&a, '\0', 512);
		sprintf(a, "There was 1 failure:\n"\
	"1) test_unittest1: %s:22: foo\n"\
	"\n"\
	"!!!FAILURES!!!\n"\
	"Runs: 1 Passes: 0 Fails: 1\n", __FILE__);

		assert(strcmp(output->buffer, a) == 0);
		CuStringDelete(output);
		CuSuiteDelete(suite);
	}

	int fails = 0;
	{
		CuSuite *suite = CuSuiteNew();
		SUITE_ADD_TEST(suite, test_unittest1);
		SUITE_ADD_TEST(suite, test_unittest2);
		SUITE_ADD_TEST(suite, test_unittest3);
		SUITE_ADD_TEST(suite, test_unittest4);
		SUITE_ADD_TEST(suite, test_unittest5);
		SUITE_ADD_TEST(suite, test_unittest6);
		SUITE_ADD_TEST(suite, test_unittest7);
		SUITE_ADD_TEST(suite, test_unittest8);
		SUITE_ADD_TEST(suite, test_unittest9);
		SUITE_ADD_TEST(suite, test_unittest10);
		SUITE_ADD_TEST(suite, test_unittest11);
		SUITE_ADD_TEST(suite, test_unittest12);
		SUITE_ADD_TEST(suite, test_unittest13);
		SUITE_ADD_TEST(suite, test_unittest14);
		SUITE_ADD_TEST(suite, test_unittest15);
		SUITE_ADD_TEST(suite, test_unittest16);
		SUITE_ADD_TEST(suite, test_unittest17);
		SUITE_ADD_TEST(suite, test_unittest18);
		SUITE_ADD_TEST(suite, test_unittest19);
		SUITE_ADD_TEST(suite, test_unittest20);
		SUITE_ADD_TEST(suite, test_unittest21);
		SUITE_ADD_TEST(suite, test_unittest22);
		SUITE_ADD_TEST(suite, test_unittest23);
		SUITE_ADD_TEST(suite, test_unittest24);
		SUITE_ADD_TEST(suite, test_unittest25);
		SUITE_ADD_TEST(suite, test_unittest26);
		SUITE_ADD_TEST(suite, test_unittest27);
		SUITE_ADD_TEST(suite, test_unittest28);
		SUITE_ADD_TEST(suite, test_unittest29);
		SUITE_ADD_TEST(suite, test_unittest30);
		SUITE_ADD_TEST(suite, test_unittest31);
		SUITE_ADD_TEST(suite, test_unittest32);
		SUITE_ADD_TEST(suite, test_unittest33);
		SUITE_ADD_TEST(suite, test_unittest34);

		CuSuiteRun(suite);

		CuString *output = CuStringNew();
		CuSuiteSummary(suite, output);
		assert(strcmp(output->buffer, "FF.F..F..FF..F.F.FF..F...F.FF.F.FF\n\n") == 0);
		CuStringDelete(output);

		output = CuStringNew();
		CuSuiteDetails(suite, output);

		int len = strlen(output->buffer);
		assert(len > 2150 && len < 2250);
		free(output->buffer);
		free(output);

		fails = suite->failCount;
		CuSuiteDelete(suite);
	}
	free(z);
	free(y);

	return fails; // 17
}
