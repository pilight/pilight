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
#ifndef _WIN32
#include <unistd.h>
#endif

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/binary.h"
#include "../libs/pilight/lua/lua.h"
#include "../libs/pilight/events/events.h"
#include "../libs/pilight/events/operator.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_dimmer.h"
#include "../libs/pilight/protocols/generic/generic_label.h"
#include "../libs/pilight/protocols/433.92/arctech_switch.h"
#include "../libs/pilight/protocols/API/datetime.h"
#include "../libs/pilight/protocols/API/sunriseset.h"

#include "alltests.h"

static int testnr = 0;
static CuTest *gtc = NULL;

static struct reason_code_received_t updates2[] = {
	{
		"{\"id\":1,\"unit\":1,\"state\":\"on\"}",
		"receiver",
		"arctech_switch",
		"1234",
		0
	},
	{
		"{\"longitude\":1,\"latitude\":1,\"year\":2017,\"month\":1,\"day\":1,\"hour\":1,\"minute\":1,\"second\":1,\"weekday\":1}",
		"receiver",
		"datetime",
		"1234",
		0
	}
};

static struct reason_config_update_t updates1[] = {
	{	/* 0 */
		"update", 						// origin
		SWITCH,								// type
		1,										// timestamp
		1,										// nrdev
		{ "switch" },					// devices
		1,										// nrval
		{											// values
			{
				"state",
				{
						.string_ = "off"
				},
				0,
				JSON_STRING
			}
		},
		NULL 								// uuid
	},
	{	/* 1 */
		"update", 						// origin
		DIMMER,								// type
		1,										// timestamp
		1,										// nrdev
		{ "dimmer" },					// devices
		1,										// nrval
		{											// values
			{
				"state",
				{
						.string_ = "off"
				},
				0,
				JSON_STRING
			}
		},
		NULL 								// uuid
	},
	{	/* 2 */
		"update", 						// origin
		DATETIME,							// type
		1,										// timestamp
		1,										// nrdev
		{ "date" },						// devices
		1,										// nrval
		{											// values
			{ "year", { .number_ = 2016 }, 0, JSON_NUMBER },
			{ "month", { .number_ = 2 }, 0, JSON_NUMBER },
			{ "day", { .number_ = 1 }, 0, JSON_NUMBER },
			{ "hour", { .number_ = 12 }, 0, JSON_NUMBER },
			{ "minute", { .number_ = 31 }, 0, JSON_NUMBER },
			{ "second", { .number_ = 15 }, 0, JSON_NUMBER }
		},
		NULL 								// uuid
	}
};

static char *values[] = {
	NULL,
	"{\"dimlevel\":2}",
	NULL,
	"{\"label\":\"21:01 21:00\"}",
	"{\"label\":\"1010\"}",
	NULL
};

static struct reason_control_device_t receives[] = {
	{ "switch", "on", NULL },
	{ "dimmer", "on", NULL },
	{ "switch1", "on", NULL },
	{ "label", NULL, NULL }, // Initialize struct JsonNode;
	{ "testlabel", NULL, NULL },
	{ "dimmer", "on", NULL }
};

struct tests_t {
	char *title;
	char *config;
	int valid;
	int runmode;
	int type;
	void *updates;
	struct reason_control_device_t *receives[10];
	int nrreceives[2];
} tests_t;

