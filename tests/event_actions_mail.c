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
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/events/action.h"
#include "../libs/pilight/events/function.h"
#include "../libs/pilight/events/actions/sendmail.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static uv_thread_t pth;
static int steps = 0;
static int nrsteps = 0;
static CuTest *gtc = NULL;
// static int doquit = 0;
static int check = 0;

// static uv_tcp_t *client_req = NULL;
static uv_timer_t *timer_req = NULL;

static struct rules_actions_t *obj = NULL;

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
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	genericSwitchInit();
	genericLabelInit();
	actionSendmailInit();
	CuAssertStrEquals(tc, "sendmail", action_sendmail->name);

	FILE *f = fopen("event_actions_mail.json", "w");
	fprintf(f,
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
		"\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"smtp-host\":\"127.0.0.1\",\"smtp-port\":10025,\"smtp-user\":\"pilight\",\"smtp-password\":\"test\",\"smtp-sender\":\"info@pilight.org\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_DEVICES | CONFIG_SETTINGS));

	/*
	 * Valid parameters
	 */
	{
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, 0, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);
	}

	{
		/*
		 * No arguments
		 */
		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(NULL));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		FREE(obj);

		/*
		 * Missing json parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Missing json parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Wrong order of parameters
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Missing arguments for a parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"Hello World!\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Missing arguments for a parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Missing arguments for a parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Too many parameters for argument
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\",\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"Hello World!\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Too many parameters for argument
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[,\"Hello World!\",\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Too many parameters for argument
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\",\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Invalid variable type for parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[1],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		/*
		 * Invalid variable type for parameter
		 */
		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info\"],\"order\":3}\
		}");

/*
 * Due to missing regex capability
 */
#if defined(__FreeBSD__) || defined(_WIN32)
		CuAssertIntEquals(tc, 0, action_sendmail->checkArguments(obj));
#else
		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));
#endif

		json_delete(obj->parsedargs);
		FREE(obj);

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();

		/*
		 * Missing smtp-host setting
		 */
		FILE *f = fopen("event_actions_mail.json", "w");
		fprintf(f,
			"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"smtp-port\":10025,\"smtp-user\":\"pilight\",\"smtp-password\":\"test\",\"smtp-sender\":\"info@pilight.org\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_DEVICES | CONFIG_SETTINGS));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();

		/*
		 * Missing smtp-port setting
		 */
		f = fopen("event_actions_mail.json", "w");
		fprintf(f,
			"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"smtp-host\":\"127.0.0.1\",\"smtp-user\":\"pilight\",\"smtp-password\":\"test\",\"smtp-sender\":\"info@pilight.org\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_DEVICES | CONFIG_SETTINGS));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();

		/*
		 * Missing smtp-user setting
		 */
		f = fopen("event_actions_mail.json", "w");
		fprintf(f,
			"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"smtp-host\":\"127.0.0.1\",\"smtp-port\":10025,\"smtp-password\":\"test\",\"smtp-sender\":\"info@pilight.org\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_DEVICES | CONFIG_SETTINGS));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();

		/*
		 * Missing smtp-user setting
		 */
		f = fopen("event_actions_mail.json", "w");
		fprintf(f,
			"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"smtp-host\":\"127.0.0.1\",\"smtp-port\":10025,\"smtp-user\":\"pilight\",\"smtp-sender\":\"info@pilight.org\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_DEVICES | CONFIG_SETTINGS));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();

		/*
		 * Missing smtp-sender setting
		 */
		f = fopen("event_actions_mail.json", "w");
		fprintf(f,
			"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
			"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
			"\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"smtp-host\":\"127.0.0.1\",\"smtp-port\":10025,\"smtp-password\":\"test\",\"smtp-user\":\"pilight\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_DEVICES | CONFIG_SETTINGS));

		obj = MALLOC(sizeof(struct rules_actions_t));
		CuAssertPtrNotNull(tc, obj);
		memset(obj, 0, sizeof(struct rules_actions_t));

		obj->parsedargs = json_decode("{\
			\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
			\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
			\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
		}");

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

		json_delete(obj->parsedargs);
		FREE(obj);

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		storage_gc();
		eventpool_gc();
	}

	event_action_gc();
	protocol_gc();

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

	int r = uv_read_start((uv_stream_t *)client_req, alloc, read_cb);
	CuAssertIntEquals(gtc, 0, r);

	FREE(req);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	uv_read_stop(stream);
	buf->base[nread] = '\0';	

	uv_os_fd_t fd = 0;
	int r = uv_fileno((uv_handle_t *)stream, &fd);
	CuAssertIntEquals(gtc, 0, r);

	CuAssertStrEquals(gtc, tests.recvmsg[tests.recvmsgnr++], buf->base);
	if(strcmp(buf->base, "QUIT\r\n") != 0) {
		uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
		CuAssertPtrNotNull(gtc, write_req);
		
		uv_buf_t buf1;
		buf1.base = tests.sendmsg[tests.sendmsgnr];
		buf1.len = strlen(tests.sendmsg[tests.sendmsgnr]);

		tests.sendmsgnr++;
		write_req->data = stream;
		int r = uv_write(write_req, stream, &buf1, 1, write_cb);
		CuAssertIntEquals(gtc, 0, r);
	} else {
		uv_close((uv_handle_t *)stream, close_cb);

		check = 1;
#ifdef _WIN32
	closesocket(fd);
#else
	close(fd);
#endif

		if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY
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
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	steps = 0;
	nrsteps = 2;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	gtc = tc;

	eventpool_init(EVENTPOOL_THREADED);

	mail_start(10025);

	FILE *f = fopen("event_actions_mail.json", "w");
	fprintf(f,
		"{\"devices\":{\"switch\":{\"protocol\":[\"generic_switch\"],\"id\":[{\"id\":100}],\"state\":\"off\"}," \
		"\"label\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":101}],\"label\":\"foo\",\"color\":\"black\"}}," \
		"\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"smtp-host\":\"127.0.0.1\",\"smtp-port\":10025,\"smtp-user\":\"pilight\",\"smtp-password\":\"test\",\"smtp-sender\":\"info@pilight.org\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	genericSwitchInit();
	genericLabelInit();
	actionSendmailInit();
	CuAssertStrEquals(tc, "sendmail", action_sendmail->name);

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("event_actions_mail.json", CONFIG_DEVICES | CONFIG_SETTINGS));

	obj = MALLOC(sizeof(struct rules_actions_t));
	CuAssertPtrNotNull(tc, obj);
	memset(obj, 0, sizeof(struct rules_actions_t));

	obj->parsedargs = json_decode("{\
		\"SUBJECT\":{\"value\":[\"Hello World!\"],\"order\":1},\
		\"MESSAGE\":{\"value\":[\"Hello World!\"],\"order\":2},\
		\"TO\":{\"value\":[\"info@pilight.org\"],\"order\":3}\
	}");

	CuAssertIntEquals(tc, 0, action_sendmail->checkArguments(obj));
	CuAssertIntEquals(tc, 0, action_sendmail->run(obj));

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	json_delete(obj->parsedargs);
	FREE(obj);

	event_action_gc();
	event_function_gc();
	protocol_gc();
	storage_gc();
	eventpool_gc();

	uv_thread_join(&pth);

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_event_actions_mail(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_event_actions_mail_check_parameters);
	SUITE_ADD_TEST(suite, test_event_actions_mail_run);

	return suite;
}
