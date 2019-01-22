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
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/config/settings.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/events/events.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/events/operator.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/protocols/433.92/arctech_dimmer.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static int steps = 0;
static int nrsteps = 0;
static int run = 0;
static int checktime = 0;
static uv_thread_t pth;
static CuTest *gtc = NULL;
static unsigned long interval[15] = {0};
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

static void test_event_actions_dim_get_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS));
	event_action_init();

	char **ret = NULL;
	int nr = 0, i = 0, check = 0;
	CuAssertIntEquals(tc, 0, event_action_get_parameters("dim", &nr, &ret));
	for(i=0;i<nr;i++) {
		if(stricmp(ret[i], "DEVICE") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "TO") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "FROM") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "FOR") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "AFTER") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "IN") == 0) {
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

	CuAssertIntEquals(tc, 63, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_dim_check_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	struct varcont_t v;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	arctechDimmerInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	/*
	 * Valid parameters
	 */
	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 10; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("10 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", NULL));
	}

	{
		/*
		 * Missing arguments
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Wrong order of parameters
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Wrong order of parameters
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Wrong order of parameters
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Wrong order of parameters
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Calling IN without a FROM
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Wrong order of parameters
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Calling FROM without an IN
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Too many argument for a parameter
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Negative FOR duration
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("-1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid FOR unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 FOO"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid FOR unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 SECOND MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid FOR unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		v.string_ = STRDUP("1 MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FOR", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Negative AFTER duration
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("-1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid AFTER unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 FOO"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid AFTER unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 SECOND MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid AFTER unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		v.string_ = STRDUP("1 MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "AFTER", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Too many FROM arguments
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Negative IN duration
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("-1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid IN unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 FOO"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid IN unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 SECOND MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid IN unit
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		v.string_ = STRDUP("1 MINUTE"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid dimlevel for dimmer device
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 16; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid dimlevel for dimmer device
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = -1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Missing value object for TO argument
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		args = event_action_add_argument(args, "TO", NULL);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid dimlevel for dimmer device
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 16; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid dimlevel for dimmer device
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = -1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid dimlevel vartype
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("a"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "FROM", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid dimlevel vartype
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 10; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("a"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "FROM", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "IN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Device does not exist
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("foo"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
	}

	{
		/*
		 * Invalid device for dim action
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("label"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "DEVICE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("dim", args));
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

static void *control_device(int reason, void *param, void *userdata) {
	struct reason_control_device_t *data = param;
	char *values = json_stringify(data->values, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	if(!RUNNING_ON_VALGRIND && checktime == 1) {
		int duration = (int)((int)timestamp.second-(int)timestamp.first);
		CuAssertTrue(gtc, (duration < interval[steps]));
	}

	steps++;
	if(run == 1) {
		if(steps == 1) {
			CuAssertStrEquals(gtc, "{\"dimlevel\":2}", values);
		} else {
			CuAssertStrEquals(gtc, "{\"dimlevel\":1}", values);
		}
	} else if(run == 2) {
		char cmp[32], *p = cmp;
		sprintf(p, "{\"dimlevel\":%d}", 10-(steps-1));
		CuAssertStrEquals(gtc, p, values);
	} else if(run == 3) {
		if(steps == 1) {
			CuAssertStrEquals(gtc, "{\"dimlevel\":5}", values);
		} else {
			CuAssertStrEquals(gtc, "{\"dimlevel\":1}", values);
		}
	}
	if(steps == nrsteps) {
		uv_stop(uv_default_loop());
	}
	json_free(values);
	return NULL;
}

static struct event_action_args_t *initialize_vars(int test) {
	struct event_action_args_t *args = NULL;
	struct varcont_t v;

	switch(test) {
		case 1: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			// memset(&v, 0, sizeof(struct varcont_t));
			// v.string_ = STRDUP("dimmer1"); v.type_ = JSON_STRING;
			// args = event_action_add_argument(args, "DEVICE", &v);
			// FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);
		} break;
		case 2: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "FOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);
		} break;
		case 3: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 10; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "FROM", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "IN", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("500 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "FOR", &v);
			FREE(v.string_);
		} break;
		case 4: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 10; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "FROM", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("1 SECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "IN", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("500 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "FOR", &v);
			FREE(v.string_);
		} break;
		case 5: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("500 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "FOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("500 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);
		} break;
		case 6: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 5; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "FOR", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("250 MILLISECOND"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "AFTER", &v);
			FREE(v.string_);
		} break;
		case 7: {
			memset(&v, 0, sizeof(struct varcont_t));
			v.string_ = STRDUP("dimmer"); v.type_ = JSON_STRING;
			args = event_action_add_argument(args, "DEVICE", &v);
			FREE(v.string_);

			memset(&v, 0, sizeof(struct varcont_t));
			v.number_ = 2; v.type_ = JSON_NUMBER; v.decimals_ = 0;
			args = event_action_add_argument(args, "TO", &v);

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

static void test_event_actions_dim_run(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	steps = 0;
	nrsteps = 1;
	interval[0] = 3000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	arctechDimmerInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(1);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("dim", args));

	args = initialize_vars(1);
	CuAssertIntEquals(tc, 0, event_action_run("dim", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	event_function_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 1, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_dim_run_delayed(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 1;
	steps = 0;
	nrsteps = 2;
	interval[0] = 275000;
	interval[1] = 275000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	arctechDimmerInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 750, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(2);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("dim", args));

	args = initialize_vars(2);
	CuAssertIntEquals(tc, 0, event_action_run("dim", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	event_function_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 2, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_dim_run_stepped_down(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	int i = 0;
	run = 0;
	steps = 0;
	nrsteps = 10;
	interval[0] = 650000;
	for(i=1;i<9;i++) {
		interval[i] = 150000;
	}
	interval[9] = 275000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	arctechDimmerInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 15000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(3);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("dim", args));

	args = initialize_vars(3);
	CuAssertIntEquals(tc, 0, event_action_run("dim", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	event_function_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 10, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_dim_run_stepped_up(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	int i = 0;
	run = 0;
	steps = 0;
	nrsteps = 10;
	interval[0] = 655000;
	for(i=1;i<9;i++) {
		interval[i] = 150000;
	}
	interval[9] = 275000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	arctechDimmerInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 15000, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(4);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("dim", args));

	args = initialize_vars(4);
	CuAssertIntEquals(tc, 0, event_action_run("dim", args));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	plua_gc();
	event_action_gc();
	event_function_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 10, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

static void second_dimmer(void *param) {
	usleep(100);

	struct event_action_args_t *args = initialize_vars(6);
	CuAssertIntEquals(gtc, 0, event_action_check_arguments("dim", args));

	args = initialize_vars(6);
	CuAssertIntEquals(gtc, 0, event_action_run("dim", args));
}

static struct reason_config_update_t update = {
	"update", DIMMER, 1, 1, { "dimmer" },	1, {
		{ "state", { .string_ = "off" }, 0, JSON_STRING }
	}, NULL
};

static void config_update(void *param) {
	eventpool_trigger(REASON_CONFIG_UPDATE, NULL, &update);
}

static void test_event_actions_dim_run_overlapped(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 3;
	steps = 0;
	nrsteps = 2;
	interval[0] = 275000;
	interval[1] = 275000;
	checktime = 0;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	arctechDimmerInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1500, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device, NULL);

	struct event_action_args_t *args = initialize_vars(5);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("dim", args));

	args = initialize_vars(5);
	CuAssertIntEquals(tc, 0, event_action_run("dim", args));

	uv_thread_create(&pth, second_dimmer, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth);

	plua_gc();
	event_action_gc();
	event_function_gc();
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
static void test_event_actions_dim_run_override(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 4;
	steps = 0;
	nrsteps = 1;
	interval[0] = 575000;
	checktime = 1;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	arctechDimmerInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_dim.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_SETTINGS));
	event_operator_init();
	event_action_init();
	event_function_init();
	storage_gc();

	event_init();
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_dimmer.json", CONFIG_DEVICES | CONFIG_RULES));

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 500, 0);

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device, NULL);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	struct event_action_args_t *args = initialize_vars(7);
	CuAssertIntEquals(tc, 0, event_action_check_arguments("dim", args));

	args = initialize_vars(7);
	CuAssertIntEquals(tc, 0, event_action_run("dim", args));

	uv_thread_create(&pth, config_update, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth);

	plua_gc();
	event_action_gc();
	event_function_gc();
	protocol_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 0, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_dim(void) {
	CuSuite *suite = CuSuiteNew();

	char config[1024] = "{\"devices\":{\"dimmer\":{\"protocol\":[\"kaku_dimmer\"],\"id\":[{\"id\":100,\"unit\":5}],\"state\":\"off\",\"dimlevel\":1}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"bar\",\"color\":\"green\"}}," \
		"\"gui\":{},\"rules\":{"\
			"\"rule1\":{\"rule\":\"IF dimmer.state == on THEN dim DEVICE dimmer TO 1\",\"active\":1}"\
		"},\"settings\":{" \
		"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
		"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
		"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
		"},\"hardware\":{},\"registry\":{}}";

	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_actions_dim.c", "", &file);

	FILE *f = fopen("event_actions_dimmer.json", "w");
	fprintf(f, config, file, file, file);
	fclose(f);
	FREE(file);	

	SUITE_ADD_TEST(suite, test_event_actions_dim_get_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_dim_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_dim_run);
	SUITE_ADD_TEST(suite, test_event_actions_dim_run_delayed);
	SUITE_ADD_TEST(suite, test_event_actions_dim_run_stepped_down);
	SUITE_ADD_TEST(suite, test_event_actions_dim_run_stepped_up);
	SUITE_ADD_TEST(suite, test_event_actions_dim_run_overlapped);
	SUITE_ADD_TEST(suite, test_event_actions_dim_run_override);

	return suite;
}
