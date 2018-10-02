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
#include <poll.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "../libs/libuv/uv.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static int steps = 0;
static int nrsteps = 0;
static CuTest *gtc = NULL;
static int check = 0;

static uv_timer_t *timer_req = NULL;

static struct tests_t {
	char *desc;
	int port;
	int ssl;
	int status;
	int recvmsgnr;
	int sendmsgnr;
	char sendmsg[12][1024];
	char recvmsg[12][1024];
} tests = {
	"gmail plain", 10025, 0, 0, 0, 0, {
		"220 smtp.gmail.com ESMTP im3sm19207876wjb.13 - gsmtp\r\n",
		"250-smtp.gmail.com at your service, [108.177.96.109]\r\n"
			"250-SIZE 35882577\r\n"
			"250-8BITMIME\r\n"
			"250-AUTH LOGIN PLAIN XOAUTH2 PLAIN-CLIENTTOKEN OAUTHBEARER XOAUTH\r\n"
			"250-ENHANCEDSTATUSCODES\r\n"
			"250-PIPELINING\r\n"
			"250-CHUNKING\r\n"
			"250 SMTPUTF8\r\n",
		"235 2.7.0 Accepted\r\n",
		"250 2.1.0 OK im3sm19207876wjb.13 - gsmtp",
		"250 2.1.5 OK im3sm19207876wjb.13 - gsmtp",
		"354  Go ahead im3sm19207876wjb.13 - gsmtp",
		"250 2.0.0 OK 1477744237 im3sm19207876wjb.13 - gsmtp",
		"250 2.1.5 Flushed im3sm19207876wjb.13 - gsmtp",
		"221 2.0.0 closing connection im3sm19207876wjb.13 - gsmtp"
	},
	{
		"EHLO pilight\r\n",
		"AUTH PLAIN AHBpbGlnaHQAdGVzdA==\r\n",
		"MAIL FROM: <info@pilight.org>\r\n",
		"RCPT TO: <info@pilight.org>\r\n",
		"DATA\r\n",
		"Subject: Hello World!\r\n"
			"From: <info@pilight.org>\r\n"
			"To: <info@pilight.org>\r\n"
			"Content-Type: text/plain\r\n"
			"Mime-Version: 1.0\r\n"
			"X-Mailer: Emoticode smtp_send\r\n"
			"Content-Transfer-Encoding: 7bit\r\n"
			"\r\n"
			"Hello World!\r\n"
			".\r\n",
		"RSET\r\n",
		"QUIT\r\n"
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

static void test_event_actions_mail_check_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	struct varcont_t v;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	genericSwitchInit();
	genericLabelInit();

	char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
		"\"gui\":{},\"rules\":{},"\
		"\"settings\":{"\
		"\"smtp-host\":\"127.0.0.1\","\
		"\"smtp-port\":10025,"\
		"\"smtp-user\":\"pilight\","\
		"\"smtp-password\":\"test\","\
		"\"smtp-sender\":\"info@pilight.org\","\
		"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
		"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
		"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
		"},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_actions_mail.c", "", &file);

	FILE *f = fopen("event_actions_mail.json", "w");
	fprintf(f, config, file, file, file);
	fclose(f);
	FREE(file);

	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_mail.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	/*
	 * Valid parameters
	 */
	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", NULL));
	}

	{
		/*
		 * Missing arguments
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing json parameters
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing json parameters
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments for a parameter
		 */
		struct event_action_args_t *args = NULL;
		args = event_action_add_argument(args, "SUBJECT", NULL);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments for a parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		args = event_action_add_argument(args, "MESSAGE", NULL);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments for a parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		args = event_action_add_argument(args, "TO", NULL);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments for a parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments for a parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments for a parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Invalid variable type for parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 1;
		args = event_action_add_argument(args, "TO", &v);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Missing arguments for a parameter
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}

	{
		/*
		 * Single dot as a message
		 */
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("."); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));
	}
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	storage_gc();
	eventpool_gc();

	/*{

		// Missing smtp-host setting

		char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{"\
			"\"smtp-port\":10025,"\
			"\"smtp-user\":\"pilight\","\
			"\"smtp-password\":\"test\","\
			"\"smtp-sender\":\"info@pilight.org\","\
			"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
			"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
			"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
			"},\"hardware\":{},\"registry\":{}}";
		char *file = STRDUP(__FILE__);
		if(file == NULL) {
			OUT_OF_MEMORY
		}
		str_replace("event_actions_mail.c", "", &file);

		FILE *f = fopen("event_actions_mail.json", "w");
		fprintf(f, config, file, file, file);
		fclose(f);
		FREE(file);

		eventpool_init(EVENTPOOL_NO_THREADS);

		plua_init();

		test_set_plua_path(tc, __FILE__, "event_actions_mail.c");

		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_SETTINGS | CONFIG_DEVICES));
		event_action_init();

		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();
	}

	{

		// Missing smtp-port setting

		char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{"\
			"\"smtp-host\":\"127.0.0.1\","\
			"\"smtp-user\":\"pilight\","\
			"\"smtp-password\":\"test\","\
			"\"smtp-sender\":\"info@pilight.org\","\
			"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
			"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
			"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
			"},\"hardware\":{},\"registry\":{}}";
		char *file = STRDUP(__FILE__);
		if(file == NULL) {
			OUT_OF_MEMORY
		}
		str_replace("event_actions_mail.c", "", &file);

		FILE *f = fopen("event_actions_mail.json", "w");
		fprintf(f, config, file, file, file);
		fclose(f);
		FREE(file);

		eventpool_init(EVENTPOOL_NO_THREADS);

		plua_init();

		test_set_plua_path(tc, __FILE__, "event_actions_mail.c");

		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_SETTINGS | CONFIG_DEVICES));
		event_action_init();

		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();
	}

	{

		// Missing smtp-user setting

		char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{"\
			"\"smtp-host\":\"127.0.0.1\","\
			"\"smtp-port\":10025,"\
			"\"smtp-password\":\"test\","\
			"\"smtp-sender\":\"info@pilight.org\","\
			"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
			"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
			"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
			"},\"hardware\":{},\"registry\":{}}";
		char *file = STRDUP(__FILE__);
		if(file == NULL) {
			OUT_OF_MEMORY
		}
		str_replace("event_actions_mail.c", "", &file);

		FILE *f = fopen("event_actions_mail.json", "w");
		fprintf(f, config, file, file, file);
		fclose(f);
		FREE(file);

		eventpool_init(EVENTPOOL_NO_THREADS);

		plua_init();

		test_set_plua_path(tc, __FILE__, "event_actions_mail.c");

		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_SETTINGS | CONFIG_DEVICES));
		event_action_init();

		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();
	}

	{

		// Missing smtp-password setting

		char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{"\
			"\"smtp-host\":\"127.0.0.1\","\
			"\"smtp-port\":10025,"\
			"\"smtp-user\":\"pilight\","\
			"\"smtp-sender\":\"info@pilight.org\","\
			"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
			"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
			"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
			"},\"hardware\":{},\"registry\":{}}";
		char *file = STRDUP(__FILE__);
		if(file == NULL) {
			OUT_OF_MEMORY
		}
		str_replace("event_actions_mail.c", "", &file);

		FILE *f = fopen("event_actions_mail.json", "w");
		fprintf(f, config, file, file, file);
		fclose(f);
		FREE(file);

		eventpool_init(EVENTPOOL_NO_THREADS);

		plua_init();

		test_set_plua_path(tc, __FILE__, "event_actions_mail.c");

		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_SETTINGS | CONFIG_DEVICES));
		event_action_init();

		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();
	}

	{

		// Missing smtp-sender setting

		char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{"\
			"\"smtp-host\":\"127.0.0.1\","\
			"\"smtp-port\":10025,"\
			"\"smtp-user\":\"pilight\","\
			"\"smtp-password\":\"test\","\
			"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
			"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
			"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
			"},\"hardware\":{},\"registry\":{}}";
		char *file = STRDUP(__FILE__);
		if(file == NULL) {
			OUT_OF_MEMORY
		}
		str_replace("event_actions_mail.c", "", &file);

		FILE *f = fopen("event_actions_mail.json", "w");
		fprintf(f, config, file, file, file);
		fclose(f);
		FREE(file);

		eventpool_init(EVENTPOOL_NO_THREADS);

		plua_init();

		test_set_plua_path(tc, __FILE__, "event_actions_mail.c");

		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_SETTINGS | CONFIG_DEVICES));
		event_action_init();

		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("sendmail", args));

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();
	}*/

	event_action_gc();
	protocol_gc();
	plua_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void stop(uv_timer_t *handle) {
	uv_close((uv_handle_t *)handle, close_cb);

	uv_stop(uv_default_loop());
}

