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
#endif
#include <mbedtls/sha256.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/webserver.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/http.h"
#include "../libs/pilight/core/ssl.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/lua_c/lua.h"

#include "alltests.h"

#define GET					0
#define AUTH 				1
#define WEBSOCKET1	2
#define WEBSOCKET2	3
#define WHITELIST		4

static int run = GET;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static int steps = 0;

static char request[255] = {
	0x81, 0x9b, 0xa0, 0x18, 0xa7,
	0x15, 0xdb, 0x3a, 0xc6, 0x76,
	0xd4, 0x71, 0xc8, 0x7b, 0x82,
	0x22, 0x85, 0x67, 0xc5, 0x69,
	0xd2, 0x70, 0xd3, 0x6c, 0x87,
	0x76, 0xcf, 0x76, 0xc1, 0x7c,
	0xc7, 0x3a, 0xda
};

static char response1[255] = {
	0x81, 0x0d, 0x7b, 0x22,
	0x66, 0x6f, 0x6f, 0x22,
	0x3a, 0x22, 0x62, 0x61,
	0x72, 0x22, 0x7d
};

static char response2[255] = {
	0x81, 0x4c, 0x7b, 0x22, 0x64,
	0x65, 0x76, 0x69, 0x63, 0x65,
	0x73, 0x22, 0x3a, 0x7b, 0x7d,
	0x2c, 0x22, 0x72, 0x75, 0x6c,
	0x65, 0x73, 0x22, 0x3a, 0x7b,
	0x7d, 0x2c, 0x22, 0x67, 0x75,
	0x69, 0x22, 0x3a, 0x7b, 0x7d,
	0x2c, 0x22, 0x73, 0x65, 0x74,
	0x74, 0x69, 0x6e, 0x67, 0x73,
	0x22, 0x3a, 0x7b, 0x7d, 0x2c,
	0x22, 0x68, 0x61, 0x72, 0x64,
	0x77, 0x61, 0x72, 0x65, 0x22,
	0x3a, 0x7b, 0x7d, 0x2c, 0x22,
	0x72, 0x65, 0x67, 0x69, 0x73,
	0x74, 0x72, 0x79, 0x22, 0x3a,
	0x7b, 0x7d, 0x7d
};

struct tests_t {
	char *url;
	int size;
	char *type;
	int code;
} tests_t;

static struct tests_t get_tests[] = {
	/*
	 * Regular pilight webgui loading until config call
	 */
	{ "http://127.0.0.1:10080/", 1674, "text/html", 200 },
	{ "http://127.0.0.1:10080/jquery-2.1.4.min.js", 84345, "text/javascript", 200 },
	{ "http://127.0.0.1:10080/jquery.mobile-1.4.5.min.js", 200144, "text/javascript", 200 },
	{ "http://127.0.0.1:10080/moment.min.js", 35416, "text/javascript", 200 },
	{ "http://127.0.0.1:10080/pilight.js", 48689, "text/javascript", 200 },
	{ "http://127.0.0.1:10080/jquery.mobile-1.4.5.min.css", 207466, "text/css", 200 },
	{ "http://127.0.0.1:10080/pilight.css", 6261, "text/css", 200 },
	{ "http://127.0.0.1:10080/pilight.jquery.theme.css", 25683, "text/css", 200 },
	{ "http://127.0.0.1:10080/images/ajax-loader.gif", 6242, "image/gif", 200 },
	{ "http://127.0.0.1:10080/config", 48, "application/json", 200 },
	/*
	 * Regular pilight ssl webgui loading until config call
	 */
	{ "https://127.0.0.1:10443/", 1674, "text/html", 200 },
	{ "https://127.0.0.1:10443/jquery-2.1.4.min.js", 84345, "text/javascript", 200 },
	{ "https://127.0.0.1:10443/jquery.mobile-1.4.5.min.js", 200144, "text/javascript", 200 },
	{ "https://127.0.0.1:10443/moment.min.js", 35416, "text/javascript", 200 },
	{ "https://127.0.0.1:10443/pilight.js", 48689, "text/javascript", 200 },
	{ "https://127.0.0.1:10443/jquery.mobile-1.4.5.min.css", 207466, "text/css", 200 },
	{ "https://127.0.0.1:10443/pilight.css", 6261, "text/css", 200 },
	{ "https://127.0.0.1:10443/pilight.jquery.theme.css", 25683, "text/css", 200 },
	{ "https://127.0.0.1:10443/images/ajax-loader.gif", 6242, "image/gif", 200 },
	{ "https://127.0.0.1:10443/config", 48, "application/json", 200 },
	{ "https://127.0.0.1:10443/nonexisting", 214, "text/html", 404 }
};

