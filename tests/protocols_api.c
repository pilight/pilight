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

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/json.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/API/cpu_temp.h"
#include "../libs/pilight/protocols/API/datetime.h"
#include "../libs/pilight/protocols/API/openweathermap.h"

#include "alltests.h"

#define BUFSIZE 1024*1024

#define CPU_TEMP 				0
#define DATETIME				1
#define OPENWEATHERMAP	2

static char add[1024];
static char adapt[1024];
static CuTest *gtc = NULL;
static int device = CPU_TEMP;
static int steps = 0;
static uv_thread_t pth;

static int http_client = 0;
static int http_server = 0;
static int http_loop = 1;
static int doquit = 0;
static int is_ssl = 0;
static int is_ssl_init = 0;
static mbedtls_ssl_context ssl_ctx;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void http_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	char *message = NULL;
	int n = 0, r = 0, len = 0;
	int doread = 0, dowrite = 0;
	fd_set fdsread;
	fd_set fdswrite;
	
	if((message = MALLOC(BUFSIZE)) == NULL) {
		OUT_OF_MEMORY
	}
#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(http_server, FIONBIO, &on);
	CuAssertTrue(gtc, r >= 0);
#else
	long arg = fcntl(http_server, F_GETFL, NULL);
	r = fcntl(http_server, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, r >= 0);
#endif

	FD_ZERO(&fdsread);
	FD_ZERO(&fdswrite);

	while(http_loop) {
		FD_SET((unsigned long)http_server, &fdsread);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if(doquit == 2) {
			http_loop = 0;
#ifdef _WIN32
			closesocket(http_client);
			closesocket(http_server);
#else
			close(http_client);
			close(http_server);
#endif
			http_client = 0;
			http_server = 0;
			break;
		}

		dowrite = 0;
		doread = 0;
		do {
			if(http_client > http_server) {
				n = select(http_client+1, &fdsread, &fdswrite, NULL, &tv);
			} else {
				n = select(http_server+1, &fdsread, &fdswrite, NULL, &tv);
			}
		} while(n == -1 && errno == EINTR && http_loop);

		if(n == 0) {
			continue;
		}
		if(http_loop == 0) {
			break;
		}

		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(is_ssl == 1 && is_ssl_init == 0) {
				is_ssl_init = 1;
				r = mbedtls_ssl_setup(&ssl_ctx, &ssl_server_conf);
				CuAssertTrue(gtc, r >= 0);

				r = mbedtls_ssl_session_reset(&ssl_ctx);
				CuAssertTrue(gtc, r >= 0);

				mbedtls_ssl_set_bio(&ssl_ctx, &http_client, mbedtls_net_send, mbedtls_net_recv, NULL);
			}
			if(FD_ISSET(http_server, &fdsread)) {
				FD_ZERO(&fdsread);
				if(http_client == 0) {
					http_client = accept(http_server, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
					CuAssertTrue(gtc, http_client > 0);
					doread = 1;
				}
			}
			if(FD_ISSET(http_client, &fdsread)) {
				FD_ZERO(&fdsread);
				memset(message, '\0', BUFSIZE);
				if(is_ssl == 1) {
					r = mbedtls_ssl_read(&ssl_ctx, (unsigned char *)message, BUFSIZE);
					if(r == MBEDTLS_ERR_SSL_WANT_READ) {
						doread = 1;
						goto next;
					}
				} else {
					r = recv(http_client, message, BUFSIZE, 0);
				}
				CuAssertTrue(gtc, r >= 0);
				if(device == OPENWEATHERMAP) {
					CuAssertStrEquals(gtc, "GET /data/2.5/weather?q=amsterdam,nl&APPID=8db24c4ac56251371c7ea87fd3115493 HTTP/1.1\r\nHost: 127.0.0.1\r\nUser-Agent: pilight\r\nConnection: close\r\n\r\n", message);
				}

				dowrite = 1;
			}
			if(FD_ISSET(http_client, &fdswrite)) {
				FD_ZERO(&fdswrite);
				if(device == OPENWEATHERMAP) {
					/*
					 * Actual raw openweathermap http response
					 */
					strcpy(message, 
						"HTTP/1.1 200 OK\r\n"
						"Server: pilight\r\n"
						"Date: Fri, 06 Jan 2017 15:22:49 GMT\r\n"
						"Content-Type: application/json; charset=utf-8\n\r"
						"Content-Length: 445\r\n"
						"Connection: close\r\n"
						"X-Cache-Key: /data/2.5/weather?APPID=8db24c4ac56251371c7ea87fd3115493&q=amsterdam,nl\r\n"
						"Access-Control-Allow-Origin: *\r\n"
						"Access-Control-Allow-Credentials: true\r\n"
						"Access-Control-Allow-Methods: GET, POST\r\n\r\n"
						"{\"coord\":{\"lon\":4.89,\"lat\":52.37},\"weather\":[{\"id\":800,\"main\":\"Clear\",\"description\":\"clear sky\",\"icon\":\"01d\"}],\"base\":\"stations\",\"main\":{\"temp\":272.15,\"pressure\":1038,\"humidity\":74,\"temp_min\":271.15,\"temp_max\":273.15},\"visibility\":10000,\"wind\":{\"speed\":4.1,\"deg\":180},\"clouds\":{\"all\":0},\"dt\":1483714500,\"sys\":{\"type\":1,\"id\":5204,\"message\":0.0527,\"country\":\"NL\",\"sunrise\":1483688925,\"sunset\":1483717490},\"id\":2759794,\"name\":\"Amsterdam\",\"cod\":200}");
					len = strlen(message);
				}

				if(is_ssl == 1) {
					r = mbedtls_ssl_write(&ssl_ctx, (unsigned char *)message, len);
				} else {
					r = send(http_client, message, len, 0);
				}
				CuAssertIntEquals(gtc, len, r);
				doquit = 2;
			}
		}
next:
		if(dowrite == 1) {
			FD_SET((unsigned long)http_client, &fdswrite);
		}
		if(doread == 1) {
			FD_SET((unsigned long)http_client, &fdsread);
		}
	}

