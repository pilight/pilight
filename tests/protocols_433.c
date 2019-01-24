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

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/433.92/alecto_ws1700.h"
#include "../libs/pilight/protocols/433.92/alecto_wsd17.h"
#include "../libs/pilight/protocols/433.92/alecto_wx500.h"
#include "../libs/pilight/protocols/433.92/arctech_contact.h"
#include "../libs/pilight/protocols/433.92/arctech_dimmer.h"
#include "../libs/pilight/protocols/433.92/arctech_dusk.h"
#include "../libs/pilight/protocols/433.92/livolo_switch.h"
#include "../libs/pilight/protocols/433.92/kerui_d026.h"
#include "../libs/pilight/protocols/433.92/tfa2017.h"

#include "alltests.h"

const struct raw_t {
	char *input;
	char *message;
	char *output;
	int validate;
} raw_t;

const struct test_t {
	void (*init)(void);
	struct protocol_t **protocol;
	const struct raw_t *raw;
	const void *nrraw;
} test_t;

#include "protocols/alecto_ws1700.h"
#include "protocols/alecto_wx500.h"
#include "protocols/arctech_contact.h"
#include "protocols/arctech_dimmer.h"
#include "protocols/arctech_dusk.h"
#include "protocols/livolo_switch.h"
#include "protocols/kerui_d026.h"
#include "protocols/tfa2017.h"

static const struct test_t tests[] = {
	{ &alectoWS1700Init, &alecto_ws1700, alecto_ws1700_tests, &alecto_ws1700_nrtests },
	{ &alectoWX500Init, &alecto_wx500, alecto_wx500_tests, &alecto_wx500_nrtests },
	{ &arctechContactInit, &arctech_contact, arctech_contact_tests, &arctech_contact_nrtests },
	{ &arctechDimmerInit, &arctech_dimmer, arctech_dimmer_tests, &arctech_dimmer_nrtests },
	{ &arctechDuskInit, &arctech_dusk, arctech_dusk_tests, &arctech_dusk_nrtests },
	{ &livoloSwitchInit, &livolo_switch, livolo_switch_tests, &livolo_switch_nrtests },
	{ &keruiD026Init, &kerui_D026, kerui_d026_tests, &kerui_d026_nrtests },
	{ &tfa2017Init, &tfa2017, tfa2017_tests, &tfa2017_nrtests },
};

static void test_protocols_433(CuTest *tc) {
	memtrack();	

	char **array = NULL, message[255], name[255];
	int raw[255], nrtests = 0, nrraw = 0;
	int x = 0, y = 0, i = 0;

	nrtests = sizeof(tests)/sizeof(tests[0]);
	
	for(x=0;x<nrtests;x++) {
		memset(&message, '\0', 255);

		nrraw = *(int *)(tests[x].nrraw);

		tests[x].init();
		struct protocol_t *protocol = *tests[x].protocol;

		sprintf(name, "%s_%s", __FUNCTION__, protocol->id);
		printf("[ %-48s ]\n", name);
		fflush(stdout);

		protocol->raw = raw;

		for(i=0;i<nrraw;i++) {
			/*
			 * Validate raw input decoding
			 */
			protocol->rawlen = explode(tests[x].raw[i].input, " ", &array);
			for(y=0;y<protocol->rawlen;y++) {
				protocol->raw[y] = atoi(array[y]);
			}
			array_free(&array, protocol->rawlen);

			/*
			 * Check if the validator worked
			 */
			if(protocol->validate != NULL) {
				CuAssertIntEquals(tc, tests[x].raw[i].validate, protocol->validate());
			}

			if(tests[x].raw[i].validate == 0) {
				char *p = message;
				protocol->parseCode(&p);

				CuAssertStrEquals(tc, tests[x].raw[i].message, message);
			}

			memset(&message, '\0', 255);

			/*
			 * Check if input paramters are parsed to proper pulses
			 */
			if(protocol->createCode != NULL && strlen(tests[x].raw[i].output) > 0) {
				struct JsonNode *json = json_decode(tests[x].raw[i].message);
				char *p = message;
				protocol->createCode(json, &p);
				json_delete(json);

				int n = explode(tests[x].raw[i].output, " ", &array);
				for(y=0;y<protocol->rawlen;y++) {
					CuAssertIntEquals(tc, protocol->raw[y], atoi(array[y]));
				}
				array_free(&array, n);
			}
		}
		protocol_gc();
	}

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_433(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_433);

	return suite;
}
