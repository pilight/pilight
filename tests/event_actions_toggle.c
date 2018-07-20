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
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static int steps = 0;
static int nrsteps = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_event_actions_toggle_get_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_toggle.json", CONFIG_SETTINGS));
	event_action_init();

	char **ret = NULL;
	int nr = 0, i = 0, check = 0;
	CuAssertIntEquals(tc, 0, event_action_get_parameters("toggle", &nr, &ret));
	for(i=0;i<nr;i++) {
		if(stricmp(ret[i], "DEVICE") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "BETWEEN") == 0) {
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

	CuAssertIntEquals(tc, 3, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_toggle_check_parameters(CuTest *tc) {
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
	CuAssertIntEquals(tc, 0, storage_read("event_actions_toggle.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	/*
	 * Valid arguments
	 */
	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("off"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", NULL));
	}

	{
		/*
		 * Missing arguments
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Wrong order of arguments
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Missing argument for parameter
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Missing argument for parameter
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("off"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Invalid state for switch device
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("bar"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Invalid state for switch device (nummeric value)
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "BETWEEN", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "BETWEEN", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * State missing value parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		args = event_action_add_argument(args, "TO", NULL);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Invalid device for toggle action
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("off"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Device not configured
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("off"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
	}

	{
		/*
		 * Device value a nummeric value
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "DEVICE", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("off"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BETWEEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("toggle", args));
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
	strcpy(data2->devices[data2->nrdev++], data1->dev);
	strncpy(data2->origin, "update", 255);
	data2->type = SWITCH;
	data2->uuid = NULL;

	eventpool_trigger(REASON_CONFIG_UPDATE, reason_config_update_free, data2);

	if(steps <= 2) {
		CuAssertStrEquals(gtc, "on", data1->state);
	} else if(steps <= 4) {
		CuAssertStrEquals(gtc, "off", data1->state);
	}
	if(steps == nrsteps) {
		uv_stop(uv_default_loop());
	}
	return NULL;
}

static struct event_action_args_t *initialize_vars(void) {
	struct event_action_args_t *args = NULL;
	struct varcont_t v;

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("switch"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "DEVICE", &v);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("switch1"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "DEVICE", &v);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("on"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "BETWEEN", &v);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("off"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "BETWEEN", &v);
	FREE(v.string_);

	return args;
}

static void stop(uv_work_t *req) {
	uv_stop(uv_default_loop());
}

static void test_event_actions_toggle_run(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 4;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	genericSwitchInit();
	genericLabelInit();

	plua_init();
	plua_coverage_output(__FUNCTION__);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_toggle.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	struct event_action_args_t *args = initialize_vars();
	CuAssertIntEquals(tc, 0, event_action_check_arguments("toggle", args));

	args = initialize_vars();
	CuAssertIntEquals(tc, 0, event_action_run("toggle", args));

	/*
	 * Give storage time to update
	 */
	int x = 0;
	for(x=0;x<10;x++) {
		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		usleep(100);
	}

	args = initialize_vars();
	CuAssertIntEquals(tc, 0, event_action_run("toggle", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	protocol_gc();
	storage_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 4, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_toggle(void) {
	CuSuite *suite = CuSuiteNew();

	char config[1024] = "{\"devices\":{"\
		"\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"switch1\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":101}],\"state\":\"off\"}}," \
		"\"gui\":{},\"rules\":{},\"settings\":{"\
		"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
		"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
		"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
		"},\"hardware\":{},\"registry\":{}}";

	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_actions_toggle.c", "", &file);

	FILE *f = fopen("event_actions_toggle.json", "w");
	fprintf(f, config, file, file, file);
	fclose(f);
	FREE(file);

	SUITE_ADD_TEST(suite, test_event_actions_toggle_get_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_toggle_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_toggle_run);

	return suite;
}
