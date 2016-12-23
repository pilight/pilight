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
#include "../libs/pilight/core/options.h"

#include "alltests.h"

static void test_options_valid(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct options_t *options = NULL;
	char *args = NULL, *argv[12] = {
		"./pilight-unittest",
		"-A",
		"-a",
		"-B=1",
		"-b=1",
		"-C",
		"-c=a",
		"-D=1",
		"--foo=bar",
		"--server=127.0.0.1",
		"--test=\"aap noot\"",
		"--test1=\"-15\""
	};
	int argc = sizeof(argv)/sizeof(argv[0]), check = 0;

	options_add(&options, 'A', "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'a', "ac", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'B', "ba", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'b', "bc", OPTION_OPT_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'C', "ca", OPTION_OPT_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'c', "cb", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, 'D', "da", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&options, 1, "foo", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, 2, "server", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, 3, "test", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, 4, "test1", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

	while(1) {
		int c = options_parse(&options, argc, argv, 1, &args);
		if(c == -1) {
			break;
		}
		if(c == -2) {
			break;
		}
		switch(c) {
			case 'A':
			case 'a':
			case 'C':
				CuAssertIntEquals(tc, 0, strlen(args));
				check++;
			break;
			case 'B':
			case 'b':
			case 'D':
				CuAssertStrEquals(tc, "1", args);
				check++;
			break;
			case 'c':
				CuAssertStrEquals(tc, "a", args);
				check++;
			break;
			case 1:
				CuAssertStrEquals(tc, "bar", args);
				check++;
			break;
			case 2:
				CuAssertStrEquals(tc, "127.0.0.1", args);
				check++;
			break;
			case 3:
				CuAssertStrEquals(tc, "\"aap noot\"", args);
				check++;
			break;
			case 4:
				CuAssertStrEquals(tc, "\"-15\"", args);
				check++;
			break;
		}
	}
	options_delete(options);

	options_gc();

	CuAssertIntEquals(tc, 11, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static int options_test(char *a, int b, char *c, int d, int e, int f, char *g) {
	struct options_t *options = NULL;
	char *args = NULL, *argv[2] = { "./pilight-unittest", a };
	int argc = 0;
	argc = sizeof(argv)/sizeof(argv[0]);

	options_add(&options, b, c, d, e, f, NULL, g);

	int z = options_parse(&options, argc, argv, 1, &args);
	if(args != NULL) {
		FREE(args);
	}
	options_delete(options);
	return z;
}

static void test_options_invalid(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int r = options_test("-Z", 'A', "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	CuAssertIntEquals(tc, -2, r);

	r = options_test("-foo", 'A', "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	CuAssertIntEquals(tc, -2, r);

	r = options_test("-foo", 'A', "foo", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	CuAssertIntEquals(tc, -2, r);

	r = options_test("--foo", 'A', "foo", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	CuAssertIntEquals(tc, 65, r);

	/*
	 * This should fail in the new library
	r = options_test("-A 1", 'A', "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	CuAssertIntEquals(tc, 0, r);

	r = options_test("-A", 'A', "ab", OPTION_HAS_VALUE, 0, JSON_NULL, NULL);
	CuAssertIntEquals(tc, 0, r);

	r = options_test("-A 1", 'A', "ab", OPTION_HAS_VALUE, 0, JSON_STRING, NULL);
	CuAssertIntEquals(tc, 0, r);

	r = options_test("-A a", 'A', "ab", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL);
	CuAssertIntEquals(tc, 0, r);
	*/

	r = options_test("--foo=-1", 'A', "foo", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	CuAssertIntEquals(tc, -1, r);

	options_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_options_merge(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct options_t *options = NULL;
	struct options_t *options1 = NULL;
	char *args = NULL, *argv[12] = {
		"./pilight-unittest",
		"-A",
		"-a",
		"-B=1",
		"-b=1",
		"-C",
		"-c=a",
		"-D=1",
		"--foo=bar",
		"--server=127.0.0.1",
		"--test=\"aap noot\"",
		"--test1=\"-15\""
	};
	int argc = sizeof(argv)/sizeof(argv[0]), i = 0;

	options_add(&options, 'A', "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'a', "ac", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'B', "ba", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'b', "bc", OPTION_OPT_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'C', "ca", OPTION_OPT_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'c', "cb", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, 'D', "da", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&options1, 1, "foo", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, 2, "server", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, 3, "test", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, 4, "test1", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

	
	for(i=0;i<7;i++) {
		int c = options_parse(&options, argc, argv, 1, &args);
		CuAssertTrue(tc, (i != 0 || (i == 0 && c == 65)));
		CuAssertTrue(tc, (i != 1 || (i == 1 && c == 97)));
		CuAssertTrue(tc, (i != 2 || (i == 2 && c == 66)));
		CuAssertTrue(tc, (i != 3 || (i == 3 && c == 98)));
		CuAssertTrue(tc, (i != 4 || (i == 4 && c == 67)));
		CuAssertTrue(tc, (i != 5 || (i == 5 && c == 99)));
		CuAssertTrue(tc, (i != 6 || (i == 6 && c == -2)));
	}
	options_merge(&options, &options1);

	for(i=0;i<12;i++) {
		int c = options_parse(&options, argc, argv, 1, &args);
		CuAssertTrue(tc, (i != 0 || (i == 0 && c == 65)));
		CuAssertTrue(tc, (i != 1 || (i == 1 && c == 97)));
		CuAssertTrue(tc, (i != 2 || (i == 2 && c == 66)));
		CuAssertTrue(tc, (i != 3 || (i == 3 && c == 98)));
		CuAssertTrue(tc, (i != 4 || (i == 4 && c == 67)));
		CuAssertTrue(tc, (i != 5 || (i == 5 && c == 99)));
		CuAssertTrue(tc, (i != 6 || (i == 6 && c == 68)));
		CuAssertTrue(tc, (i != 7 || (i == 7 && c == 1)));
		CuAssertTrue(tc, (i != 8 || (i == 8 && c == 2)));
		CuAssertTrue(tc, (i != 9 || (i == 9 && c == 3)));
		CuAssertTrue(tc, (i != 10 || (i == 10 && c == 4)));
		CuAssertTrue(tc, (i != 11 || (i == 11 && c == -1)));
	}	
	options_delete(options);
	options_delete(options1);

	options_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_options(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_options_valid);
	SUITE_ADD_TEST(suite, test_options_invalid);
	SUITE_ADD_TEST(suite, test_options_merge);

	return suite;
}