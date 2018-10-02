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
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static int test = 0;
static struct tests_t {
	char sendmsg[1024];
	char recvmsg[1024];
} tests[] = {
	{
		"HTTP/1.0 200 OK\r\n"
		"Content-Type: application/json; charset=utf-8\r\n"
		"X-Ratelimit-Limit: 16384\r\n"
		"X-Ratelimit-Remaining: 16384\r\n"
		"X-Ratelimit-Reset: 1531842725\r\n"
		"X-Cloud-Trace-Context: 1106ec116645d04e69145d905c1580d9\r\n"
		"Date: Tue, 17 Jul 2018 15:02:01 GMT\r\n"
		"Server: Google Frontend\r\n"
		"Content-Length: 428\r\n\r\n"
		"{\"active\":true,\"iden\":\"zHHk5f59OL4veP7y1XX1\",\"created\":1531896331.5908997,\"modified\":1531896331.6010456,\"type\":\"note\",\"dismissed\":false,\"direction\":\"self\",\"sender_iden\":\"pilight\",\"sender_email\":info@pilight.org,\"sender_email_normalized\":\"info@pilight.org\",\"sender_name\":\"pilight\",\"receiver_iden\":\"pilight\",\"receiver_email\":\"info@pilight.org\",\"receiver_email_normalized\":\"info@pilight.org\",\"title\":\"test\",\"body\":\"this is a test\"}",
		"POST /v2/pushes HTTP/1.0\r\n"
		"Host: 127.0.0.1\r\n"
		"User-Agent: pilight\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: 59\r\n\r\n"
		"{\"type\": \"note\", \"title\": \"test\", \"body\": \"this is a test\"}"
	}
};

