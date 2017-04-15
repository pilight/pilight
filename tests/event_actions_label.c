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
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/events/operator.h"
#include "../libs/pilight/events/events.h"
#include "../libs/pilight/events/actions/label.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static int steps = 0;
static int nrsteps = 0;
static uv_thread_t pth;
static CuTest *gtc = NULL;
static unsigned long interval = 0;
static uv_timer_t *timer_req = NULL;

static struct rules_actions_t *obj = NULL;
static struct rules_actions_t *obj1 = NULL;

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

struct timestamp_t timestamp;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_event_actions_label_check_parameters(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	genericSwitchInit();
	genericLabelInit();
	actionLabelInit();
	CuAssertStrEquals(tc, "label", action_label->name);

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_DEVICES));

	/*
	 * Valid parameters
	 */
	{
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"foo\"],\"order\":2},\
			\"COLOR\":{\"value\":[\"black\"],\"order\":3},\
			\"FOR\":{\"value\":[\"1 SECOND\"],\"order\":4},\
			\"AFTER\":{\"value\":[\"1 SECOND\"],\"order\":5}\
		}");

		CuAssertIntEquals(tc, 0, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, action_label->checkArguments(NULL));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		FREE(obj);

		/*
		 * Missing json parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Missing json parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":3},\
			\"FOR\":{\"value\":[\"1 SECOND\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"COLOR\":{\"value\":[\"black\"],\"order\":4},\
			\"FOR\":{\"value\":[\"1 SECOND\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":3},\
			\"AFTER\":{\"value\":[\"1 SECOND\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"COLOR\":{\"value\":[\"black\"],\"order\":4},\
			\"AFTER\":{\"value\":[\"1 SECOND\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":2},\
			\"TO\":{\"value\":[\"on\"],\"order\":1}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":3},\
			\"COLOR\":{\"value\":[\"black\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Too many argument for a parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"foo\",\"bar\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Negative FOR duration
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"foo\"],\"order\":2},\
			\"FOR\":{\"value\":[\"-1 SECOND\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Invalid FOR unit
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"FOR\":{\"value\":[\"1 FOO\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Invalid FOR unit
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"FOR\":{\"value\":[\"1 SECOND MINUTE\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Too many FOR arguments
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"FOR\":{\"value\":[\"1 SECOND\",\"1 MINUTE\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Negative AFTER duration
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"AFTER\":{\"value\":[\"-1 SECOND\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Invalid AFTER unit
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"AFTER\":{\"value\":[\"1 FOO\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Invalid AFTER unit
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"AFTER\":{\"value\":[\"1 SECOND MINUTE\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Too many AFTER arguments
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"AFTER\":{\"value\":[\"1 SECOND\",\"1 MINUTE\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Too many AFTER arguments
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2},\
			\"COLOR\":{\"value\":[\"foo\",\"bar\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * State missing value parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
			\"TO\":{\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"switch\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Device not configured
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[\"foo\"],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Device value a nummeric value
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"DEVICE\":{\"value\":[1],\"order\":1},\
			\"TO\":{\"value\":[\"on\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_label->checkArguments(obj));

		json_delete(obj->parsedargs);
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

static void *control_device(int reason, void *param) {
	struct reason_control_device_t *data = param;
	char *values = json_stringify(data->values, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	int duration = (int)((int)timestamp.second-(int)timestamp.first);

	CuAssertTrue(gtc, (duration < interval));

	steps++;
	if(steps == 1) {
		CuAssertStrEquals(gtc, "{\"color\":\"red\",\"label\":\"foo\"}", values);
	} else if(steps == 2) {
		CuAssertStrEquals(gtc, "{\"color\":\"green\",\"label\":\"bar\"}", values);
	}
	if(steps == nrsteps) {
		uv_stop(uv_default_loop());
	}
	json_free(values);
	return NULL;
}

static void test_event_actions_label_run(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 1;
	interval = 3000;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();
	actionLabelInit();
	CuAssertStrEquals(tc, "label", action_label->name);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_DEVICES));

	obj = MALLOC(sizeof(struct rules_actions_t));
	CuAssertPtrNotNull(tc, obj);
	memset(obj, 0, sizeof(struct rules_actions_t));

	obj->parsedargs = json_decode("{\
		\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
		\"TO\":{\"value\":[\"foo\"],\"order\":2},\
		\"COLOR\":{\"value\":[\"red\"],\"order\":3}\
	}");

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	CuAssertIntEquals(tc, 0, action_label->checkArguments(obj));
	CuAssertIntEquals(tc, 0, action_label->run(obj));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	json_delete(obj->parsedargs);
	FREE(obj);

	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_label_run_delayed(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 2;
	interval = 275000;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();
	actionLabelInit();
	CuAssertStrEquals(tc, "label", action_label->name);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_DEVICES));

	obj = MALLOC(sizeof(struct rules_actions_t));
	CuAssertPtrNotNull(tc, obj);
	memset(obj, 0, sizeof(struct rules_actions_t));

	obj->parsedargs = json_decode("{\
		\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
		\"TO\":{\"value\":[\"foo\"],\"order\":2},\
		\"COLOR\":{\"value\":[\"red\"],\"order\":3},\
		\"FOR\":{\"value\":[\"250 MILLISECOND\"],\"order\":4},\
		\"AFTER\":{\"value\":[\"250 MILLISECOND\"],\"order\":5}\
	}");

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	CuAssertIntEquals(tc, 0, action_label->checkArguments(obj));
	CuAssertIntEquals(tc, 0, action_label->run(obj));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	json_delete(obj->parsedargs);
	FREE(obj);

	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void second_label(void *param) {
	obj1 = MALLOC(sizeof(struct rules_actions_t));
	CuAssertPtrNotNull(gtc, obj1);
	memset(obj1, 0, sizeof(struct rules_actions_t));

	usleep(100);

	obj1->parsedargs = json_decode("{\
		\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
		\"TO\":{\"value\":[\"foo\"],\"order\":2},\
		\"COLOR\":{\"value\":[\"red\"],\"order\":3},\
		\"FOR\":{\"value\":[\"250 MILLISECOND\"],\"order\":4},\
		\"AFTER\":{\"value\":[\"250 MILLISECOND\"],\"order\":5}\
	}");

	CuAssertIntEquals(gtc, 0, action_label->checkArguments(obj1));
	CuAssertIntEquals(gtc, 0, action_label->run(obj1));
}

static struct reason_config_update_t update = {
	"update", LABEL, 1, 1, { "label" },	1, {
		{ "label", { .string_ = "monkey" }, 0, JSON_STRING }
	}, NULL
};

static void config_update(void *param) {
	eventpool_trigger(REASON_CONFIG_UPDATE, NULL, &update);
}

static void test_event_actions_label_run_overlapped(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 2;
	interval = 275000;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();
	actionLabelInit();
	CuAssertStrEquals(tc, "label", action_label->name);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_DEVICES));

	obj = MALLOC(sizeof(struct rules_actions_t));
	CuAssertPtrNotNull(tc, obj);
	memset(obj, 0, sizeof(struct rules_actions_t));

	obj->parsedargs = json_decode("{\
		\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
		\"TO\":{\"value\":[\"foo\"],\"order\":2},\
		\"COLOR\":{\"value\":[\"black\"],\"order\":3},\
		\"FOR\":{\"value\":[\"500 MILLISECOND\"],\"order\":4},\
		\"AFTER\":{\"value\":[\"500 MILLISECOND\"],\"order\":5}\
	}");

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	CuAssertIntEquals(tc, 0, action_label->checkArguments(obj));
	CuAssertIntEquals(tc, 0, action_label->run(obj));

	uv_thread_create(&pth, second_label, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	json_delete(obj->parsedargs);
	json_delete(obj1->parsedargs);
	FREE(obj);
	FREE(obj1);

	uv_thread_join(&pth);

	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void stop(uv_work_t *req) {
	uv_stop(uv_default_loop());
}

/*
 * If a device was updated before the delayed action was executed,
 * the delayed action should be skipped.
 */
