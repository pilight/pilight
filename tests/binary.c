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

#include "alltests.h"

static void test_binary(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	int binary[255] = {0, 1, 0, 1, 0, 1, 0, 1};
	int out[255] = {0};
	unsigned int i = 0;

	CuAssertIntEquals(tc, 0, binToDec(binary, 0, 0));
	CuAssertIntEquals(tc, 2, binToDec(binary, 0, 1));
	CuAssertIntEquals(tc, 2, binToDec(binary, 0, 2));
	CuAssertIntEquals(tc, 10, binToDec(binary, 0, 3));
	CuAssertIntEquals(tc, 10, binToDec(binary, 0, 4));
	CuAssertIntEquals(tc, 42, binToDec(binary, 0, 5));
	CuAssertIntEquals(tc, 42, binToDec(binary, 0, 6));
	CuAssertIntEquals(tc, 170, binToDec(binary, 0, 7));
	CuAssertIntEquals(tc, 1, binToDec(binary, 1, 1));
	CuAssertIntEquals(tc, 1, binToDec(binary, 1, 2));
	CuAssertIntEquals(tc, 5, binToDec(binary, 1, 3));
	CuAssertIntEquals(tc, 5, binToDec(binary, 1, 4));
	CuAssertIntEquals(tc, 21, binToDec(binary, 1, 5));
	CuAssertIntEquals(tc, 21, binToDec(binary, 1, 6));
	CuAssertIntEquals(tc, 85, binToDec(binary, 1, 7));
	CuAssertIntEquals(tc, 1, binToDec(binary+1, 0, 0));
	CuAssertIntEquals(tc, 1, binToDec(binary+1, 0, 1));
	CuAssertIntEquals(tc, 5, binToDec(binary+1, 0, 2));
	CuAssertIntEquals(tc, 5, binToDec(binary+1, 0, 3));
	CuAssertIntEquals(tc, 21, binToDec(binary+1, 0, 4));
	CuAssertIntEquals(tc, 21, binToDec(binary+1, 0, 5));
	CuAssertIntEquals(tc, 85, binToDec(binary+1, 0, 6));

	CuAssertIntEquals(tc, 0, binToDecRev(binary, 0, 0));
	CuAssertIntEquals(tc, 1, binToDecRev(binary, 0, 1));
	CuAssertIntEquals(tc, 2, binToDecRev(binary, 0, 2));
	CuAssertIntEquals(tc, 5, binToDecRev(binary, 0, 3));
	CuAssertIntEquals(tc, 10, binToDecRev(binary, 0, 4));
	CuAssertIntEquals(tc, 21, binToDecRev(binary, 0, 5));
	CuAssertIntEquals(tc, 42, binToDecRev(binary, 0, 6));
	CuAssertIntEquals(tc, 85, binToDecRev(binary, 0, 7));
	CuAssertIntEquals(tc, 1, binToDecRev(binary, 1, 1));
	CuAssertIntEquals(tc, 2, binToDecRev(binary, 1, 2));
	CuAssertIntEquals(tc, 5, binToDecRev(binary, 1, 3));
	CuAssertIntEquals(tc, 10, binToDecRev(binary, 1, 4));
	CuAssertIntEquals(tc, 21, binToDecRev(binary, 1, 5));
	CuAssertIntEquals(tc, 42, binToDecRev(binary, 1, 6));
	CuAssertIntEquals(tc, 85, binToDecRev(binary, 1, 7));
	CuAssertIntEquals(tc, 1, binToDecRev(binary+1, 0, 0));
	CuAssertIntEquals(tc, 2, binToDecRev(binary+1, 0, 1));
	CuAssertIntEquals(tc, 5, binToDecRev(binary+1, 0, 2));
	CuAssertIntEquals(tc, 10, binToDecRev(binary+1, 0, 3));
	CuAssertIntEquals(tc, 21, binToDecRev(binary+1, 0, 4));
	CuAssertIntEquals(tc, 42, binToDecRev(binary+1, 0, 5));
	CuAssertIntEquals(tc, 85, binToDecRev(binary+1, 0, 6));

	CuAssertIntEquals(tc, 4, decToBin(31, out));
	for(i=0;i<5;i++) {
		CuAssertIntEquals(tc, 1, out[i]);
	}
	CuAssertIntEquals(tc, 0, out[++i]);

	CuAssertIntEquals(tc, 4, decToBinRev(31, out));
	for(i=0;i<5;i++) {
		CuAssertIntEquals(tc, 1, out[i]);
	}
	CuAssertIntEquals(tc, 0, out[++i]);	

	for(i=0;i<64;i++) {
		binary[i] = i % 2;
	}

	CuAssertULongEquals(tc, 12297829382473034410U, binToDecUl(binary, 0, 63));
	CuAssertULongEquals(tc, 6148914691236517205, binToDecRevUl(binary, 0, 63));
	
	for(i=0;i<64;i++) {
		binary[i] = i % 2 == 0;
	}
	CuAssertULongEquals(tc, 6148914691236517205, binToDecUl(binary, 0, 63));
	CuAssertULongEquals(tc, 12297829382473034410U, binToDecRevUl(binary, 0, 63));

	CuAssertULongEquals(tc, 63, decToBinUl(12297829382473034410U, binary));

	for(i=0;i<64;i++) {
		binary[i] = (i % 2) == 0;
	}

	CuAssertULongEquals(tc, 63, decToBinRevUl(12297829382473034410U, binary));
	for(i=0;i<64;i++) {
		CuAssertIntEquals(tc, (i % 2), binary[i]);
	}

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_binary(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_binary);

	return suite;
}
