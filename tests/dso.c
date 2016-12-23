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
#include "../libs/pilight/core/dso.h"

#include "alltests.h"

static void test_dso(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	/*
	 * False positives from valgrind:
	 *
	 * at 0x4C2FB55: calloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
	 * by 0x597A626: _dlerror_run (dlerror.c:141)
	 * by 0x5979FA0: dlopen@@GLIBC_2.2.5 (dlopen.c:87)
	 */
	
	void *handle = NULL;
	int (*multiply)(int, int) = NULL;
#ifdef _WIN32
	handle = dso_load("./libdso.dll");
#else
	handle = dso_load("./libdso.so");
#endif
	CuAssertPtrNotNull(tc, handle);

	multiply = dso_function(handle, "multiply");
	CuAssertPtrNotNull(tc, multiply);
	CuAssertIntEquals(tc, 6, multiply(2, 3));

	dso_gc();	

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_dso(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_dso);

	return suite;
}