static void test_event_actions_label_run_override(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 1;
	interval = 575000;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();
	actionLabelInit();
	CuAssertStrEquals(tc, "label", action_label->name);

	event_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_DEVICES | CONFIG_RULES));

	obj = MALLOC(sizeof(struct rules_actions_t));
	CuAssertPtrNotNull(tc, obj);
	memset(obj, 0, sizeof(struct rules_actions_t));

	obj->parsedargs = json_decode("{\
		\"DEVICE\":{\"value\":[\"label\"],\"order\":1},\
		\"TO\":{\"value\":[\"foo\"],\"order\":2},\
		\"COLOR\":{\"value\":[\"black\"],\"order\":3},\
		\"AFTER\":{\"value\":[\"500 MILLISECOND\"],\"order\":4}\
	}");

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 750, -1);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	CuAssertIntEquals(tc, 0, action_label->checkArguments(obj));
	CuAssertIntEquals(tc, 0, action_label->run(obj));

	uv_thread_create(&pth, config_update, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	json_delete(obj->parsedargs);
	FREE(obj);

	uv_thread_join(&pth);

	event_operator_gc();
	event_function_gc();
	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 0, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_label(void) {
	CuSuite *suite = CuSuiteNew();

	FILE *f = fopen("event_actions_label.json", "w");
	fprintf(f,
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"bar\",\"color\":\"green\"}}," \
		"\"gui\":{},\"rules\":{"\
			"\"rule1\":{\"rule\":\"IF label.label == bar THEN label DEVICE label TO bar\",\"active\":1}"\
		"},\"settings\":{},\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	SUITE_ADD_TEST(suite, test_event_actions_label_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_label_run);
	SUITE_ADD_TEST(suite, test_event_actions_label_run_delayed);
	SUITE_ADD_TEST(suite, test_event_actions_label_run_overlapped);
	SUITE_ADD_TEST(suite, test_event_actions_label_run_override);

	return suite;
}