static struct tests_t auth_tests[] = {
	{ "http://test:test@127.0.0.1:10080/", 1674, "text/html", 200 },
	{ "http://test1:test1@127.0.0.1:10080/", 0, "", 401 }
};

static int testnr = 0;
static uv_async_t *async_close_req = NULL;

static void *reason_socket_send_free(void *param) {
	struct reason_socket_send_t *data = param;
	FREE(data->buffer);
	FREE(data);
	return NULL;
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void async_close_cb(uv_async_t *handle) {
	webserver_gc();
	whitelist_free();
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_stop(uv_default_loop());
}

static void callback(int code, char *data, int size, char *type, void *userdata) {
	if(run == GET || run == WHITELIST) {
		int len = sizeof(get_tests)/sizeof(get_tests[0]);
		/*
		 * This code should never be reached, but when it does
		 * the config size is larger than the expected 220 due
		 * to the additional whitelist parameter.
		 */
		if(run == WHITELIST && size == 242) {
			size = 220;
		}
		CuAssertIntEquals(gtc, get_tests[testnr].code, code);
		CuAssertIntEquals(gtc, get_tests[testnr].size, size);
		CuAssertStrEquals(gtc, get_tests[testnr].type, type);

		if(testnr+1 < len) {
			char *file = STRDUP(get_tests[++testnr].url);
			str_replace("127.0.0.1:", "..", &file);
			str_replace("10080", "", &file);
			str_replace("10443", "", &file);
			printf("[ - %-46s ]\n", file);
			fflush(stdout);
			FREE(file);

			http_get_content(get_tests[testnr].url, callback, NULL);
		} else {
			uv_async_send(async_close_req);
		}
	} else if(run == AUTH) {
		int len = sizeof(auth_tests)/sizeof(auth_tests[0]);
		CuAssertIntEquals(gtc, auth_tests[testnr].code, code);
		CuAssertIntEquals(gtc, auth_tests[testnr].size, size);
		CuAssertStrEquals(gtc, auth_tests[testnr].type, type);

		if(testnr+1 < len) {
			char *file = STRDUP(auth_tests[++testnr].url);
			str_replace("127.0.0.1:", "..", &file);
			str_replace("10080", "", &file);
			str_replace("10443", "", &file);
			printf("[ - %-46s ]\n", file);
			fflush(stdout);
			FREE(file);

			http_get_content(auth_tests[testnr].url, callback, NULL);
		} else {
			uv_async_send(async_close_req);
		}
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void poll_close_cb(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	int fd = -1, r = 0;

	if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
	}

	if(fd > -1) {
#ifdef _WIN32
		shutdown(fd, SD_BOTH);
		closesocket(fd);
#else
		shutdown(fd, SHUT_RDWR);
		close(fd);
#endif
	}

	if(!uv_is_closing((uv_handle_t *)req)) {
		uv_close((uv_handle_t *)req, close_cb);
	}

	if(custom_poll_data->data != NULL) {
		FREE(custom_poll_data->data);
	}

	if(req->data != NULL) {
		uv_custom_poll_free(custom_poll_data);
		req->data = NULL;
	}
	uv_async_send(async_close_req);
}

static void read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	struct uv_custom_poll_t *custom_poll_data = req->data;

	if(*nread > 0) {
		buf[*nread] = '\0';
	}

	switch(steps) {
		case 1: {
			CuAssertStrEquals(gtc,
				"HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
				"Connection: Upgrade\r\n"
				"Upgrade: websocket\r\n"
				"Sec-WebSocket-Accept: I0Py0AMLWSZtp8aSeQd+j/A2xMw=\r\n\r\n", buf);

			iobuf_append(&custom_poll_data->send_iobuf, request, 33);
			uv_custom_write(req);
			uv_custom_read(req);

			*nread = 0;
			steps = 2;
		} break;
		case 2: {
			if(run == WEBSOCKET1) {
				CuAssertTrue(gtc, strncmp(buf, response1, 15) == 0);
			}
			if(run == WEBSOCKET2) {
				CuAssertTrue(gtc, strncmp(buf, response2, 78) == 0);
			}

			uv_custom_close(req);
		} break;
	}
}

static void write_cb(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	char buffer[1024];
	int len = 0;

	switch(steps) {
		case 0: {
			len = sprintf(buffer, "GET /websocket HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"User-Agent: pilight\r\n"
				"Sec-WebSocket-Version: 13\r\n"
				"Origin: http://127.0.0.1\r\n"
				"Sec-WebSocket-Extensions: permessage-deflate\r\n"
				"Sec-WebSocket-Key: 94enVvHDRqelYdOoZ41ZvA==\r\n"
				"Connection: close\r\n"
				"Upgrade: websocket\r\n\r\n");
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buffer, len);

			uv_custom_write(req);
			steps = 1;
		} break;
		case 1: {
			uv_custom_read(req);
			return;
		} break;
	}
}

