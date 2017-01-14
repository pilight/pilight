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
#include <math.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/json.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/core/raw.h"

#include "alltests.h"

static char *code = "286 2825 286 201 289 1337 287 209 283 1351 287 204 289 1339 288 207 288 1341 289 207 281 1343 284 205 292 1346 282 212 283 1348 282 213 279 1352 282 211 281 1349 282 210 283 1347 284 211 288 1348 281 211 285 1353 278 213 280 1351 280 232 282 1356 279 213 285 1351 276 215 285 1348 277 216 278 1359 278 216 279 1353 272 214 283 1358 276 216 276 1351 278 214 284 1357 275 217 276 1353 270 217 277 1353 272 220 277 1351 275 220 272 1356 275 1353 273 224 277 236 282 1355 272 1353 273 233 273 222 268 1358 270 219 277 1361 274 218 280 1358 272 1355 271 243 273 222 268 1358 270 219 277 1361 274 218 280 1358 272 1355 271 243 251 10302";

static void test_protocols_core_raw(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	char message[1024];
	int pulses[256];

	memtrack();

	rawInit();
	raw->raw = pulses;

	struct JsonNode *jcode = json_mkobject();
	json_append_member(jcode, "code", json_mkstring(code));
	json_append_member(jcode, "repeats", json_mknumber(1, 0));

	raw->createCode(jcode, message);

	CuAssertIntEquals(tc, 148, raw->rawlen);
	CuAssertIntEquals(tc, 1, raw->txrpt);

	char **array = NULL;
	int i = 0, n = 0;
	n = explode(code, " ", &array);
	CuAssertIntEquals(tc, 148, n);

	for(i=0;i<raw->rawlen;i++) {
		CuAssertIntEquals(tc, raw->raw[i], atoi(array[i]));
	}
	json_delete(jcode);
	array_free(&array, n);

	protocol_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_core(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_core_raw);
	return suite;
}