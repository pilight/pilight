/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/binary.h"
#include "../libs/pilight/events/events.h"
#include "../libs/pilight/events/operator.h"
#include "../libs/pilight/events/operators/eq.h"
#include "../libs/pilight/events/operators/ne.h"
#include "../libs/pilight/events/operators/or.h"
#include "../libs/pilight/events/operators/gt.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/events/actions/switch.h"
#include "../libs/pilight/events/actions/dim.h"
#include "../libs/pilight/events/actions/toggle.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/events/functions/random.h"
#include "../libs/pilight/events/functions/date_format.h"
#include "../libs/pilight/events/functions/date_add.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_dimmer.h"
#include "../libs/pilight/protocols/433.92/arctech_switch.h"
#include "../libs/pilight/protocols/API/datetime.h"

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
	}
};

static char *values[] = {
	NULL,
	"{\"dimlevel\":2}"
};

static struct reason_control_device_t receives[] = {
	{ "switch", "on", NULL },
	{ "dimmer", "on", NULL },
	{ "switch1", "on", NULL }
};

struct tests_t {
	char *title;
	char *config;
	int runmode;
	int type;
	void *updates;
	struct reason_control_device_t *receives[10];
	int nrreceives[2];
} tests_t;

static struct tests_t get_tests[] = {
	{
		"simple_true_formula",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"simple_false_formula",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 0 THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"with_hooks",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF ((1 == 1)) THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"with_and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 AND a == a THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"single_quoted_with_and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == '1' THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"double_quoted_with_and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == \\\"1\\\" THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"with_or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 OR a == b THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"with_or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR a == a THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"multiple_and_or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR a == a AND 1 == a THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"multiple_and_or",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 OR a == b AND 1 == 1 THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"multiple_and_or_hooks",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 2 OR 1 == 1 AND 2 == 1 AND 1 == 1 THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"multiple_and_or_hooks",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 0 OR (2 == 0 AND (2 == 2 OR 1 == 1)) THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"gt_and",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 0 > 10 AND 10 > 2 THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"with_dot",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF . == on THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_NOWAIT,
		0, &updates1[0],
		{ NULL }, { 0 }
	},
	{
		"valid_device_param",
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF switch.state == off THEN switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"valid_device_param",
		"{\"devices\":{\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF dimmer.dimlevel == 10 THEN switch DEVICE dimmer TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1] },
		{ 1, 0 }
	},
	{
		"valid_device_param",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"switch1\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"on\"}"\
		"}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF switch.state != switch1.state THEN switch DEVICE switch TO switch1.state\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"function_as_device_param",
		"{\"devices\":{\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}}," \
		"\"gui\":{},"\
		"\"rules\":{\"switch\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO RANDOM(1, 10)\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ NULL }, { 0 }
	},
	{
		"valid_device_param_in_function",
		"{\"devices\":{"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10},"\
			"\"dimmer1\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":2},"\
			"\"dimmer2\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":5}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO RANDOM(dimmer1.dimlevel, dimmer2.dimlevel)\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ NULL }, { 0 }
	},
	{
		"dimmer_values_from_function",
		"{\"devices\":{"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO DATE_FORMAT('2016,01,02', '%%Y,%%m,%%d', %%d)\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1] },
		{ 1, 0 }
	},
	{
		"multiple_actions",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO 2 AND switch DEVICE switch TO on\",\"active\":1}},"\
		"\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"multiple_rules",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO 2\",\"active\":1},"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE switch TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[1], &receives[0] },
		{ 2, 0 }
	},
	{
		"multiple_rules_one_inactive",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"},"\
			"\"dimmer\":{\"protocol\":[\"generic_dimmer\"],\"id\":[{\"id\":100}],\"state\":\"off\",\"dimlevel\":10}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"dimmer\":{\"rule\":\"IF 1 == 1 THEN dim DEVICE dimmer TO 2\",\"active\":0},"\
			"\"switch\":{\"rule\":\"IF 1 == 1 THEN switch DEVICE switch TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"nested_functions",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF DATE_FORMAT(DATE_ADD('2015-01-01 21:00:00', RANDOM(1, 59) MINUTE), '%%Y-%%m-%%d %%H:%%M:%%S', '%%H.%%M') > 21.00 THEN switch DEVICE switch TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[1],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"received_device",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF arctech_switch.id == 1 AND arctech_switch.unit == 1 THEN switch DEVICE switch TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		1, &updates2[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"date_format_with_device_parameter",
		"{\"devices\":{"\
			"\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":20,\"minute\":56,\"second\":48,\"weekday\":1,\"dst\":0},"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF switch.state == off AND DATE_FORMAT(test, %%Y%%m%%d%%H%%M%%S) == 20160314205648 THEN switch DEVICE switch TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"date_add_with_device_parameter",
		"{\"devices\":{"\
			"\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895168,\"latitude\":52.370216}],\"year\":2016,\"month\":3,\"day\":14,\"hour\":20,\"minute\":56,\"second\":48,\"weekday\":1,\"dst\":0},"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF switch.state == off AND DATE_FORMAT(DATE_ADD(test, +12 HOUR), '%%Y-%%m-%%d %%H:%%M:%%S', '%%Y%%m%%d%%H%%M%%S') == 20160315085648 THEN switch DEVICE switch TO on\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_DEFAULT,
		0, &updates1[0],
		{ &receives[0] },
		{ 1, 0 }
	},
	{
		"only_execute_device_rules",
		"{\"devices\":{"\
			"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":1}],\"state\":\"off\"},"\
			"\"switch1\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":2}],\"state\":\"off\"}"\
		"},"\
		"\"gui\":{},"\
		"\"rules\":{"\
			"\"switch\":{\"rule\":\"IF switch1.state == off THEN toggle DEVICE switch BETWEEN on AND off\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}",
		UV_RUN_NOWAIT,
		0, &updates1[0],
		{ &receives[2] },
		{ 1, 0 }
	}
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

		FILE *f = fopen("events.json", "w");
		fprintf(f,
			get_tests[testnr].config,
			""
		);
		fclose(f);

		operatorEqInit();
		operatorNeInit();
		operatorOrInit();
		operatorGtInit();
		actionSwitchInit();
		actionToggleInit();
		actionDimInit();
		functionRandomInit();
		functionDateFormatInit();
		functionDateAddInit();
		arctechSwitchInit();
		genericSwitchInit();
		genericDimmerInit();
		datetimeInit();

		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("events.json", CONFIG_DEVICES | CONFIG_RULES));

		eventpool_init(EVENTPOOL_NO_THREADS);
		eventpool_callback(REASON_CONTROL_DEVICE, control_device);
		event_init();

		if(get_tests[testnr].type == 0) {
			eventpool_trigger(REASON_CONFIG_UPDATE, NULL, get_tests[testnr].updates);
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

		event_operator_gc();
		event_action_gc();
		event_function_gc();
		protocol_gc();
		storage_gc();
		eventpool_gc();

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