static void stop(uv_timer_t *req) {
	uv_async_send(async_close_req);
}

static void *test(void) {
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct sockaddr_in addr;
	int r = 0, sockfd = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	r = uv_ip4_addr("127.0.0.1", 10080, &addr);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
		goto free;
	}

	/*
	 * Partly bypass libuv in case of ssl connections
	 */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		logprintf(LOG_ERR, "socket: %s", strerror(errno));
		goto free;
	}

#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(sockfd, FIONBIO, &on);
#else
	long arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg | O_NONBLOCK);
#endif

	if(connect(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
#ifdef _WIN32
		if(!(WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEISCONN)) {
#else
		if(!(errno == EINPROGRESS || errno == EISCONN)) {
#endif
			logprintf(LOG_ERR, "connect: %s", strerror(errno));
			goto free;
		}
	}

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_custom_poll_init(&custom_poll_data, poll_req, NULL);
	custom_poll_data->is_ssl = 0;
	custom_poll_data->write_cb = write_cb;
	custom_poll_data->read_cb = read_cb;
	custom_poll_data->close_cb = poll_close_cb;

	r = uv_poll_init_socket(uv_default_loop(), poll_req, sockfd);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		FREE(poll_req);
		goto free;
	}

	uv_custom_write(poll_req);

	return NULL;

free:
	if(sockfd > 0) {
#ifdef _WIN32
		closesocket(sockfd);
#else
		close(sockfd);
#endif
	}
	return NULL;
}

static void *reason_broadcast_core_free(void *param) {
	char *code = param;
	FREE(code);
	return NULL;
}

