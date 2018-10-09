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

static void test_options_absent(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct options_t *options = NULL;
	char *argv[1] = {
		"./pilight-unittest"
	};
	int argc = sizeof(argv)/sizeof(argv[0]);

	options_add(&options, "A", "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	options_parse(options, argc, argv);

	options_delete(options);
	options_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_options_valid(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct options_t *options = NULL;
	char *argv[18] = {
		"./pilight-unittest",
		"-A",
		"-a",
		"-B=1",
		"-b=1",
		"-C",
		"-c=a",
		"-D=1",
		"-e=-a",
		"-Ls=2",
		"-Lt",
		"3",
		"--foo=bar",
		"--server=127.0.0.1",
		"--test=\"aap noot\"",
		"--test1=\"-15\"",
		"--test2=\"-15'",
		"--no-value"
	};
	int argc = sizeof(argv)/sizeof(argv[0]), check = 0;

	options_add(&options, "A", "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "a", "ac", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "B", "ba", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&options, "b", "bc", OPTION_OPT_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&options, "C", "ca", OPTION_OPT_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "c", "cb", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "D", "da", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&options, "e", "fe", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "Ls", "fe", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "Lt", "fg", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "1", "foo", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "2", "server", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "3", "test", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "4", "test1", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "5", "test2", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "6", "no-value", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	char *str = NULL;
	int num = 0, i = 0;
	options_parse(options, argc, argv);

	while(options_list(options, i++, &str) == 0) {
		if((strcmp(str, "A") == 0) || (strcmp(str, "a") == 0) ||
			(strcmp(str, "B") == 0) || (strcmp(str, "b") == 0) ||
			(strcmp(str, "C") == 0) || (strcmp(str, "c") == 0) ||
			(strcmp(str, "D") == 0) || (strcmp(str, "Ls") == 0) ||
			(strcmp(str, "Lt") == 0) || (strcmp(str, "1") == 0) ||
			(strcmp(str, "2") == 0) || (strcmp(str, "3") == 0) ||
			(strcmp(str, "4") == 0) || (strcmp(str, "5") == 0) ||
			(strcmp(str, "6") == 0) || (strcmp(str, "e") == 0)) {
			check++;
		}
	}

	CuAssertIntEquals(tc, 0, options_exists(options, "A"));
	CuAssertIntEquals(tc, 0, options_exists(options, "a"));

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "B"));
		CuAssertIntEquals(tc, 0, options_get_number(options, "B", &num));
		CuAssertIntEquals(tc, 1, num);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "b"));
		CuAssertIntEquals(tc, 0, options_get_number(options, "b", &num));
		CuAssertIntEquals(tc, 1, num);
	}

	CuAssertIntEquals(tc, 0, options_exists(options, "C"));

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "c"));
		CuAssertIntEquals(tc, 0, options_get_string(options, "c", &str));
		CuAssertStrEquals(tc, "a", str);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "D"));
		CuAssertIntEquals(tc, 0, options_get_number(options, "D", &num));
		CuAssertIntEquals(tc, 1, num);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "e"));
		CuAssertIntEquals(tc, 0, options_get_string(options, "e", &str));
		CuAssertStrEquals(tc, "-a", str);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "Ls"));
		CuAssertIntEquals(tc, 0, options_get_number(options, "Ls", &num));
		CuAssertIntEquals(tc, 2, num);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "Lt"));
		CuAssertIntEquals(tc, 0, options_get_number(options, "Lt", &num));
		CuAssertIntEquals(tc, 3, num);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "1"));
		CuAssertIntEquals(tc, 0, options_get_string(options, "1", &str));
		CuAssertStrEquals(tc, "bar", str);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "2"));
		CuAssertIntEquals(tc, 0, options_get_string(options, "2", &str));
		CuAssertStrEquals(tc, "127.0.0.1", str);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "3"));
		CuAssertIntEquals(tc, 0, options_get_string(options, "3", &str));
		CuAssertStrEquals(tc, "aap noot", str);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "4"));
		CuAssertIntEquals(tc, 0, options_get_number(options, "4", &num));
		CuAssertIntEquals(tc, -15, num);
	}

	{
		CuAssertIntEquals(tc, 0, options_exists(options, "5"));
		CuAssertIntEquals(tc, 0, options_get_string(options, "5", &str));
		CuAssertStrEquals(tc, "\"-15'", str);
	}

	CuAssertIntEquals(tc, 0, options_exists(options, "6"));

	options_delete(options);
	options_gc();

	CuAssertIntEquals(tc, 16, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static struct options_t *options_test(char *a, char *b, char *c, int d, int e, int f, char *g, char **ret) {
	struct options_t *options = NULL;
	char *args = NULL, *argv[2] = { "./pilight-unittest", a };
	int argc = 0;
	argc = sizeof(argv)/sizeof(argv[0]);

	options_add(&options, b, c, d, e, f, NULL, g);

	options_parse(options, argc, argv);
	if(args != NULL) {
		FREE(args);
	}
	return options;
}

static void test_options_invalid(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *ret = NULL, *str = NULL;
	struct options_t *options = options_test("-Z", "A", "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, &ret);
	CuAssertTrue(tc, ret == NULL);
	CuAssertIntEquals(tc, -1, options_exists(options, "A"));
	CuAssertIntEquals(tc, -1, options_get_string(options, "A", &str));
	CuAssertTrue(tc, str == NULL);
	options_delete(options);

	options = options_test("-foo", "A", "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, &ret);
	CuAssertTrue(tc, ret == NULL);
	CuAssertIntEquals(tc, -1, options_exists(options, "A"));
	CuAssertIntEquals(tc, -1, options_get_string(options, "A", &str));
	CuAssertTrue(tc, str == NULL);
	options_delete(options);

	options = options_test("-foo", "A", "foo", OPTION_NO_VALUE, 0, JSON_NULL, NULL, &ret);
	CuAssertTrue(tc, ret == NULL);
	CuAssertIntEquals(tc, -1, options_exists(options, "A"));
	CuAssertIntEquals(tc, -1, options_get_string(options, "A", &str));
	CuAssertTrue(tc, str == NULL);
	options_delete(options);

	options = options_test("--foo", "A", "foo", OPTION_NO_VALUE, 0, JSON_NULL, NULL, &ret);
	CuAssertTrue(tc, ret == NULL);
	CuAssertIntEquals(tc, 0, options_exists(options, "A"));
	CuAssertIntEquals(tc, -1, options_get_string(options, "A", &str));
	CuAssertTrue(tc, str == NULL);
	options_delete(options);

	/*
	 * This should fail in the new library
	 */
	options = options_test("-A 1", "A", "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, &ret);
	CuAssertTrue(tc, ret == NULL);
	CuAssertIntEquals(tc, -1, options_exists(options, "A"));
	CuAssertIntEquals(tc, -1, options_get_string(options, "A", &str));
	CuAssertTrue(tc, str == NULL);
	options_delete(options);

	options = options_test("-A", "A", "ab", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, &ret);
	CuAssertTrue(tc, ret == NULL);
	CuAssertIntEquals(tc, -1, options_exists(options, "A"));
	CuAssertIntEquals(tc, -1, options_get_string(options, "A", &str));
	CuAssertTrue(tc, str == NULL);
	options_delete(options);

	// r = options_test("-A 1", "A", "ab", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, &ret);
	// CuAssertIntEquals(tc, -1, r);

	// r = options_test("-A a", "A", "ab", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, &ret);
	// CuAssertIntEquals(tc, -1, r);

	options = options_test("--foo=-1", "A", "foo", OPTION_NO_VALUE, 0, JSON_NULL, NULL, &ret);
	CuAssertTrue(tc, ret == NULL);
	CuAssertIntEquals(tc, -1, options_exists(options, "A"));
	CuAssertIntEquals(tc, -1, options_get_string(options, "A", &str));
	CuAssertTrue(tc, str == NULL);
	options_delete(options);

	options_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_options_merge(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct options_t *options = NULL;
	struct options_t *options1 = NULL;
	char *argv[12] = {
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
	int argc = sizeof(argv)/sizeof(argv[0]);

	options_add(&options, "A", "ab", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "a", "ac", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "B", "ba", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "b", "bc", OPTION_OPT_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "C", "ca", OPTION_OPT_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "c", "cb", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, "D", "da", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&options1, "1", "foo", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, "2", "server", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, "3", "test", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options1, "4", "test1", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

	char *str = NULL;
	int i = 0, check = 0;

	options_parse(options, argc, argv);

	while(options_list(options, i++, &str) == 0) {
		if((strcmp(str, "A") == 0) || (strcmp(str, "a") == 0) ||
			(strcmp(str, "B") == 0) || (strcmp(str, "b") == 0) ||
			(strcmp(str, "C") == 0) || (strcmp(str, "c") == 0) ||
			(strcmp(str, "D") == 0) || (strcmp(str, "1") == 0) ||
			(strcmp(str, "2") == 0) || (strcmp(str, "3") == 0) ||
			(strcmp(str, "4") == 0)) {
			check++;
		}
	}
	CuAssertIntEquals(tc, 6, check);

	options_merge(&options, &options1);

	i = 0, check = 0;
	while(options_list(options, i++, &str) == 0) {
		if((strcmp(str, "A") == 0) || (strcmp(str, "a") == 0) ||
			(strcmp(str, "B") == 0) || (strcmp(str, "b") == 0) ||
			(strcmp(str, "C") == 0) || (strcmp(str, "c") == 0) ||
			(strcmp(str, "D") == 0) || (strcmp(str, "1") == 0) ||
			(strcmp(str, "2") == 0) || (strcmp(str, "3") == 0) ||
			(strcmp(str, "4") == 0)) {
			check++;
		}
	}
	CuAssertIntEquals(tc, 11, check);

	options_delete(options);
	options_delete(options1);

	options_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_options_mask(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct options_t *options = NULL;
	char *argv[2] = {
		"./pilight-unittest",
		"-A=1"
	};
	int argc = sizeof(argv)/sizeof(argv[0]), check = 0;

	options_add(&options, "A", "ab", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^[A-Z]$");

	options_parse(options, argc, argv);
	options_delete(options);

	options_gc();

	CuAssertIntEquals(tc, 0, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_options(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_options_absent);
	SUITE_ADD_TEST(suite, test_options_valid);
	SUITE_ADD_TEST(suite, test_options_invalid);
	SUITE_ADD_TEST(suite, test_options_merge);
#if !defined(__FreeBSD__) && !defined(_WIN32)
	SUITE_ADD_TEST(suite, test_options_mask);
#endif

	return suite;
}