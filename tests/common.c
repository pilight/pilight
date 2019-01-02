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

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/psutil/psutil.h"

static void test_explode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char **array = NULL;
	int n = explode("foo bar", " ", &array);
	CuAssertIntEquals(tc, 2, n);
	CuAssertPtrNotNull(tc, array);
	CuAssertStrEquals(tc, "foo", array[0]);
	CuAssertStrEquals(tc, "bar", array[1]);

	array_free(&array, n);
	CuAssertPtrEquals(tc, NULL, array);

	n = explode("hello,there,you", ",", &array);
	CuAssertIntEquals(tc, 3, n);
	CuAssertPtrNotNull(tc, array);
	CuAssertStrEquals(tc, "hello", array[0]);
	CuAssertStrEquals(tc, "there", array[1]);
	CuAssertStrEquals(tc, "you", array[2]);

	array_free(&array, n);
	CuAssertPtrEquals(tc, NULL, array);

	n = explode("foo", " ", &array);
	CuAssertIntEquals(tc, 1, n);
	CuAssertPtrNotNull(tc, array);
	CuAssertStrEquals(tc, "foo", array[0]);

	array_free(&array, n);
	CuAssertPtrEquals(tc, NULL, array);

	n = explode("sleep", "ee", &array);
	CuAssertIntEquals(tc, 2, n);
	CuAssertPtrNotNull(tc, array);
	CuAssertStrEquals(tc, "sl", array[0]);
	CuAssertStrEquals(tc, "p", array[1]);

	array_free(&array, n);
	CuAssertPtrEquals(tc, NULL, array);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_nrcpu(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	int nr = psutil_cpu_count_phys(), len = 0;
	char line[255];

	FILE *f = NULL;
	CuAssertPtrNotNull(tc, (f = fopen("rapport.txt", "a")));
	CuAssertTrue(tc, (len = sprintf(line, "Number of CPU cores: %d\n", nr)));
	CuAssertIntEquals(tc, len, fwrite(line, sizeof(char), len, f));
	fclose(f);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_isrunning(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int n = 0, *ret = NULL;
	n = isrunning("pilight-unittest", &ret);
	CuAssertTrue(tc, n > 0);
	CuAssertPtrNotNull(tc, ret);
	FREE(ret);
	ret = NULL;

	n = isrunning("foo", &ret);
	CuAssertIntEquals(tc, 0, n);
	CuAssertPtrEquals(tc, ret, NULL);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_isnummeric(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int nr = isNumeric("a");
	CuAssertIntEquals(tc, -1, nr);

	nr = isNumeric("1");
	CuAssertIntEquals(tc, 0, nr);

	nr = isNumeric("a1");
	CuAssertIntEquals(tc, -1, nr);

	nr = isNumeric("1.1");
	CuAssertIntEquals(tc, 0, nr);

	nr = isNumeric("-1.1");
	CuAssertIntEquals(tc, 0, nr);

	nr = isNumeric("-1");
	CuAssertIntEquals(tc, 0, nr);

	nr = isNumeric("-1.a");
	CuAssertIntEquals(tc, -1, nr);

	nr = isNumeric("a.a");
	CuAssertIntEquals(tc, -1, nr);

	nr = isNumeric("1e1");
	CuAssertIntEquals(tc, 0, nr);

	nr = isNumeric("1e1.1");
	CuAssertIntEquals(tc, -1, nr);

	nr = isNumeric("1234567890");
	CuAssertIntEquals(tc, 0, nr);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_name2uid(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

#ifndef _WIN32
	memtrack();

	int nr = name2uid("root");
	CuAssertIntEquals(tc, 0, nr);

	nr = name2uid("foo");
	CuAssertIntEquals(tc, -1, nr);

	CuAssertIntEquals(tc, 0, xfree());
#endif
}

static void test_strstr(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *str = "hellobellohellohello";
	const char *p = rstrstr(str, "hello");
	char **t = (char **)&p;
	int n = 0;

	CuAssertPtrNotNull(tc, p);

	n = p-str;

	CuAssertIntEquals(tc, 15, n);
	assert(strcmp(&str[n], "hello") == 0);

	p = rstrstr(str, "bello");
	CuAssertPtrNotNull(tc, p);
	n = p-str;

	CuAssertIntEquals(tc, 5, n);
	assert(strncmp(&str[n], "bello", 5) == 0);

	p = rstrstr(str, "o");
	CuAssertPtrNotNull(tc, p);
	n = p-str;

	CuAssertIntEquals(tc, 19, n);
	assert(strncmp(&str[n], "o", 1) == 0);

	p = rstrstr(str, "a");
	CuAssertPtrEquals(tc, NULL, *t);

	p = rstrstr(str, NULL);
	CuAssertPtrEquals(tc, NULL, *t);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_ishex(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int n = 0, i = 0;

	for(i=65;i<=91;i++) {
		n = ishex(i);
		if(i < 71) {
			CuAssertIntEquals(tc, 1, n);
		} else {
			CuAssertTrue(tc, (n != 1));
		}
	}
	for(i=97;i<=122;i++) {
		n = ishex(i);
		if(i < 103) {
			CuAssertIntEquals(tc, 1, n);
		} else {
			CuAssertTrue(tc, (n != 1));
		}
	}
	for(i=48;i<=57;i++) {
		n = ishex(i);
		CuAssertIntEquals(tc, 1, n);
	}

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_urldecode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char dest[255], *p = dest;
	int n = 0;

	n = urldecode("one%20%26%20two", p);
	CuAssertIntEquals(tc, 10, n);
	CuAssertIntEquals(tc, n-1, strlen(p));
	CuAssertStrEquals(tc, "one & two", p);

	n = urldecode("http%3a%2f%2fexample.com%2fquery%3fq%3drandom%20word%20%24500%20bank%20%24", p);
	CuAssertIntEquals(tc, 51, n);
	CuAssertIntEquals(tc, n-1, strlen(p));
	CuAssertStrEquals(tc, "http://example.com/query?q=random word $500 bank $", p);

	n = urldecode("http://example.com/", p);
	CuAssertIntEquals(tc, 20, n);
	CuAssertIntEquals(tc, n-1, strlen(p));
	CuAssertStrEquals(tc, "http://example.com/", p);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_urlencode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char dest[255], *p = dest;

	p = urlencode(NULL);
	CuAssertPtrEquals(tc, NULL, p);

	p = urlencode("HelloWorld");
	CuAssertPtrNotNull(tc, p);
	CuAssertIntEquals(tc, 10, strlen(p));
	CuAssertStrEquals(tc, "HelloWorld", p);
	FREE(p);

	p = urlencode("one & two");
	CuAssertPtrNotNull(tc, p);
	CuAssertIntEquals(tc, 15, strlen(p));
	CuAssertStrEquals(tc, "one%20%26%20two", p);
	FREE(p);

	p = urlencode("http://example.com/query?q=random word $500 bank $");
	CuAssertPtrNotNull(tc, p);
	CuAssertIntEquals(tc, 74, strlen(p));
	CuAssertStrEquals(tc, "http%3a%2f%2fexample.com%2fquery%3fq%3drandom%20word%20%24500%20bank%20%24", p);
	FREE(p);

	p = urlencode("http://example.com/");
	CuAssertPtrNotNull(tc, p);
	CuAssertIntEquals(tc, 27, strlen(p));
	CuAssertStrEquals(tc, "http%3a%2f%2fexample.com%2f", p);
	FREE(p);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_base64decode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *out = NULL;
	size_t len = 0;

	out = base64decode("QWxhZGRpbjpvcGVuIHNlc2FtZQ", 26, &len);
	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 19, len);
	CuAssertStrEquals(tc, "Aladdin:open sesame", out);
	FREE(out);

	out = base64decode("SGVsbG8sIFdvcmxkIQo=", 20, &len);
	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 14, len);
	CuAssertStrEquals(tc, "Hello, World!\n", out);
	FREE(out);

	out = base64decode("V2VsY29tZSB0byBvcGVuc3NsIHdpa2kK", 32, &len);

	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 24, len);
	CuAssertStrEquals(tc, "Welcome to openssl wiki\n", out);
	FREE(out);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_base64encode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *out = NULL;

	out = base64encode("Aladdin:open sesame", 19);
	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 28, strlen(out));
	CuAssertStrEquals(tc, "QWxhZGRpbjpvcGVuIHNlc2FtZQ==", out);
	FREE(out);

	out = base64encode("Hello, World!\n", 15);
	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 20, strlen(out));
	CuAssertStrEquals(tc, "SGVsbG8sIFdvcmxkIQoA", out);
	FREE(out);

	out = base64encode("Welcome to openssl wiki\n", 25);
	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 36, strlen(out));
	CuAssertStrEquals(tc, "V2VsY29tZSB0byBvcGVuc3NsIHdpa2kKAA==", out);
	FREE(out);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_hostname(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *out = hostname();
	char line[255];
	int len = 0;

	CuAssertPtrNotNull(tc, out);

	FILE *f = NULL;
	CuAssertPtrNotNull(tc, (f = fopen("rapport.txt", "a")));
	CuAssertTrue(tc, (len = sprintf(line, "Hostname: %s\n", out)));
	CuAssertIntEquals(tc, len, fwrite(line, sizeof(char), len, f));
	fclose(f);

	FREE(out);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_distroname(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *out = distroname(), line[255];
	int len = 0;

	CuAssertPtrNotNull(tc, out);

	FILE *f = NULL;
	CuAssertPtrNotNull(tc, (f = fopen("rapport.txt", "a")));
	CuAssertTrue(tc, (len = sprintf(line, "Operating System: %s\n", out)));
	CuAssertIntEquals(tc, len, fwrite(line, sizeof(char), len, f));
	fclose(f);

	FREE(out);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_file_exists(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int n = file_exists("gplv3.txt");
	CuAssertIntEquals(tc, 0, n);

	n = file_exists("foobar");
	CuAssertIntEquals(tc, -1, n);

	n = file_exists(NULL);
	CuAssertIntEquals(tc, -1, n);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_path_exists(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int n = path_exists("../tests");
	CuAssertIntEquals(tc, 0, n);

	n = path_exists("../foobar");
	CuAssertIntEquals(tc, -1, n);

	n = path_exists(NULL);
	CuAssertIntEquals(tc, -1, n);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_vercmp(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int n = vercmp("1", "0");
	CuAssertIntEquals(tc, 1, n);

	n = vercmp("0", "1");
	CuAssertIntEquals(tc, -1, n);

	n = vercmp("0", "0");
	CuAssertIntEquals(tc, 0, n);

	n = vercmp("1a", "1b");
	CuAssertIntEquals(tc, -1, n);

	n = vercmp("1a1", "1b1");
	CuAssertIntEquals(tc, -1, n);

	n = vercmp("1a2", "1a1");
	CuAssertIntEquals(tc, 1, n);

	n = vercmp("1.1-2.3a", "1.1-2.3b");
	CuAssertIntEquals(tc, -1, n);

	n = vercmp("1.1-2.3a", "1.1-2.3a");
	CuAssertIntEquals(tc, 0, n);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_uniq_space(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char str[255], *out = NULL;

	strcpy(str, "    ");
	out = uniq_space(str);
	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 1, strlen(out));
	CuAssertStrEquals(tc, " ", out);

	out = uniq_space(NULL);
	CuAssertPtrEquals(tc, NULL, out);

	strcpy(str, "a b  c   d");
	out = uniq_space(str);
	CuAssertPtrNotNull(tc, out);
	CuAssertIntEquals(tc, 7, strlen(out));
	CuAssertStrEquals(tc, "a b c d", out);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_str_replace(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *str = MALLOC(255), *p = str;
	int n = 0;
	CuAssertPtrNotNull(tc, str);

	strcpy(str, "HelloWorld");
	n = str_replace("o", "a", &p);
	CuAssertIntEquals(tc, 10, n);
	CuAssertStrEquals(tc, "HellaWarld", str);

	strcpy(str, "<body text='%body%'>");
	n = str_replace("%body%", "black", &p);
	CuAssertIntEquals(tc, 19, n);
	CuAssertStrEquals(tc, "<body text='black'>", str);

	strcpy(str, "Apple");
	n = str_replace("Apple", "Pineapple", &p);
	CuAssertIntEquals(tc, 9, n);
	CuAssertStrEquals(tc, "Pineapple", str);

	strcpy(str, "My fruit");
	n = str_replace("fruit", "raspberry", &p);
	CuAssertIntEquals(tc, 12, n);
	CuAssertStrEquals(tc, "My raspberry", str);

	FREE(str);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_stricmp(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int n = stricmp("A", "a");
	CuAssertIntEquals(tc, 0, n);

	n = stricmp("Hello World!", "hello world!");
	CuAssertIntEquals(tc, 0, n);

	n = stricmp("HelloWorld!", "Hello World!");
	CuAssertTrue(tc, (n != 0));

	n = stricmp("Hello World!", "hELLO wORLD!");
	CuAssertIntEquals(tc, 0, n);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_strnicmp(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int n = strnicmp("A", "a", 1);
	CuAssertIntEquals(tc, 0, n);
	
	n = strnicmp("A", "a", 0);
	CuAssertIntEquals(tc, 0, n);

	n = strnicmp("Hello World!", "hello world!", 12);
	CuAssertIntEquals(tc, 0, n);

	n = strnicmp("Hello World!", "hello", 5);
	CuAssertIntEquals(tc, 0, n);

	n = strnicmp("Hello World!", "hello", 12);
	CuAssertTrue(tc, (n != 0));

	n = strnicmp("HelloWorld!", "Hello W", 7);
	CuAssertTrue(tc, (n != 0));

	n = strnicmp("Hello World!", "hELLO wO", 8);
	CuAssertIntEquals(tc, 0, n);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_strtolower(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	{
		char *a = STRDUP("A");
		CuAssertPtrNotNull(tc, a);
		strtolower(&a);
		CuAssertStrEquals(tc, "a", a);
		FREE(a);
	}

	{
		char *a = STRDUP("Hello World!");
		CuAssertPtrNotNull(tc, a);
		strtolower(&a);
		CuAssertStrEquals(tc, "hello world!", a);
		FREE(a);
	}

	{
		char *a = STRDUP("HelloWorld!");
		CuAssertPtrNotNull(tc, a);
		strtolower(&a);
		CuAssertStrEquals(tc, "helloworld!", a);
		FREE(a);
	}

	{
		char *a = STRDUP("hELLO wORLd!");
		CuAssertPtrNotNull(tc, a);
		strtolower(&a);
		CuAssertStrEquals(tc, "hello world!", a);
		FREE(a);
	}

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_file_get_contents(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	FILE *f = fopen("test.txt", "w");
	CuAssertPtrNotNull(tc, f);
	fprintf(f, "HelloWorld!");
	fclose(f);

	char *tmp = NULL;
	int n = file_get_contents("gplv3.txt", &tmp);
	CuAssertIntEquals(tc, 0, n);
	CuAssertPtrNotNull(tc, tmp);
	FREE(tmp);

	n = file_get_contents("test.txt", &tmp);
	CuAssertIntEquals(tc, 0, n);
	CuAssertPtrNotNull(tc, tmp);
	assert(strcmp(tmp, "HelloWorld!") == 0);
	FREE(tmp);

	n = file_get_contents("foobar", &tmp);
	CuAssertIntEquals(tc, -1, n);
	assert(tmp == NULL);

	n = file_get_contents(NULL, &tmp);
	CuAssertIntEquals(tc, -1, n);
	assert(tmp == NULL);

	n = file_get_contents("test.txt", NULL);
	CuAssertIntEquals(tc, -1, n);
	assert(tmp == NULL);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_calc_time_interval(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	struct timeval tv;
	/*
	 * Milliseconds
	 */
	calc_time_interval(1, 8, 8, &tv);
	CuAssertIntEquals(tc, 0, tv.tv_sec);
	CuAssertIntEquals(tc, 1, tv.tv_usec);

	calc_time_interval(1, 100, 8, &tv);
	CuAssertIntEquals(tc, 0, tv.tv_sec);
	CuAssertIntEquals(tc, 12, tv.tv_usec);

	calc_time_interval(1, 1000, 20, &tv);
	CuAssertIntEquals(tc, 0, tv.tv_sec);
	CuAssertIntEquals(tc, 50, tv.tv_usec);

	/*
	 * Seconds
	 */
	calc_time_interval(2, 1, 8, &tv);
	CuAssertIntEquals(tc, 0, tv.tv_sec);
	CuAssertIntEquals(tc, 125, tv.tv_usec);

	calc_time_interval(2, 100, 8, &tv);
	CuAssertIntEquals(tc, 12, tv.tv_sec);
	CuAssertIntEquals(tc, 500, tv.tv_usec);

	calc_time_interval(2, 1000, 20, &tv);
	CuAssertIntEquals(tc, 50, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	/*
	 * Minutes
	 */
	calc_time_interval(3, 1, 8, &tv);
	CuAssertIntEquals(tc, 7, tv.tv_sec);
	CuAssertIntEquals(tc, 500, tv.tv_usec);

	calc_time_interval(3, 100, 8, &tv);
	CuAssertIntEquals(tc, 750, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	calc_time_interval(3, 15, 20, &tv);
	CuAssertIntEquals(tc, 45, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	/*
	 * Hours
	 */
	calc_time_interval(4, 1, 5, &tv);
	CuAssertIntEquals(tc, 720, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	calc_time_interval(4, 2, 8, &tv);
	CuAssertIntEquals(tc, 900, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	calc_time_interval(4, 15, 20, &tv);
	CuAssertIntEquals(tc, 2700, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	/*
	 * Days
	 */
	calc_time_interval(5, 1, 5, &tv);
	CuAssertIntEquals(tc, 17280, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	calc_time_interval(5, 2, 8, &tv);
	CuAssertIntEquals(tc, 21600, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	calc_time_interval(5, 15, 20, &tv);
	CuAssertIntEquals(tc, 64800, tv.tv_sec);
	CuAssertIntEquals(tc, 0, tv.tv_usec);

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_common(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_explode);
	SUITE_ADD_TEST(suite, test_nrcpu);
	SUITE_ADD_TEST(suite, test_isrunning);
	SUITE_ADD_TEST(suite, test_isnummeric);
	SUITE_ADD_TEST(suite, test_name2uid);
	SUITE_ADD_TEST(suite, test_strstr);
	SUITE_ADD_TEST(suite, test_ishex);
	SUITE_ADD_TEST(suite, test_urldecode);
	SUITE_ADD_TEST(suite, test_urlencode);
	SUITE_ADD_TEST(suite, test_base64decode);
	SUITE_ADD_TEST(suite, test_base64encode);
	SUITE_ADD_TEST(suite, test_hostname);
	SUITE_ADD_TEST(suite, test_distroname);
	SUITE_ADD_TEST(suite, test_file_exists);
	SUITE_ADD_TEST(suite, test_path_exists);
	SUITE_ADD_TEST(suite, test_vercmp);
	SUITE_ADD_TEST(suite, test_uniq_space);
	SUITE_ADD_TEST(suite, test_str_replace);
	SUITE_ADD_TEST(suite, test_stricmp);
	SUITE_ADD_TEST(suite, test_strnicmp);
	SUITE_ADD_TEST(suite, test_strtolower);
	SUITE_ADD_TEST(suite, test_file_get_contents);
	SUITE_ADD_TEST(suite, test_calc_time_interval);

	return suite;
}
