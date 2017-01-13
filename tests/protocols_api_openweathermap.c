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
#include <math.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/json.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/API/openweathermap.h"

#include "alltests.h"

#define BUFSIZE 1024*1024

#define UPDATE		1
#define SUNRISE		2
#define SUNSET		3
#define MIDNIGHT	4

static char add[1024];
static char adapt[1024];
static CuTest *gtc = NULL;
static int steps = 0;
static int run = UPDATE;
static uv_thread_t pth;

static int http_client = 0;
static int http_server = 0;
static int http_loop = 1;
static int doquit = 0;
static int is_ssl = 0;
static int is_ssl_init = 0;
static mbedtls_ssl_context ssl_ctx;

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

struct timestamp_t timestamp;

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
		tv.tv_sec = 0;
		tv.tv_usec = 1000;

		if(doquit == 2) {
			http_loop = 0;
#ifdef _WIN32
			if(http_client > 0) {
				closesocket(http_client);
			}
			closesocket(http_server);
#else
			if(http_client > 0) {
				close(http_client);
			}
			close(http_server);
#endif
			http_client = 0;
			http_server = 0;

			protocol_gc();
			eventpool_gc();
			uv_stop(uv_default_loop());
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
		
		if(http_loop == 0) {
			doquit = 2;
			http_loop = 1;
		}

		if(n == 0 || doquit == 2) {
			continue;
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

				if((message = MALLOC(BUFSIZE)) == NULL) {
					OUT_OF_MEMORY
				}
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
				CuAssertStrEquals(gtc, "GET /data/2.5/weather?q=amsterdam,nl&APPID=8db24c4ac56251371c7ea87fd3115493 HTTP/1.1\r\nHost: 127.0.0.1\r\nUser-Agent: pilight\r\nConnection: close\r\n\r\n", message);
				FREE(message);
				dowrite = 1;
			}
			if(FD_ISSET(http_client, &fdswrite)) {
				FD_ZERO(&fdswrite);
				if((message = MALLOC(BUFSIZE)) == NULL) {
					OUT_OF_MEMORY
				}
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

				if(is_ssl == 1) {
					r = mbedtls_ssl_write(&ssl_ctx, (unsigned char *)message, len);
				} else {
					r = send(http_client, message, len, 0);
				}
				CuAssertIntEquals(gtc, len, r);
				doread = 1;

#ifdef _WIN32
				closesocket(http_client);
#else
				close(http_client);
#endif
				http_client = 0;
				FREE(message);
			}
		}
next:
		if(http_client > 0) {
			if(dowrite == 1) {
				FD_SET((unsigned long)http_client, &fdswrite);
			}
			if(doread == 1) {
				FD_SET((unsigned long)http_client, &fdsread);
			}
		}
	}

