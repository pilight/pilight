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

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/http.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/webserver.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/ssl.h"

#include "alltests.h"

#include "gplv3.h"

#define GET		0
#define	POST	1
#define BUFSIZE 1024*1024

/*
 * FIXME
 * - 300 urls
 */

static struct tests_t {
	char *desc;
	int getpost;
	char *url;
	int port;
	int ssl;
	int sprintf;
	int code;
	int size;
	char *type;
	char *data;
	char *post;
	char *recvmsg;
	char *sendmsg;
} tests[] = {
	{ "plain get", GET, "http://127.0.0.1:10080/", 10080, 0, 1, 200, 12, "text/html", "Hello World!", "",
			"GET / HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"User-Agent: pilight\r\n"
				"Connection: close\r\n\r\n",
			"HTTP/1.1 200 OK\r\n"
				"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
				"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
				"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
				"ETag: \"d5-4e955b12f4640\"\r\n"
				"Accept-Ranges: bytes\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"%s"
	},
	{ "plain get ssl", GET, "https://127.0.0.1:10443/", 10443, 1, 1, 200, 12, "text/html", "Hello World!", "",
			"GET / HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"User-Agent: pilight\r\n"
				"Connection: close\r\n\r\n",
			"HTTP/1.1 200 OK\r\n"
				"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
				"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
				"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
				"ETag: \"d5-4e955b12f4640\"\r\n"
				"Accept-Ranges: bytes\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"%s"
	},
	{ "json get", GET, "http://127.0.0.1:10080/data/2.5/weather?q=amsterdam,netherlands&APPID=8db24c4ac56251371c7ea87fd3115493/", 10080, 0, 1, 200, 462, "application/json", "{\"coord\":{\"lon\":4.89,\"lat\":52.37},\"weather\":[{\"id\":801,\"main\":\"Clouds\",\"description\":\"few clouds\",\"icon\":\"02n\"}],\"base\":\"stations\",\"main\":{\"temp\":282.924,\"pressure\":1043.96,\"humidity\":93,\"temp_min\":282.924,\"temp_max\":282.924,\"sea_level\":1043.89,\"grnd_level\":1043.96},\"wind\":{\"speed\":1.31,\"deg\":78.5027},\"clouds\":{\"all\":12},\"dt\":1477846217,\"sys\":{\"message\":0.0058,\"country\":\"NL\",\"sunrise\":1477809271,\"sunset\":1477843964},\"id\":2759794,\"name\":\"Amsterdam\",\"cod\":200}", "",
			"GET /data/2.5/weather?q=amsterdam,netherlands&APPID=8db24c4ac56251371c7ea87fd3115493/ HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"User-Agent: pilight\r\n"
				"Connection: close\r\n\r\n",
			"HTTP/1.1 200 OK\r\n"
				"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
				"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
				"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
				"ETag: \"d5-4e955b12f4640\"\r\n"
				"Accept-Ranges: bytes\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"%s"
	},
	{ "big file get", GET, "http://127.0.0.1:10080/LICENSE.txt", 10080, 0, 1, 200, 35146, "text/plain", gplv3, "",
			"GET /LICENSE.txt HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"User-Agent: pilight\r\n"
				"Connection: close\r\n\r\n",
			"HTTP/1.1 200 OK\r\n"
				"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
				"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
				"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
				"ETag: \"d5-4e955b12f4640\"\r\n"
				"Accept-Ranges: bytes\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"%s"
	},
	{ "chunked get", GET, "http://127.0.0.1:10080/test.jpg", 10080, 0, 0, 200, 30, NULL, "123456789012345678901234567890", "",
		"GET /test.jpg HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Connection: close\r\n\r\n",
		"HTTP/1.1 200 OK\r\n"
			"Keep-Alive: timeout=15, max=100\r\n"
			"Transfer-Encoding: chunked\r\n\r\n"
			"1E\r\n"
			"123456789012345678901234567890\r\n"
			"0\r\n\r\n"
	},
	{ "multiple chunked get", GET, "http://127.0.0.1:10080/test.jpg", 10080, 0, 0, 200, 49, NULL, "1234567890123456789012345678901234567890123456789", "",
		"GET /test.jpg HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Connection: close\r\n\r\n",
		"HTTP/1.1 200 OK\r\n"
			"Keep-Alive: timeout=15, max=100\r\n"
			"Transfer-Encoding: chunked\r\n\r\n"
			"1E\r\n"
			"123456789012345678901234567890\r\n"
			"13\r\n"
			"1234567890123456789\r\n"
			"0\r\n\r\n"
	},
	{ "404 header, ssl, and lower case headers", GET, "https://127.0.0.1:10443/foobar", 10443, 1, 1, 404, 0, "text/plain", "", "",
		"GET /foobar HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Connection: close\r\n\r\n",
		"HTTP/1.1 404 Not Found\r\n"
			"Content-length: 1%d8\r\n"
			"Content-type: %s\r\n\r\n"
			"<html><head><title>Not Found</title></head><body>Sorry, the object you requested was not found.</body><html>"
	},
	{ "simple post", POST, "http://127.0.0.1:10080/index.php?foo=bar", 10080, 0, 1, 200, 55, "application/json", "", "{\"foo\":\"bar\"}",
		"POST /index.php?foo=bar HTTP/1.0\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 13\r\n\r\n"
			"{\"foo\":\"bar\"}",
		"HTTP/1.1 200 OK\r\n"
				"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
				"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
				"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
				"ETag: \"d5-4e955b12f4640\"\r\n"
				"Accept-Ranges: bytes\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"%s"
	},
	{ "simple ssl post", POST, "https://127.0.0.1:10443/index.php?foo=bar", 10443, 1, 1, 200, 55, "application/json", "", "{\"foo\":\"bar\"}",
		"POST /index.php?foo=bar HTTP/1.0\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 13\r\n\r\n"
			"{\"foo\":\"bar\"}",
		"HTTP/1.1 200 OK\r\n"
				"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
				"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
				"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
				"ETag: \"d5-4e955b12f4640\"\r\n"
				"Accept-Ranges: bytes\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"%s"
	},
	{ "http auth", GET, "http://test:test1234@127.0.0.1:10080/", 10080, 0, 1, 200, 12, "text/html", "Hello World!", "",
		"GET / HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Authorization: Basic dGVzdDp0ZXN0MTIzNA==\r\n"
			"User-Agent: pilight\r\n"
			"Connection: close\r\n\r\n",
		"HTTP/1.1 200 OK\r\n"
				"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
				"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
				"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
				"ETag: \"d5-4e955b12f4640\"\r\n"
				"Accept-Ranges: bytes\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"%s"
	}
};