static struct tests_t get_tests[] = {
	{
		"invalid rule 1",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 2",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF THEN\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 3",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 THEN\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 4",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 5",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 a 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 6",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF (1 == 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 7",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1) THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 8",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 THEN foo DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 9",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR a == a AND 1 == a THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 10",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR a == a AND 1 == a THEN 'switch' DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"invalid rule 11",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO on == dim DEVICE dimmer TO 2\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 12",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO dim DEVICE dimmer TO 2\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 13",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 14",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch FOO\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 15",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch TO\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 16",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 17",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO on FOO dim DEVICE dimmer TO 2\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 18",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF RANDOM(1, 2) RANDOM(3, 4) THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 19",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF RANDOM(1 2) > 0 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 20",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF RANDOM(1, 2 > 0 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"invalid rule 21",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":20,\"minute\":56,\"second\":48,\"weekday\":1,\"dst\":0}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF DATE_FORMAT(DATE_ADD(time, +1 HOUR), '%%Y-%%m-%%d %%H:%%M:%%S', '%%H.%%M') == 15.00 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		/*
		 * The operators and functions are not loaded
		 */
		"invalid rule 22",
		"{\"devices\":{" \
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"on\"},"\
			"\"date\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"year\":2018,\"month\":1,\"day\":24,\"hour\":20,\"minute\":10,\"second\":12,\"weekday\":4,\"dst\":0},"\
			"\"sun\": {\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"sunrise\":8.34,\"sunset\":17.14,\"sun\":\"set\"}"\
		"}," \
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF (DATE_FORMAT(DATE_ADD(date, '-10 MINUTE'), '%%Y-%%m-%%d %%H:%%M:%%S', %%H.%%M) == DATE_FORMAT(DATE_ADD(date, '-10 MINUTE'), '%%Y-%%m-%%d %%H:%%M:%%S', %%H.%%M) AND date.second == 0) AND (0 AND 1) THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		/*
		 * The operators and functions are not loaded
		 */
		"invalid rule 23",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"on\"}}," \
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF (1 == 1) THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"invalid rule 24",
		"{\"devices\":{\"label1\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"}}," \
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN label DEVICE label1 TO 'error:' foo COLOR red BLINK on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"invalid rule 25",
		"{\"devices\":{"\
			"\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN label DEVICE testlabel TO 'a\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"invalid rule 26",
		"{\"devices\":{"\
			"\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF 1 == 0 THEN label DEVICE testlabel\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		-1, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"simple true formula",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"simple false formula",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 0 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"with hooks",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF ((1 == 1)) THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"with and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 AND a == a THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"single quoted with and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == '1' THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"double quoted with and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == \\\"1\\\" THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"with or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 OR a == b THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"with or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR a == a THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"multiple and or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR a == a AND 1 == a THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"multiple and or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 OR a == b AND 1 == 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"multiple and or hooks",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR 1 == 1 AND 2 == 1 AND 1 == 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"multiple and or hooks",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 0 OR (2 == 0 AND (2 == 2 OR 1 == 1)) THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"gt and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 0 > 10 AND 10 > 2 THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"with dot",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF '.' == on THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"decimals",
		"{\"devices\":{"\
			"\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF 9.4 == 9.4 THEN label DEVICE testlabel TO 1000 + 10\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[4] },
		{ 1, 0 }
	},
	{
		"with multiple dots",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 'http://192.168.1.1/' == 'http://192.168.1.1/' THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"float from functions",
		"{\"devices\":{"\
			"\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"},"\
			"\"date\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"year\":2018,\"month\":1,\"day\":24,\"hour\":12,\"minute\":30,\"second\":12,\"weekday\":4,\"dst\":0}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF DATE_FORMAT(date, %H.%M) == 12.30 THEN label DEVICE testlabel TO 1000 + 10\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[2],
		{ &receives[4] },
		{ 1, 0 }
	},
	{
		"valid device param",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF switch.state == off THEN switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"valid device param",
		"{\"devices\":{\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF dimmer.dimlevel == 10 THEN switch DEVICE dimmer TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[5] },
		{ 1, 0 }
	},
	{
		/*
		 * This rule is intentionally written without hooks.
		 */
		"valid device param3",
		"{\"devices\":{\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF datetime.hour < datetime.hour + 10 THEN switch DEVICE dimmer TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		1, &updates2[1],
		{ &receives[5] },
		{ 1, 0 }
	},
	{
		"valid device param",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"switch1\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"on\"}"\
		"}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF switch.state != switch1.state THEN switch DEVICE 'switch' TO switch1.state\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"dim action",
		"{\"devices\":{\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO 1\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ NULL }, { 0 }
	},
	{
		"function as device param",
		"{\"devices\":{\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO RANDOM(1, 10)\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ NULL }, { 0 }
	},
	{
		"valid device param in function",
		"{\"devices\":{"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10},"\
			"\"dimmer1\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":2},"\
			"\"dimmer2\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":5}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF dimmer.dimlevel == dimmer.dimlevel THEN dim DEVICE dimmer TO RANDOM(dimmer1.dimlevel, dimmer2.dimlevel)\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ NULL }, { 0 }
	},
	{
		"dimmer values from function",
		"{\"devices\":{"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO DATE_FORMAT(\\\"2016,01,02\\\", '%%Y,%%m,%%d', '%%d')\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1] },
		{ 1, 0 }
	},
	{
		"multiple actions",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO on AND dim DEVICE dimmer TO 2\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"multiple rules",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO 2\",\"active\":1},"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"multiple rules one inactive",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO 2\",\"active\":0},"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"nested functions",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF RANDOM(RANDOM(1, 2), RANDOM(3, 4)) * RANDOM(5, 6) - (RANDOM(7, 8) / RANDOM(9, 10)) > 0 THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"received device",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF arctech_switch.id == 1 AND arctech_switch.unit == 1 THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		1, &updates2[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"date format with device parameter",
		"{\"devices\":{"\
			"\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":20,\"minute\":56,\"second\":48,\"weekday\":1,\"dst\":0},"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF switch.state == off AND DATE_FORMAT(test, %%Y%%m%%d%%H%%M%%S) == 20160314205648 THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"date add with device parameter",
		"{\"devices\":{"\
			"\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":20,\"minute\":56,\"second\":48,\"weekday\":1,\"dst\":0},"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF switch.state == off AND DATE_FORMAT(DATE_ADD(test, '+12 HOUR'), '%%Y-%%m-%%d %%H:%%M:%%S', '%%Y%%m%%d%%H%%M%%S') == 20160315085648 THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"only execute device rules",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":1}],\"state\":\"off\"},"\
			"\"switch1\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":2}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF switch1.state == off THEN toggle DEVICE 'switch' BETWEEN on AND off\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"nested function in action",
		"{\"devices\":{"\
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN label DEVICE 'label' TO DATE_FORMAT(DATE_ADD('2015-01-01 21:00:00', '+1 MINUTE'), '%%Y-%%m-%%d %%H:%%M:%%S', '%%H:%%M') . ' ' . DATE_FORMAT(DATE_ADD('2015-01-01 21:00:00', '+1 DAY'), '%%Y-%%m-%%d %%H:%%M:%%S', '%%H:%%M')\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[3] },
		{ 1, 0 }
	},
	{
		"concat function and string in action",
		"{\"devices\":{"\
			"\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"},"\
			"\"datetime1\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":20,\"minute\":56,\"second\":48,\"weekday\":1,\"dst\":0}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN label DEVICE testlabel TO 'the alarm was trigger at' . DATE_FORMAT(datetime1, %%Y-%%m-%%d)\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		/*
		 * This rule was intentionally written like this
		 */
		"prioritize evaluations",
		"{\"devices\":{"\
			"\"date\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":8,\"minute\":44,\"second\":48,\"weekday\":1,\"dst\":0},"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"sun\":{\"protocol\":[\"sunriseset\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"sunrise\":8.34,\"sunset\":17.14,\"sun\":\"set\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF (sun.sunrise == DATE_FORMAT(DATE_ADD(date, '-10 MINUTE'), '%%Y-%%m-%%d %%H:%%M:%%S', '%%H.%%M') AND date.second == 1) OR (1 == 0) THEN switch DEVICE 'switch' TO on\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"simple if else",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE 'switch' TO on ELSE switch DEVICE 'switch' TO off\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"simple if else",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 0 THEN switch DEVICE 'switch' TO off ELSE switch DEVICE 'switch' TO on END\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"simple if else if",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 0 THEN IF 1 == 1 THEN switch DEVICE 'switch' TO off END ELSE switch DEVICE 'switch' TO on END\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"simple if if else else",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 0 THEN IF 1 == 1 THEN switch DEVICE 'switch' TO off ELSE switch DEVICE 'switch' TO on END ELSE switch DEVICE 'switch' TO on\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	/*
	 * This rule intentionally has a DATE_FORMAT function
	 * in various places. In the IF, the ELSE and inside
	 * an action. This rule (and the next) can be used to
	 * test if the date is the only device being cached,
	 * and not the date1. So an `date` device should
	 * trigger this rule, a `date1` should not.
	 */
	{
		"stacked if else with device parameters 1",
		"{\"devices\":{"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}," \
			"\"date\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":8,\"minute\":44,\"second\":48,\"weekday\":1,\"dst\":0},	"\
			"\"date1\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":8,\"minute\":44,\"second\":48,\"weekday\":1,\"dst\":0}"\
		"},\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 0 AND DATE_FORMAT(date, %H.%M) == 12.30 THEN IF 1 == 1 AND DATE_FORMAT(date, %H.%M) == 12.30 THEN dim DEVICE 'dimmer' TO DATE_FORMAT(date1, %H) - 4 ELSE dim DEVICE 'dimmer' TO DATE_FORMAT(date1, %H) - 5 END ELSE dim DEVICE 'dimmer' TO DATE_FORMAT(date1, %H) - 6\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[2],
		{ &receives[1] },
		{ 1, 0 }
	},
	{
		"stacked if else with device parameters 2",
		"{\"devices\":{"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}," \
			"\"date\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":8,\"minute\":44,\"second\":48,\"weekday\":1,\"dst\":0},	"\
			"\"date1\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":8,\"minute\":44,\"second\":48,\"weekday\":1,\"dst\":0}"\
		"},\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 0 AND DATE_FORMAT(date1, %H.%M) == 12.30 THEN IF 1 == 1 AND DATE_FORMAT(date1, %H.%M) == 12.30 THEN dim DEVICE 'dimmer' TO DATE_FORMAT(date, %H) - 4 ELSE dim DEVICE 'dimmer' TO DATE_FORMAT(date, %H) - 5 END ELSE dim DEVICE 'dimmer' TO DATE_FORMAT(date, %H) - 6\",\"active\":1}},"\
		"\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_NOWAIT,
		0, &updates1[2],
		{ NULL }, { 0 }
	},
	{
		"calculation inside label",
		"{\"devices\":{"\
			"\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"foo\",\"color\":\"black\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN label DEVICE testlabel TO 1000 + 10\",\"active\":1}"\
		"},\"settings\":%s,\"hardware\":{},\"registry\":{}}",
		0, UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[4] },
		{ 1, 0 }
	},
};

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void *control_device(int reason, void *param) {
	int a = get_tests[testnr].nrreceives[1], i = 0;
	int len = sizeof(receives)/sizeof(receives[0]);
	struct reason_control_device_t *data = param;

	CuAssertIntEquals(gtc, get_tests[testnr].runmode, UV_RUN_DEFAULT);

	if(get_tests[testnr].receives[a] == NULL) {
		uv_stop(uv_default_loop());
		return NULL;
	}	

	for(i=0;i<len;i++) {
		if(get_tests[testnr].receives[a] == &receives[i] && values[i] != NULL) {
			struct reason_control_device_t *p = &receives[i];
			p->values = json_decode(values[i]);
		}
	}

	CuAssertStrEquals(gtc, get_tests[testnr].receives[a]->dev, data->dev);
	CuAssertStrEquals(gtc, get_tests[testnr].receives[a]->state, data->state);
	if(data->values != NULL && get_tests[testnr].receives[a]->values != NULL) {
		char *out = json_stringify(data->values, NULL);
		char *out1 = json_stringify(get_tests[testnr].receives[a]->values, NULL);

		CuAssertStrEquals(gtc, out1, out);
		json_free(out);
		json_free(out1);
	}

	get_tests[testnr].nrreceives[1] += 1;
	if(get_tests[testnr].nrreceives[1] == get_tests[testnr].nrreceives[0]) {
		uv_stop(uv_default_loop());
	}
	return NULL;
}

