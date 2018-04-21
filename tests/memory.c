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
#include "../libs/pilight/core/mem.h"

#include "alltests.h"

static void test_memory1(CuTest *tc) {
	{
		memtrack();

		char *a = MALLOC(128);
		CuAssertPtrNotNull(tc, a);

		int i = 0, check = 1;
		for(i=0;i<128;i++) {
			if((a[i] & 0xFF) != 0x00) {
				check = 1;
			}
		}
		CuAssertIntEquals(tc, 1, check);

		FREE(a);
		CuAssertPtrEquals(tc, NULL, a);

		CuAssertIntEquals(tc, 0, xfree());
	}

	{
		memtrack();
		char *a = MALLOC(128);
		CuAssertPtrNotNull(tc, a);

		int i = 0, check = 0;
		for(i=0;i<128;i++) {
			if((a[i] & 0xFF) != 0x00) {
				check = 1;
			}
		}
		CuAssertIntEquals(tc, 1, check);

		/*
		 * Make it smaller to ensure
		 * it fits in the previous
		 * memory address
		 */
		char *b = REALLOC(a, 64);
		CuAssertPtrEquals(tc, a, b);

		FREE(a);
		CuAssertPtrEquals(tc, NULL, a);

		CuAssertIntEquals(tc, 0, xfree());
	}

	{
		memtrack();

		char *a = STRDUP("foo");
		CuAssertPtrNotNull(tc, a);
		CuAssertIntEquals(tc, 3, strlen(a));
		CuAssertStrEquals(tc, "foo", a);
		FREE(a);
		CuAssertPtrEquals(tc, NULL, a);

		CuAssertIntEquals(tc, 0, xfree());
	}

	{
		memtrack();

		int line = 0;
		char *format = "CHECK: calling free on already freed pointer in %s at line #%d";
		char error[1024], *p = error;
		char *a = MALLOC(0);
		CuAssertPtrNotNull(tc, a);
		FREE(a);
		FREE(a); line = __LINE__;
		CuAssertPtrEquals(tc, NULL, a);

		CuAssertIntEquals(tc, 0, xfree());

		snprintf(p, 1024, format, __FILE__, line);
		printf("  %s\n", error);
	}

	{
		memtrack();

		char *a = CALLOC(sizeof(char), 128);
		CuAssertPtrNotNull(tc, a);

		int i = 0, check = 0;
		for(i=0;i<128;i++) {
			if((a[i] & 0xFF) != 0x00) {
				check = 1;
			}
		}
		CuAssertIntEquals(tc, 0, check);

		FREE(a);
		CuAssertPtrEquals(tc, NULL, a);

		CuAssertIntEquals(tc, 0, xfree());
	}

	{
		memtrack();

		int line = 0;
		char *format = "CHECK: unfreed pointer in %s at line #%d";
		char error[1024], *p = error;
		char *a = MALLOC(128); line = __LINE__;
		CuAssertPtrNotNull(tc, a);

		CuAssertIntEquals(tc, 1, xfree());

		snprintf(p, 1024, format, __FILE__, line);
		printf("  %s\n", error);
	}

	{
		memtrack();

		int line[2] = { 0 };
		char *format = "CHECK: unfreed pointer in %s at line #%d";
		char error[1024], *p = error;
		char *a = MALLOC(128); line[0] = __LINE__;
		CuAssertPtrNotNull(tc, a);

		char *b = MALLOC(128); line[1] = __LINE__;
		CuAssertPtrNotNull(tc, b);

		CuAssertIntEquals(tc, 2, xfree());

		snprintf(p, 1024, format, __FILE__, line[1]);
		printf("  %s\n", error);
		snprintf(p, 1024, format, __FILE__, line[0]);
		printf("  %s\n", error);
	}

	{
		memtrack();

		int line = 0;
		char *format = "CHECK: calling realloc on an unknown pointer in %s at line #%d";
		char error[1024], *p = error;
		char *b = malloc(0);
		char *a = REALLOC(b, 128); line = __LINE__;
		CuAssertPtrNotNull(tc, a);

		FREE(a);

		CuAssertIntEquals(tc, 0, xfree());

		snprintf(p, 1024, format, __FILE__, line);
		printf("%s\n", error);
	}

	{
		memtrack();

		int line = 0;
		char *format = "CHECK: calling free after xfree was called in %s at line #%d";
		char error[1024], *p = error;
		char *b = malloc(0);
		char *a = MALLOC(128);;
		CuAssertPtrNotNull(tc, a);
		FREE(a);
		CuAssertIntEquals(tc, 0, xfree());
		FREE(b); line = __LINE__;
		snprintf(p, 1024, format, __FILE__, line);
		printf("  %s\n", error);
	}

	{
		xabort();

		char *a = MALLOC(128);
		CuAssertPtrNotNull(tc, a);
		FREE(a);
		CuAssertIntEquals(tc, 0, xfree());
	}

	{
		xabort();

		char *a = REALLOC(NULL, 128);
		CuAssertPtrNotNull(tc, a);
		FREE(a);
		CuAssertIntEquals(tc, 0, xfree());
	}

	{
		xabort();

		char *a = CALLOC(sizeof(char), 128);
		CuAssertPtrNotNull(tc, a);
		FREE(a);
		CuAssertIntEquals(tc, 0, xfree());
	}

	{
		memtrack();

		int line = 0;
		char *format = "CHECK: trying to free an unknown pointer in %s at line #%d";
		char error[1024], *p = error;

		char *a = malloc(128);
		CuAssertPtrNotNull(tc, a);
		FREE(a); line = __LINE__;
		CuAssertIntEquals(tc, -1, xfree());

		snprintf(p, 1024, format, __FILE__, line);
		printf("%s\n", error);
	}
}

int test_memory(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_memory1);

	CuSuiteRun(suite);
	int x = suite->failCount;
	CuSuiteDelete(suite);

	return x;
}