static CuTest *gtc = NULL;
static uv_thread_t pth;
static uv_thread_t pth1;
static int testnr = 0;
static int http_server = 0;
static int http_client = 0;
static int http_loop = 1;
static int is_ssl = 0;
static int is_ssl_init = 0;
static int doquit = 0;
static int threaded = 0;
static int started = 0;
static mbedtls_ssl_context ssl_ctx;

static void test(void *param);

static void callback(int code, char *data, int size, char *type, void *userdata) {
	CuAssertIntEquals(gtc, code, tests[testnr].code);
	CuAssertIntEquals(gtc, size, tests[testnr].size);
	CuAssertStrEquals(gtc, data, tests[testnr].data);
	if(tests[testnr].type != NULL) {
		CuAssertStrEquals(gtc, type, tests[testnr].type);
	}

	testnr++;

	doquit = 2;
	uv_thread_join(&pth);
	if(is_ssl == 1) {
		mbedtls_ssl_free(&ssl_ctx);
	}

	if(testnr < sizeof(tests)/sizeof(tests[0])) {
		if(threaded == 0) {
			test(NULL);
		} else {
			uv_stop(uv_default_loop());
		}
	} else {
		uv_stop(uv_default_loop());
	}
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
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
	FD_SET((unsigned long)http_server, &fdsread);

	if(tests[testnr].ssl == 1) {
		is_ssl = 1;
	} else {
		is_ssl = 0;
	}
	while(http_loop) {
		tv.tv_sec = 0;
		tv.tv_usec = 250000;

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
				CuAssertStrEquals(gtc, message, tests[testnr].recvmsg);

				struct connection_t c;
				http_parse_request(message, &c);
				dowrite = 1;
			}
			if(FD_ISSET(http_client, &fdswrite)) {
				FD_ZERO(&fdswrite);
				if(tests[testnr].sprintf == 1) {
					len = sprintf(message, tests[testnr].sendmsg, tests[testnr].size, tests[testnr].type, tests[testnr].data);
				} else {
					strcpy(message, tests[testnr].sendmsg);
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

static void test(void *param) {
	http_loop = 1;
	is_ssl_init = 0;
	is_ssl = 0;
	doquit = 0;

	http_start(tests[testnr].port);

	uv_thread_create(&pth, http_wait, NULL);

	if(tests[testnr].getpost == GET) {
		http_get_content(tests[testnr].url, callback, NULL);
	}
	if(tests[testnr].getpost == POST) {
		http_post_content(tests[testnr].url, tests[testnr].type, tests[testnr].post, callback, NULL);
	}

	started = 1;
}

static void test_http(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();
	started = 0;
	testnr = 0;

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("http.json", CONFIG_SETTINGS));	

	ssl_init();
	CuAssertIntEquals(tc, 0, ssl_server_init_status());

	test(NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	http_gc();
	ssl_gc();
	storage_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 10, testnr);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_http_threaded(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;
	testnr = 0;
	
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("http.json", CONFIG_SETTINGS));	

	ssl_init();
	CuAssertIntEquals(tc, 0, ssl_server_init_status());

	while(testnr < sizeof(tests)/sizeof(tests[0])) {
		started = 0;

		threaded = 1;

		uv_thread_create(&pth1, test, NULL);

		while(started == 0) {
			usleep(10);
		}

		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		/*
		 * FIXME: http_gc depends on poll->data structure.
		 * When running http_gc after walk_cb the poll
		 * structure is already freed.
		 */
		http_gc();

		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		uv_thread_join(&pth1);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}
	}

	ssl_gc();
	storage_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 10, testnr);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_http(void) {	
	CuSuite *suite = CuSuiteNew();

	FILE *f = fopen("http.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"../res/pilight.pem\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	SUITE_ADD_TEST(suite, test_http);
	SUITE_ADD_TEST(suite, test_http_threaded);

	return suite;
}