static void test_events(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	int len = sizeof(get_tests)/sizeof(get_tests[0]), i = 0;
	for(testnr=0;testnr<len;testnr++) {
		printf("[ - %-46s ]\n", get_tests[testnr].title);
		fflush(stdout);

		memtrack();

		char settings[1024] = "{"\
			"\"operators-root\":\"%s../libs/pilight/events/operators/\","\
			"\"actions-root\":\"%s../libs/pilight/events/actions/\","\
			"\"functions-root\":\"%s../libs/pilight/events/functions/\""\
		"}";
		char settings1[1024], *p = settings1;
		char *file = STRDUP(__FILE__);
		CuAssertPtrNotNull(tc, file);
		memset(&settings1, '\0', 1024);
		str_replace("events.c", "", &file);
		snprintf(p, 1024, settings, file, file, file);
		FREE(file);

		FILE *f = fopen("events.json", "w");
		fprintf(f,
			get_tests[testnr].config,
			settings1
		);
		fclose(f);

		plua_init();

		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("events.json", CONFIG_SETTINGS));
		event_action_init();
		event_operator_init();
		event_function_init();
		storage_gc();

		storage_init();

		arctechSwitchInit();
		genericSwitchInit();
		genericDimmerInit();
		genericLabelInit();
		datetimeInit();
		sunRiseSetInit();

		CuAssertIntEquals(tc, get_tests[testnr].valid, storage_read("events.json", CONFIG_DEVICES | CONFIG_RULES));

		if(get_tests[testnr].valid == 0) {
			eventpool_init(EVENTPOOL_NO_THREADS);
			eventpool_callback(REASON_CONTROL_DEVICE, control_device);
			event_init();

			if(get_tests[testnr].type == 0) {
				eventpool_trigger(REASON_CONFIG_UPDATED, NULL, get_tests[testnr].updates);
			} else {
				eventpool_trigger(REASON_CODE_RECEIVED, NULL, get_tests[testnr].updates);
			}

			if(get_tests[testnr].runmode == UV_RUN_NOWAIT) {
				for(i=0;i<10;i++) {
					uv_run(uv_default_loop(), get_tests[testnr].runmode);
					usleep(1000);
				}
			} else {
				uv_run(uv_default_loop(), get_tests[testnr].runmode);
			}
			uv_walk(uv_default_loop(), walk_cb, NULL);
			uv_run(uv_default_loop(), UV_RUN_ONCE);

			while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
				uv_run(uv_default_loop(), UV_RUN_ONCE);
			}
		}
		event_operator_gc();
		event_action_gc();
		event_function_gc();
		protocol_gc();
		storage_gc();
		eventpool_gc();
		plua_gc();

		int len = sizeof(receives)/sizeof(receives[0]), i = 0, x = 0;

		for(x=0;x<get_tests[testnr].nrreceives[0];x++) {
			for(i=0;i<len;i++) {
				if(get_tests[testnr].receives[x] == &receives[i]) {
					struct reason_control_device_t *p = &receives[i];
					if(p->values != NULL) {
						json_delete(p->values);
					}
				}
			}
		}
		
		CuAssertIntEquals(tc, 0, xfree());
	}
}

CuSuite *suite_events(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_events);

	return suite;
}