static int steps = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static int plua_error(lua_State *L) {
	if(lua_type(L, -1) == LUA_TSTRING) {
		if(strcmp(lua_tostring(L, -1), "pushbullet action succeeded with message \"{\"active\":true,\"iden\":\"zHHk5f59OL4veP7y1XX1\",\"created\":1531896331.5908997,\"modified\":1531896331.6010456,\"type\":\"note\",\"dismissed\":false,\"direction\":\"self\",\"sender_iden\":\"pilight\",\"sender_email\":info@pilight.org,\"sender_email_normalized\":\"info@pilight.org\",\"sender_name\":\"pilight\",\"receiver_iden\":\"pilight\",\"receiver_email\":\"info@pilight.org\",\"receiver_email_normalized\":\"info@pilight.org\",\"title\":\"test\",\"body\":\"this is a test\"}\"") == 0) {
			steps = 1;
		}
	}
	return 1;
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void stop(uv_timer_t *handle) {
	uv_close((uv_handle_t *)handle, close_cb);

	uv_stop(uv_default_loop());
}

static void test_event_actions_pushbullet_get_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_pushbullet.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_pushbullet.json", CONFIG_SETTINGS));
	event_action_init();

	char **ret = NULL;
	int nr = 0, i = 0, check = 0;
	CuAssertIntEquals(tc, 0, event_action_get_parameters("pushbullet", &nr, &ret));
	for(i=0;i<nr;i++) {
		if(stricmp(ret[i], "TITLE") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "BODY") == 0) {
			check |= 1 << i;
		}
		if(stricmp(ret[i], "TOKEN") == 0) {
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

	CuAssertIntEquals(tc, 7, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_event_actions_pushbullet_check_parameters(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	struct varcont_t v;
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	genericSwitchInit();
	genericLabelInit();

	plua_init();

	test_set_plua_path(tc, __FILE__, "event_actions_pushbullet.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_pushbullet.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	/*
	 * Valid arguments
	 */
	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TITLE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("this is a test"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BODY", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("zHHk5f59OL4veP7y1XX1"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TOKEN", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, 0, event_action_check_arguments("pushbullet", args));
	}

	/*
	 * Different argument types
	 */
	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TITLE", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "BODY", &v);

		memset(&v, 0, sizeof(struct varcont_t));
		v.number_ = 1; v.type_ = JSON_NUMBER; v.decimals_ = 0;
		args = event_action_add_argument(args, "TOKEN", &v);

		CuAssertIntEquals(tc, 0, event_action_check_arguments("pushbullet", args));
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, event_action_check_arguments("pushbullet", NULL));
	}

	{
		/*
		 * Missing arguments
		 */
		struct event_action_args_t *args = NULL;
		CuAssertIntEquals(tc, -1, event_action_check_arguments("pushbullet", args));

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TITLE", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("pushbullet", args));
	}

	{
		/*
		 * Missing argument for parameter
		 */
		struct event_action_args_t *args = NULL;

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TITLE", &v);
		FREE(v.string_);

		args = event_action_add_argument(args, "BODY", NULL);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("pushbullet", args));
	}

	{
		struct event_action_args_t *args = NULL;
		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "TITLE", &v);
		FREE(v.string_);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("this is a test"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BODY", &v);
		FREE(v.string_);

		args = event_action_add_argument(args, "TOKEN", NULL);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("pushbullet", args));
	}

	{
		/*
		 * Missing argument for parameter
		 */
		struct event_action_args_t *args = NULL;
		args = event_action_add_argument(args, "TITLE", NULL);

		memset(&v, 0, sizeof(struct varcont_t));
		v.string_ = STRDUP("this is a test"); v.type_ = JSON_STRING;
		args = event_action_add_argument(args, "BODY", &v);
		FREE(v.string_);

		CuAssertIntEquals(tc, -1, event_action_check_arguments("pushbullet", args));
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

static void alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	buf->base = malloc(len);
	CuAssertPtrNotNull(gtc, buf->base);
	memset(buf->base, 0, len);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

static void write_cb(uv_write_t *req, int status) {
	FREE(req);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	uv_read_stop(stream);
	buf->base[nread] = '\0';

	uv_os_fd_t fd = 0;
	int r = uv_fileno((uv_handle_t *)stream, &fd);
	CuAssertIntEquals(gtc, 0, r);

	CuAssertStrEquals(gtc, tests[test].recvmsg, buf->base);

	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	uv_buf_t buf1;
	buf1.base = tests[test].sendmsg;
	buf1.len = strlen(tests[test].sendmsg);

	write_req->data = stream;

	r = uv_write(write_req, stream, &buf1, 1, write_cb);
	CuAssertIntEquals(gtc, 0, r);

	free(buf->base);
}

static void connection_cb(uv_stream_t *server_req, int status) {
	uv_tcp_t *client_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, client_req);
	CuAssertIntEquals(gtc, 0, status);
	uv_tcp_init(uv_default_loop(), client_req);

	int r = uv_accept(server_req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_read_start((uv_stream_t *)client_req, alloc, read_cb);
	CuAssertIntEquals(gtc, 0, r);
}

static void http_start(int port) {
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

static struct event_action_args_t *initialize_vars(void) {
	struct event_action_args_t *args = NULL;
	struct varcont_t v;

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("test"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "TITLE", &v);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("this is a test"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "BODY", &v);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("zHHk5f59OL4veP7y1XX1"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "TOKEN", &v);
	FREE(v.string_);

	memset(&v, 0, sizeof(struct varcont_t));
	v.string_ = STRDUP("http://127.0.0.1:10080/v2/pushes"); v.type_ = JSON_STRING;
	args = event_action_add_argument(args, "URL", &v);
	FREE(v.string_);

	return args;
}

static void test_event_actions_pushbullet_run(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	plua_init();
	plua_override_global("error", plua_error);

	test_set_plua_path(tc, __FILE__, "event_actions_pushbullet.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_pushbullet.json", CONFIG_SETTINGS | CONFIG_DEVICES));
	event_action_init();

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	http_start(10080);

	struct event_action_args_t *args = initialize_vars();
	CuAssertIntEquals(tc, 0, event_action_check_arguments("pushbullet", args));

	args = initialize_vars();
	CuAssertIntEquals(tc, 0, event_action_run("pushbullet", args));

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

	CuAssertIntEquals(tc, 1, steps);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_pushbullet(void) {
	CuSuite *suite = CuSuiteNew();

	char config[1024] = "{\"devices\":{}," \
		"\"gui\":{},\"rules\":{},\"settings\":{"\
		"\"actions-root\":\"%s../libs/pilight/events/actions/\"," \
		"\"operators-root\":\"%s../libs/pilight/events/operators/\"," \
		"\"functions-root\":\"%s../libs/pilight/events/functions/\"" \
		"},\"hardware\":{},\"registry\":{}}";

	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("event_actions_pushbullet.c", "", &file);

	FILE *f = fopen("event_actions_pushbullet.json", "w");
	fprintf(f, config, file, file, file);
	fclose(f);
	FREE(file);

	SUITE_ADD_TEST(suite, test_event_actions_pushbullet_get_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_pushbullet_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_pushbullet_run);

	return suite;
}
