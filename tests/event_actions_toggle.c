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
	#include <sys/time.h>
#endif

#include "../libs/libuv/uv.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/events/actions/toggle.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static int steps = 0;
static int nrsteps = 0;
static CuTest *gtc = NULL;

static struct rules_actions_t *obj = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_event_actions_toggle_check_parameters(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	genericSwitchInit();
	genericLabelInit();
	actionToggleInit();
	CuAssertStrEquals(tc, "toggle", action_toggle->name);

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_toggle.json", CONFIG_DEVICES));

	/*
	 * Valid parameters
	 */
	{
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
			\"BETWEEN\":{\"value\":[\"on\",\"off\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, 0, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(NULL));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		FREE(obj);

		/*
		 * Missing json parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Missing json parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":2},\
			\"BETWEEN\":{\"value\":[\"on\"],\"order\":1}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Missing arguments for a parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
			\"BETWEEN\":{\"value\":[\"on\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Too many argument for a parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
			\"BETWEEN\":{\"value\":[\"on\",\"off\",\"foo\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Invalid state for switch device
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
			\"BETWEEN\":{\"value\":[\"foo\",\"bar\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Invalid state for switch device (nummeric value)
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
			\"BETWEEN\":{\"value\":[1, 2],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * State missing value parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
			\"BETWEEN\":{\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Invalid device for toggle action
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"BETWEEN\":{\"value\":[\"on\", \"off\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Device not configured
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[\"foo\"],\"order\":1},\
			\"BETWEEN\":{\"value\":[\"on\", \"off\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);

		/*
		 * Device value a nummeric value
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->arguments = json_decode("{\
			\"DEVICE\":{\"value\":[1],\"order\":1},\
			\"BETWEEN\":{\"value\":[\"on\",\"off\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_toggle->checkArguments(obj));

		json_delete(obj->arguments);
		FREE(obj);
	}

	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	event_action_gc();
	protocol_gc();
	storage_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void *reason_config_update_free(void *param) {
	struct reason_config_update_t *data = param;
	FREE(data);
	return NULL;
}

static void *control_device(int reason, void *param) {
	struct reason_control_device_t *data1 = param;

	steps++;

	struct reason_config_update_t *data2 = MALLOC(sizeof(struct reason_config_update_t));
	if(data2 == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(data2, 0, sizeof(struct reason_config_update_t));

	data2->timestamp = 1;
	strcpy(data2->values[data2->nrval].string_, data1->state);
	strcpy(data2->values[data2->nrval].name, "state");
	data2->values[data2->nrval].type = JSON_STRING;
	data2->nrval++;
	strcpy(data2->devices[data2->nrdev++], "switch");
	strncpy(data2->origin, "update", 255);
	data2->type = SWITCH;
	data2->uuid = NULL;

	eventpool_trigger(REASON_CONFIG_UPDATE, reason_config_update_free, data2);

	if(steps == 1) {
		CuAssertStrEquals(gtc, "on", data1->state);
	}
	if(steps == 2) {
		CuAssertStrEquals(gtc, "off", data1->state);
	}
	if(steps == nrsteps) {
		uv_stop(uv_default_loop());
	}
	return NULL;
}

static void test_event_actions_toggle_run(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 2;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	genericSwitchInit();
	genericLabelInit();
	actionToggleInit();
	CuAssertStrEquals(tc, "toggle", action_toggle->name);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_toggle.json", CONFIG_DEVICES));

	obj = MALLOC(sizeof(struct rules_actions_t));
	CuAssertPtrNotNull(tc, obj);
	memset(obj, 0, sizeof(struct rules_actions_t));

	obj->arguments = json_decode("{\
		\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
		\"BETWEEN\":{\"value\":[\"on\",\"off\"],\"order\":2}\
	}");


	CuAssertIntEquals(tc, 0, action_toggle->checkArguments(obj));
	CuAssertIntEquals(tc, 0, action_toggle->run(obj));

	/*
	 * Give storage time to update
	 */
	int x = 0;
	for(x=0;x<10;x++) {
		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		usleep(100);
	}

	CuAssertIntEquals(tc, 0, action_toggle->run(obj));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	json_delete(obj->arguments);
	FREE(obj);

	event_action_gc();
	protocol_gc();
	storage_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_toggle(void) {
	CuSuite *suite = CuSuiteNew();

	FILE *f = fopen("event_actions_toggle.json", "w");
	fprintf(f,
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
		"\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	SUITE_ADD_TEST(suite, test_event_actions_toggle_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_toggle_run);

	return suite;
}