clear:
	FREE(message);
	return;
}

static void http_start(int port) {
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

	http_server = socket(AF_INET, SOCK_STREAM, 0);
	CuAssertTrue(gtc, http_server >= 0);

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	r = setsockopt(http_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, r >= 0);

	r = bind(http_server, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, r >= 0);

	r = listen(http_server, 0);
	CuAssertTrue(gtc, r >= 0);
}

static void *received(int reason, void *param) {
	struct reason_code_received_t *data = param;

	switch(device) {
		case CPU_TEMP:
			CuAssertStrEquals(gtc, "{\"id\":1,\"temperature\":46.540}", data->message);
		break;
		case DATETIME:
			CuAssertStrEquals(gtc,
				"{\"longitude\":4.895168,\"latitude\":52.370216,\"year\":2017,\"month\":1,\"day\":6,\"weekday\":6,\"hour\":13,\"minute\":15,\"second\":5,\"dst\":0}",
				data->message);
		break;
		case OPENWEATHERMAP:
			switch(steps) {
				case 0: {
					char message[1024];
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"update\":1}",
						data->message);
					steps = 1;
					CuAssertPtrNotNull(gtc, openweathermap->createCode);
					struct JsonNode *code = json_decode("{\"location\":\"amsterdam\",\"country\":\"nl\",\"update\":1}");
					openweathermap->createCode(code, message);
					json_delete(code);
				} break;
				case 1: {
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":16.44,\"sunrise\":16.44,\"sun\":set}",
						data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		break;
	}

	if(device != OPENWEATHERMAP) {
		uv_stop(uv_default_loop());
	}

	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_protocols_api(CuTest *tc) {
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	eventpool_init(EVENTPOOL_NO_THREADS);

	if(strlen(adapt) > 0) {
		eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
	}
	if(strlen(add) > 0) {
		eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	}
	eventpool_callback(REASON_CODE_RECEIVED, received);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	protocol_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_api_cpu_temp(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();	

	cpuTempInit();

	FILE *f = fopen("cpu_temp.txt", "w");
	CuAssertPtrNotNull(tc, f);
	fprintf(f, "%d", 46540);
	fclose(f);	

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(add, "{\"test\":{\"protocol\":[\"cpu_temp\"],\"id\":[{\"id\":1}],\"temperature\":0,\"poll-interval\":1}}");
	strcpy(adapt, "{\"cpu_temp\":{\"path\":\"cpu_temp.txt\"}}");

	device = CPU_TEMP;

	test_protocols_api(tc);
}

static void test_protocols_api_datetime(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	
	gtc = tc;

	memtrack();	

	datetimeInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	strcpy(add, "{\"test\":{\"protocol\":[\"datetime\"],\"id\":[{\"longitude\":4.895167899999933,\"latitude\":52.3702157}],\"year\":2015,\"month\":1,\"day\":27,\"hour\":14,\"minute\":37,\"second\":8,\"weekday\":3,\"dst\":1}}");
	strcpy(adapt, "{\"datetime\":{\"time_override\":1483704905}}");

	device = DATETIME;

	test_protocols_api(tc);
}

static void test_protocols_api_openweathermap(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();	

	openweathermapInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	http_start(10080);

	uv_thread_create(&pth, http_wait, NULL);
	
	strcpy(adapt, "{\"openweathermap\":{\"url\":\"http://127.0.0.1:10080/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493\"}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"openweathermap\"],\"id\":[{\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	device = OPENWEATHERMAP;

	test_protocols_api(tc);
}

CuSuite *suite_protocols_api(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_api_cpu_temp);
	SUITE_ADD_TEST(suite, test_protocols_api_datetime);
	SUITE_ADD_TEST(suite, test_protocols_api_openweathermap);

	return suite;
}