static void alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	buf->base = malloc(len);
	CuAssertPtrNotNull(gtc, buf->base);
	memset(buf->base, 0, len);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

static void write_cb(uv_write_t *req, int status) {
	uv_tcp_t *client_req = req->data;
	CuAssertIntEquals(gtc, 0, status);

	if(check == 0) {
		int r = uv_read_start((uv_stream_t *)client_req, alloc, read_cb);
		CuAssertIntEquals(gtc, 0, r);
	}
	FREE(req);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	uv_read_stop(stream);
	buf->base[nread] = '\0';

	uv_os_fd_t fd = 0;
	int r = uv_fileno((uv_handle_t *)stream, &fd);
	CuAssertIntEquals(gtc, 0, r);

	CuAssertStrEquals(gtc, tests.recvmsg[tests.recvmsgnr++], buf->base);

	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	uv_buf_t buf1;
	buf1.base = tests.sendmsg[tests.sendmsgnr];
	buf1.len = strlen(tests.sendmsg[tests.sendmsgnr]);

	tests.sendmsgnr++;
	write_req->data = stream;

	r = uv_write(write_req, stream, &buf1, 1, write_cb);
	CuAssertIntEquals(gtc, 0, r);

	if(strcmp(buf->base, "QUIT\r\n") == 0) {
		uv_close((uv_handle_t *)stream, close_cb);

		check = 1;
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif

		if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		uv_timer_init(uv_default_loop(), timer_req);
		uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 100, 0);
	}
	free(buf->base);
}

