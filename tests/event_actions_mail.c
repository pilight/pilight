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
#include "../libs/pilight/events/actions/sendmail.h"
#include "../libs/pilight/protocols/generic/generic_switch.h"
#include "../libs/pilight/protocols/generic/generic_label.h"

#include "alltests.h"

static uv_thread_t pth;
static int steps = 0;
static int nrsteps = 0;
static CuTest *gtc = NULL;
static int mail_server = 0;
static int mail_client = 0;
static int mail_loop = 1;
static int doquit = 0;
static int check = 0;

static struct rules_actions_t *obj = NULL;

static struct tests_t {
	char *desc;
	int port;
	int ssl;
	int status;
	int recvmsgnr;
	int sendmsgnr;
	char sendmsg[12][255];
	char recvmsg[12][255];
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

		CuAssertIntEquals(tc, -1, action_sendmail->checkArguments(obj));

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
		OUT_OF_MEMORY
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

static void mail_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	char *message = NULL;
	int n = 0, r = 0, len = 0;
	int doread = 0, dowrite = 0;
	fd_set fdsread;
	fd_set fdswrite;

	if((message = MALLOC(BUFFER_SIZE)) == NULL) {
		OUT_OF_MEMORY
	}

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(mail_server, FIONBIO, &on);
	CuAssertTrue(gtc, r >= 0);
#else
	long arg = fcntl(mail_server, F_GETFL, NULL);
	r = fcntl(mail_server, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, r >= 0);
#endif

	FD_ZERO(&fdsread);
	FD_ZERO(&fdswrite);
	FD_SET((unsigned long)mail_server, &fdsread);

	while(mail_loop) {
		tv.tv_sec = 0;
		tv.tv_usec = 250000;

		if(doquit == 2) {
			mail_loop = 0;
#ifdef _WIN32
			closesocket(mail_client);
			closesocket(mail_server);
#else
			close(mail_client);
			close(mail_server);
#endif
			mail_client = 0;
			mail_server = 0;
			break;
		}

		dowrite = 0;
		doread = 0;
		do {
			if(mail_client > mail_server) {
				n = select(mail_client+1, &fdsread, &fdswrite, NULL, &tv);
			} else {
				n = select(mail_server+1, &fdsread, &fdswrite, NULL, &tv);
			}
		} while(n == -1 && errno == EINTR && mail_loop);

		if(n == 0) {
			continue;
		}
		if(mail_loop == 0) {
			break;
		}

		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(FD_ISSET(mail_server, &fdsread)) {
				FD_ZERO(&fdsread);
				if(mail_client == 0) {
					mail_client = accept(mail_server, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
					CuAssertTrue(gtc, mail_client > 0);

					memset(message, '\0', INET_ADDRSTRLEN+1);
					inet_ntop(AF_INET, (void *)&(addr.sin_addr), message, INET_ADDRSTRLEN+1);
					dowrite = 1;
				}
			}
			if(FD_ISSET(mail_client, &fdsread)) {
				FD_ZERO(&fdsread);
				memset(message, '\0', BUFFER_SIZE);
				r = recv(mail_client, message, BUFFER_SIZE, 0);
				CuAssertTrue(gtc, r >= 0);
				CuAssertStrEquals(gtc, tests.recvmsg[tests.recvmsgnr++], message);
				if(strcmp(message, "QUIT\r\n") == 0) {
					doquit = 1;
				}
				steps++;
				dowrite = 1;
			}
			if(FD_ISSET(mail_client, &fdswrite)) {
				FD_ZERO(&fdswrite);
				strcpy(message, tests.sendmsg[tests.sendmsgnr]);
				len = strlen(message);
				r = send(mail_client, message, len, 0);
				if(doquit == 1) {
					doquit = 2;
				}
				CuAssertIntEquals(gtc, len, r);
				tests.sendmsgnr++;
				doread = 1;
				steps++;
			}
		}

		if(dowrite == 1) {
			FD_SET((unsigned long)mail_client, &fdswrite);
		}
		if(doread == 1) {
			FD_SET((unsigned long)mail_client, &fdsread);
		}
	}

clear:
	if(n != -1) {
		check = 1;
	}
	uv_stop(uv_default_loop());
	FREE(message);
	return;
}

static void mail_start(int port) {
	struct sockaddr_in addr;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	mail_server = socket(AF_INET, SOCK_STREAM, 0);
	CuAssertTrue(gtc, mail_server > 0);

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	r = setsockopt(mail_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, r >= 0);

	r = bind(mail_server, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, r >= 0);

	r = listen(mail_server, 0);
	CuAssertTrue(gtc, r >= 0);
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
	eventpool_callback(REASON_CONTROL_DEVICE, control_device);

	mail_start(10025);
	uv_thread_create(&pth, mail_wait, NULL);

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