clear:
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

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	int duration = (int)((int)timestamp.second-(int)timestamp.first);
	duration = (int)round((double)duration/1000000);

	switch(run) {
		case UPDATE:
			switch(steps) {
				case 0: {
					CuAssertIntEquals(gtc, 1, duration);
					char message[1024];
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"update\":1}",
						data->message);
					steps = 1;
					CuAssertPtrNotNull(gtc, openweathermap->createCode);
					struct JsonNode *code = json_decode("{\"location\":\"amsterdam\",\"country\":\"nl\",\"update\":1}");
					openweathermap->createCode(code, message);
					json_delete(code);
					printf("[ %-48s ]\n", "- waiting for openweathermap notification");
					fflush(stdout);
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 0, duration);
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":7.48,\"sunset\":15.44,\"sun\":set}",
						data->message);
					doquit = 2;
				} break;
			}
		break;
		case SUNRISE:
			switch(steps) {
				case 0: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * Ignore the first update message.
					 */
					steps = 1;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 2, duration);
					/*
					 * After two seconds the first message should be received
					 */
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":7.48,\"sunset\":15.44,\"sun\":set}",
						data->message);
					/*
					 * Shift time to after sunrise and check if we get a
					 * sunrise from openweathermap as well.
					 */
					strcpy(adapt, "{\"openweathermap\":{\"time-override\":1483688926}}");
					eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
					steps = 2;
					printf("[ %-48s ]\n", "- waiting for openweathermap sunrise");
					fflush(stdout);	
				} break;
				case 2: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * After one second the sunrise notification should be sent
					 */
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":7.48,\"sunset\":15.44,\"sun\":rise}",
						data->message);
					doquit = 2;
				} break;
			}
		break;
		case SUNSET:
			switch(steps) {
				case 0: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * Ignore the first update message.
					 */
					steps = 1;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 2, duration);
					/*
					 * After two seconds the first message should be received
					 */
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":7.48,\"sunset\":15.44,\"sun\":rise}",
						data->message);
					/*
					 * Shift time to after sunset and check if we get a
					 * sunset from openweathermap as well.
					 */
					strcpy(adapt, "{\"openweathermap\":{\"time-override\":1483717491}}");
					eventpool_trigger(REASON_DEVICE_ADAPT, done, json_decode(adapt));
					steps = 2;
					printf("[ %-48s ]\n", "- waiting for openweathermap sunset");
					fflush(stdout);	
				} break;
				case 2: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * After one second the sunrise notification should be sent
					 */
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":7.48,\"sunset\":15.44,\"sun\":set}",
						data->message);
					doquit = 2;
				} break;
			}
		break;
		case MIDNIGHT:
			switch(steps) {
				case 0: {
					/*
					 * Ignore the first update message.
					 */
					steps = 1;
					CuAssertIntEquals(gtc, 1, duration);
				} break;
				case 1: {
					CuAssertIntEquals(gtc, 2, duration);
					/*
					 * Directly after, the first message should be received
					 */
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":7.48,\"sunset\":15.44,\"sun\":set}",
						data->message);

					steps = 2;
					printf("[ %-48s ]\n", "- waiting for openweathermap midnight");
					fflush(stdout);	
				} break;
				case 2: {
					CuAssertIntEquals(gtc, 1, duration);
					/*
					 * After one second the sunrise notification should be sent
					 */
					CuAssertStrEquals(gtc,
						"{\"location\":\"amsterdam\",\"country\":\"nl\",\"temperature\":-1.00,\"humidity\":74.00,\"update\":0,\"sunrise\":7.48,\"sunset\":15.44,\"sun\":set}",
						data->message);
					doquit = 2;
				} break;
			}
		break;
	}
	
	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_protocols_api_openweathermap(CuTest *tc) {
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;	
	
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

	uv_thread_join(&pth);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_api_openweathermap_update(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();	

	steps = 0;
	http_loop = 1;
	doquit = 0;
	
	openweathermapInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	http_start(10080);

	uv_thread_create(&pth, http_wait, NULL);
	
	strcpy(adapt, "{\"openweathermap\":{\"url\":\"http://127.0.0.1:10080/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493\"}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"openweathermap\"],\"id\":[{\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	run = UPDATE;
	
	printf("[ %-48s ]\n", "- waiting for update message");
	fflush(stdout);

	test_protocols_api_openweathermap(tc);
}

static void test_protocols_api_openweathermap_sunrise(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	steps = 0;
	http_loop = 1;
	doquit = 0;

	openweathermapInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	http_start(10080);

	uv_thread_create(&pth, http_wait, NULL);
	
	strcpy(adapt, "{\"openweathermap\":{\"time-override\":1483688921,\"url\":\"http://127.0.0.1:10080/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493\"}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"openweathermap\"],\"id\":[{\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	printf("[ %-48s ]\n", "- waiting for first openweathermap notification");
	fflush(stdout);	
	
	run = SUNRISE;

	test_protocols_api_openweathermap(tc);
}

static void test_protocols_api_openweathermap_sunset(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	steps = 0;
	http_loop = 1;
	doquit = 0;

	openweathermapInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	http_start(10080);

	uv_thread_create(&pth, http_wait, NULL);
	
	strcpy(adapt, "{\"openweathermap\":{\"time-override\":1483717489,\"url\":\"http://127.0.0.1:10080/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493\"}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"openweathermap\"],\"id\":[{\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	printf("[ %-48s ]\n", "- waiting for first openweathermap notification");
	fflush(stdout);	

	run = SUNSET;

	test_protocols_api_openweathermap(tc);
}

static void test_protocols_api_openweathermap_midnight(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	steps = 0;
	http_loop = 1;
	doquit = 0;

	openweathermapInit();

	memset(&add, '\0', 1024);
	memset(&adapt, '\0', 1024);

	http_start(10080);

	uv_thread_create(&pth, http_wait, NULL);
	
	strcpy(adapt, "{\"openweathermap\":{\"time-override\":1483833599,\"url\":\"http://127.0.0.1:10080/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493\"}}");
	strcpy(add, "{\"test\":{\"protocol\":[\"openweathermap\"],\"id\":[{\"country\":\"nl\",\"location\":\"amsterdam\"}],\"humidity\":94.00,\"temperature\":0.21,\"sunrise\":8.29,\"sunset\":17.05,\"sun\":\"set\",\"update\":0,\"poll-interval\":1}}");

	printf("[ %-48s ]\n", "- waiting for first openweathermap notification");
	fflush(stdout);	

	run = MIDNIGHT;

	test_protocols_api_openweathermap(tc);
}

CuSuite *suite_protocols_api_openweathermap(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_api_openweathermap_update);
	SUITE_ADD_TEST(suite, test_protocols_api_openweathermap_sunrise);
	SUITE_ADD_TEST(suite, test_protocols_api_openweathermap_sunset);
	SUITE_ADD_TEST(suite, test_protocols_api_openweathermap_midnight);

	return suite;
}