static void connection_cb(uv_stream_t *server_req, int status) {
	uv_tcp_t *client_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, client_req);
	CuAssertIntEquals(gtc, 0, status);
	uv_tcp_init(uv_default_loop(), client_req);

	int r = uv_accept(server_req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	uv_buf_t buf1;
	buf1.base = tests.sendmsg[tests.sendmsgnr];
	buf1.len = strlen(tests.sendmsg[tests.sendmsgnr]);
	tests.sendmsgnr++;
	write_req->data = client_req;
	r = uv_write(write_req, (uv_stream_t *)client_req, &buf1, 1, write_cb);
	CuAssertIntEquals(gtc, 0, r);
}

static void mail_start(int port) {
	/*
	 * Bypass socket_connect
	 */
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	uv_tcp_t *server_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, server_req);

	struct sockaddr_in addr;
	int r = uv_ip4_addr("127.0.0.1", port, &addr);
	CuAssertIntEquals(gtc, r, 0);

	uv_tcp_init(uv_default_loop(), server_req);
	uv_tcp_bind(server_req, (const struct sockaddr *)&addr, 0);

	r = uv_listen((uv_stream_t *)server_req, 128, connection_cb);
	CuAssertIntEquals(gtc, r, 0);
}

static void test_event_actions_mail_run(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 2;

	struct varcont_t v;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	eventpool_init(EVENTPOOL_THREADED);

	mail_start(10025);

	genericSwitchInit();
	genericLabelInit();

	char config[1024] = "{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
		"\"gui\":{},\"rules\":{},"\
		"\"settings\":{"\
		"\"smtp-host\":\"127.0.0.1\","\
		"\"smtp-port\":10025,"\
		"\"smtp-user\":\"pilight\","\
		"\"smtp-password\":\"test\","\
		"\"smtp-sender\":\"info@pilight.org\","\
		"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
		"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
		"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
		"},\"hardware\":{},\"registry\":{}}";
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_actions_mail.c", "", &file);

	FILE *f = fopen("event_actions_mail.json", "w");
	fprintf(f, config, file, file, file);
	fclose(f);
	FREE(file);

	eventpool_init(EVENTPOOL_NO_THREADS);

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_mail.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_action_check_arguments("sendmail", args));
	}

	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "SUBJECT", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("Hello World!"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "MESSAGE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("info@pilight.org"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TO", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_action_run("sendmail", args));
	}

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
	storage_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_mail(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_event_actions_mail_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_mail_run);

	return suite;
}
