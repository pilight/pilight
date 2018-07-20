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
#include <valgrind/valgrind.h>

#include "../libs/libuv/uv.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/lua/lua.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/events/operator.h"
#include "../libs/pilight/events/events.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static int run = 0;
static int steps = 0;
static int nrsteps = 0;
static int checktime = 0;
static uv_thread_t pth;
static CuTest *gtc = NULL;
static unsigned long interval = 0;
static uv_timer_t *timer_req = NULL;

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

static void test_event_actions_label_get_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_SETTINGS));
	event_action_init();

	char **ret = NULL;
	int nr = 0, i = 0, check = 0;
	CuAssertIntEquals(tc, 0, event_action_get_parameters("label", &nr, &ret));
	for(i=0;i<nr;i++) {
		if(stricmp(ret[i], "DEVICE") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "TO") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "FOR") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "AFTER") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "COLOR") == 0) {
			check |= 1 << i;
		}
		FREE(ret[i]);
	}
	FREE(ret);

	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	event_action_gc();
	storage_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 31, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_label_check_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	struct varcont_t v;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	genericSwitchInit();
	genericLabelInit();

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	/*
	 * Valid arguments
	 */
	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("black"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "COLOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_action_check_arguments("label", args));
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", NULL));
	}

	{
		/*
		 * Missing arguments
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Wrong order of arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Wrong order of arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("black"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "COLOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Wrong order of arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Wrong order of arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("black"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "COLOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Wrong order of arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Wrong order of arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("black"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "COLOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Too many argument for a parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("off"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Negative FOR duration
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("-1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Invalid FOR unit
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 FOO"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Invalid FOR unit
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Too many FOR arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Negative AFTER duration
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("-1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Invalid AFTER unit
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 FOO"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Invalid AFTER unit
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Too many AFTER arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * State missing value parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		args = event_action_add_argument(args, "TO", NULL);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Invalid device type
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Device not configured
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	{
		/*
		 * Device value numeric
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "DEVICE", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("label", args));
	}

	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	event_action_gc();
	protocol_gc();
	storage_gc();
	eventpool_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void *control_device(int reason, void *param) {
	struct reason_control_device_t *data = param;
	char *values = json_stringify(data->values, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	if(!RUNNING_ON_VALGRIND && checktime == 1) {
		int duration = (int)((int)timestamp.second-(int)timestamp.first);
		CuAssertTrue(gtc, (duration < interval));
	}

	steps++;
	if(run == 0) {
		CuAssertStrEquals(gtc, "{\"color\":\"red\",\"label\":\"foo\"}", values);
		CuAssertTrue(gtc, strcmp(data->dev, "label") == 0 || strcmp(data->dev, "label1") == 0);
	} else if(run == 1) {
		if(steps == 1) {
			CuAssertStrEquals(gtc, "{\"color\":\"red\",\"label\":\"foo\"}", values);
		} else if(steps == 2) {
			CuAssertStrEquals(gtc, "{\"color\":\"green\",\"label\":\"bar\"}", values);
		}
	} else if(run == 2) {
		CuAssertStrEquals(gtc, "{\"color\":\"red\",\"label\":\"1010\"}", values);
	}

	if(steps == nrsteps) {
		uv_stop(uv_default_loop());
	}
	json_free(values);
	return NULL;
}

static struct event_action_args_t *initialize_vars(int num) {
	struct event_action_args_t *args = NULL;
	struct varcont_t v;

	switch(num) {
		case 1: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("label1"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "TO", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("red"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "COLOR", &v);
			FREE(v.string_);

		} break;
		case 2: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 1010; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("red"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "COLOR", &v);
			FREE(v.string_);

		} break;
		case 3: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "TO", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("red"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "COLOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "FOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);
		} break;
		case 4: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "TO", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("black"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "COLOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("500 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "FOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("500 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);
		} break;
		case 5: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "TO", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("black"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "COLOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("500 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);
		} break;
	}

	return args;
}

static void stop(uv_work_t *req) {
	uv_stop(uv_default_loop());
}

static void test_event_actions_label_run(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	steps = 0;
	nrsteps = 2;
	interval = 3000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(1);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("label", args));

	args = initialize_vars(1);
	CuAssertIntEquals(tc, 0, event_action_run("label", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 2, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_label_run1(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 2;
	steps = 0;
	nrsteps = 1;
	interval = 3000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(2);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("label", args));

	args = initialize_vars(2);
	CuAssertIntEquals(tc, 0, event_action_run("label", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 1, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_label_run_delayed(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 1;
	steps = 0;
	nrsteps = 2;
	interval = 275000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(3);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("label", args));

	args = initialize_vars(3);
	CuAssertIntEquals(tc, 0, event_action_run("label", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 2, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

static void second_label(void *param) {
	usleep(100);

	struct event_action_args_t *args = initialize_vars(3);
	CuAssertIntEquals(gtc, 0, event_action_check_arguments("label", args));

	args = initialize_vars(3);
	CuAssertIntEquals(gtc, 0, event_action_run("label", args));
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
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 1;
	steps = 0;
	nrsteps = 2;
	interval = 275000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(3);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("label", args));

	args = initialize_vars(3);
	CuAssertIntEquals(tc, 0, event_action_run("label", args));

	uv_thread_create(&pth, second_label, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 2, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

/*
 * If a device was updated before the delayed action was executed,
 * the delayed action should be skipped.
 */
static void test_event_actions_label_run_override(CuTest *tc) {
	if(suiteFailed()) return;

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

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_SETTINGS));
	event_operator_init();
	event_action_init();
	event_function_init();
	storage_gc();

	event_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_label.json", CONFIG_DEVICES | CONFIG_RULES));

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(5);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("label", args));

	args = initialize_vars(5);
	CuAssertIntEquals(tc, 0, event_action_run("label", args));

	uv_thread_create(&pth, config_update, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth);

	event_operator_gc();
	event_action_gc();
	event_function_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_label(void) {
	CuSuite *suite = CuSuiteNew();

	char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"bar\",\"color\":\"green\"}," \
		"\"label1\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":102}],\"label\":\"bar\",\"color\":\"green\"}}," \
		"\"gui\":{},\"rules\":{"\
		"\"rule1\":{\"rule\":\"IF label.label == bar THEN label DEVICE 'label' TO bar\",\"active\":1}"\
		"},\"settings\":{" \
		"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
		"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
		"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
		"},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_actions_label.c", "", &file);

	FILE *f = fopen("event_actions_label.json", "w");
	fprintf(f, config, file, file, file);
	fclose(f);
	FREE(file);

	SUITE_ADD_TEST(suite, test_event_actions_label_get_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_label_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_label_run);
	SUITE_ADD_TEST(suite, test_event_actions_label_run1);
	SUITE_ADD_TEST(suite, test_event_actions_label_run_delayed);
	SUITE_ADD_TEST(suite, test_event_actions_label_run_overlapped);
	SUITE_ADD_TEST(suite, test_event_actions_label_run_override);

	return suite;
}