static void *listener(int reason, void *param, void *userdata) {
	if(run == WEBSOCKET1) {
		char *respons = "{\"foo\":\"bar\"}";
		struct reason_socket_received_t *data = param;
		struct reason_socket_send_t *data1 = MALLOC(sizeof(struct reason_socket_send_t));
		if(data1 == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		data1->fd = data->fd;
		if((data1->buffer = MALLOC(strlen(respons)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/;
		}
		strcpy(data1->buffer, respons);
		strncpy(data1->type, data->type, 255);

		eventpool_trigger(REASON_SOCKET_SEND, reason_socket_send_free, data1);
	}
	if(run == WEBSOCKET2) {
		char *conf = STRDUP("{\"devices\":{},\"rules\":{},\"gui\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}");
		eventpool_trigger(REASON_BROADCAST_CORE, reason_broadcast_core_free, conf);
	}

	return NULL;
}


static void test_webserver(CuTest *tc) {
	memtrack();

	testnr = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	plua_init();

	test_set_plua_path(tc, __FILE__, "webserver.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("webserver.json", CONFIG_SETTINGS));

	ssl_init();

	if(run == GET || run == WHITELIST || run == AUTH) {
		eventpool_init(EVENTPOOL_NO_THREADS);
	} else if(run == WEBSOCKET1 || run == WEBSOCKET2) {
		eventpool_init(EVENTPOOL_THREADED);
		eventpool_callback(REASON_SOCKET_RECEIVED, listener, NULL);
	}
	webserver_start();

	if(run == GET || run == WHITELIST) {
		char *file = STRDUP(get_tests[testnr].url);
		CuAssertPtrNotNull(tc, file);
		str_replace("127.0.0.1:", "..", &file);
		str_replace("10080", "", &file);
		str_replace("10443", "", &file);
		printf("[ - %-46s ]\n", file);
		fflush(stdout);
		FREE(file);

		http_get_content(get_tests[testnr].url, callback, NULL);
	} else if(run == AUTH) {
		char *file = STRDUP(auth_tests[testnr].url);
		CuAssertPtrNotNull(tc, file);
		str_replace("127.0.0.1:", "..", &file);
		str_replace("10080", "", &file);
		str_replace("10443", "", &file);
		printf("[ - %-46s ]\n", file);
		fflush(stdout);
		FREE(file);

		http_get_content(auth_tests[testnr].url, callback, NULL);
	} else if(run == WEBSOCKET1 || run == WEBSOCKET2) {
		test();
	}

	if(run == WHITELIST) {
		timer_req = MALLOC(sizeof(uv_timer_t));
		CuAssertPtrNotNull(tc, timer_req);
		uv_timer_init(uv_default_loop(), timer_req);
		uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);
	}

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	if(run == GET || run == WHITELIST || run == AUTH) {
		http_gc();
	}
	ssl_gc();
	storage_gc();
	plua_gc();
	eventpool_gc();

	switch(run) {
		case WHITELIST:
			CuAssertIntEquals(tc, 0, testnr);
		break;
		case GET:
			CuAssertIntEquals(tc, 20, testnr);
		break;
	}
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_webserver_get(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\",\"webserver-root\":\"%s../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443},"\
		"\"hardware\":{},\"registry\":{}}";

	char *file = strdup(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("webserver.c", "", &file);

	FILE *f = fopen("webserver.json", "w");
	fprintf(f, config, file, file);
	fclose(f);
	strdup(file);

	gtc = tc;
	steps = 0;

	run = GET;
	test_webserver(tc);
}

static void test_webserver_whitelist(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\",\"webserver-root\":\"%s../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443,\"whitelist\":\"0.0.0.0\"},"\
		"\"hardware\":{},\"registry\":{}}";

	char *file = strdup(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("webserver.c", "", &file);

	FILE *f = fopen("webserver.json", "w");
	fprintf(f, config, file, file);
	fclose(f);
	free(file);

	gtc = tc;
	steps = 0;

	run = WHITELIST;
	test_webserver(tc);
}

static void test_webserver_auth(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\",\"webserver-root\":\"%s../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443,"\
		"\"webserver-authentication\":[\"test\",\"5cc87eb5ab20e91ba99d213869f431a7aa701fc0cd548caccb076eb4712d4962\"]},"\
		"\"hardware\":{},\"registry\":{}}";

	char *file = strdup(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("webserver.c", "", &file);

	FILE *f = fopen("webserver.json", "w");
	fprintf(f, config, file, file);
	fclose(f);
	free(file);

	gtc = tc;
	steps = 0;

	run = AUTH;
	test_webserver(tc);
}

static void test_webserver_websocket1(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\",\"webserver-root\":\"%s../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443},"\
		"\"hardware\":{},\"registry\":{}}";

	char *file = strdup(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("webserver.c", "", &file);

	FILE *f = fopen("webserver.json", "w");
	fprintf(f, config, file, file);
	fclose(f);
	free(file);

	gtc = tc;
	steps = 0;

	run = WEBSOCKET1;
	test_webserver(tc);
}

static void test_webserver_websocket2(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\",\"webserver-root\":\"%s../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443},"\
		"\"hardware\":{},\"registry\":{}}";

	char *file = strdup(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("webserver.c", "", &file);

	FILE *f = fopen("webserver.json", "w");
	fprintf(f, config, file, file);
	fclose(f);
	strdup(file);

	gtc = tc;
	steps = 0;

	run = WEBSOCKET2;
	test_webserver(tc);
}

CuSuite *suite_webserver(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_webserver_get);
	SUITE_ADD_TEST(suite, test_webserver_whitelist);
	SUITE_ADD_TEST(suite, test_webserver_auth);
	SUITE_ADD_TEST(suite, test_webserver_websocket1);
	SUITE_ADD_TEST(suite, test_webserver_websocket2);

	return suite;
}
