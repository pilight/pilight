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
#include "../libs/pilight/lua_c/lua.h"

#include "alltests.h"

#include "gplv3.h"

#define GET		0
#define	POST	1
#define BUFSIZE 1024*1024

void *_userdata = NULL;

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
	{ "plain get (ipv6)", GET, "http://[::1]:10080/", 10080, 0, 1, 200, 12, "text/html", "Hello World!", "",
			"GET / HTTP/1.1\r\n"
				"Host: ::1\r\n"
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
	{ "header 301", GET, "http://127.0.0.1:10080/", 10080, 0, 0, 301, 23, NULL, "http://raspi4/nodeMcu2/", NULL,
			"GET / HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"User-Agent: pilight\r\n"
				"Connection: close\r\n\r\n",
			"HTTP/1.0 301 Moved Permanently\r\n"
			"Location: http://raspi4/nodeMcu2/\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n"
			"Date: Sat, 11 Aug 2018 11:15:15 GMT\r\n"
			"Server: lighttpd/1.4.35\r\n\r\n"
	},
	// Content-Length is actually 12 bytes
	{ "too big content-length", GET, "http://127.0.0.1:10080/", 10080, 0, 1, 408, 0, NULL, NULL, "",
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
				"Content-Length: 18\r\n"
				"Connection: close\r\n"
				"Content-Type: %s\r\n\r\n"
				"Hello World!"
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
	{ "big file get without content-length", GET, "http://127.0.0.1:10080/LICENSE.txt", 10080, 0, 1, 200, 35146, "text/plain", gplv3, "",
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
			"0000001E\r\n"
			"123456789012345678901234567890\r\n"
			"00000000\r\n\r\n"
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
	{ "partial chunked get", GET, "http://127.0.0.1:10080/test.jpg", 10080, 0, 0, 408, 0, NULL, NULL, "",
		"GET /test.jpg HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Connection: close\r\n\r\n",
		"HTTP/1.1 200 OK\r\n"
			"Keep-Alive: timeout=15, max=100\r\n"
			"Transfer-Encoding: chunked\r\n\r\n"
			"1E\r\n"
			"123456789012345678901234567890\r\n"
	},
	{ "404 header, ssl, and lower case headers", GET, "https://127.0.0.1:10443/foobar", 10443, 1, 1, 404, 108, "text/plain", "<html><head><title>Not Found</title></head><body>Sorry, the object you requested was not found.</body><html>", "",
		"GET /foobar HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Connection: close\r\n\r\n",
		"HTTP/1.1 404 Not Found\r\n"
			"Content-length: %d\r\n"
			"Content-type: %s\r\n\r\n"
			"%s"
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
	},
	{ "connection made but no response", GET, "http://127.0.0.1:10080/", 10080, 0, 0, 408, 0, NULL, NULL, NULL,
			"GET / HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"User-Agent: pilight\r\n"
				"Connection: close\r\n\r\n",
			NULL
	},
	{ "ip exists but no connection", GET, "http://127.0.0.1:10080/", 10081, 0, 0, 404, 0, NULL, NULL, NULL,
			NULL,
			NULL
	},
	{ "invalid url (getaddrinfo error)", GET, "http://WvQTxNJ13BJUBC62R8PM.com/", 10080, 0, 1, 404, 0, NULL, NULL, "",
			NULL,
			NULL
	},
};

static CuTest *gtc = NULL;
static uv_thread_t pth;
static uv_thread_t pth1;
static int testnr = 0;
static int http_server = 0;
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
	CuAssertTrue(gtc, _userdata == userdata);

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
	int http_client = 0;
	fd_set fdsread;
	fd_set fdswrite;

	if((message = MALLOC(BUFSIZE)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if(doquit == 2) {
			http_loop = 0;
#ifdef _WIN32
			if(http_client > 0) {
				closesocket(http_client);
			}
			if(http_server > 0) {
				closesocket(http_server);
			}
#else
			if(http_client > 0) {
				close(http_client);
			}
			if(http_server > 0) {
				close(http_server);
			}
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
				if(tests[testnr].recvmsg != NULL) {
					CuAssertStrEquals(gtc, message, tests[testnr].recvmsg);
				}
				if(tests[testnr].sendmsg != NULL) {
					dowrite = 1;
				}
			}
			if(FD_ISSET(http_client, &fdswrite)) {
				FD_ZERO(&fdswrite);
				if(tests[testnr].sprintf == 1) {
					if(strstr(tests[testnr].sendmsg, "Content-Length") == NULL && strstr(tests[testnr].sendmsg, "Content-length") == NULL) {
						len = sprintf(message, tests[testnr].sendmsg, tests[testnr].type, tests[testnr].data);
					} else {
						len = sprintf(message, tests[testnr].sendmsg, tests[testnr].size, tests[testnr].type, tests[testnr].data);
					}
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

static void http_start(char *url, int port) {
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	int r = 0;
	int opt = 1;
	int type = 0;
	int len = 0;
	char *ip = NULL;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	/*
	 * Remove http:// or https://
	 */
	char *cpy = STRDUP(url);
	CuAssertPtrNotNull(gtc, cpy);
	str_replace("https://", "", &cpy);
	str_replace("http://", "", &cpy);

	/*
	 * Remove backslash after host and port
	 */
	int pos = rstrstr(cpy, "/")-cpy;
	cpy[pos] = '\0';

	/*
	 * Remove port
	 */
	if(rstrstr(cpy, ":") != NULL) {
		pos = rstrstr(cpy, ":")-cpy;
		cpy[pos] = '\0';
	}

	/*
	 * Remove auth
	 */
	if(rstrstr(cpy, "@") != NULL) {
		pos = (strstr(cpy, "@")-cpy)+1;
		len = strlen(cpy)-pos;
		memmove(&cpy[0], &cpy[pos], len);
		cpy[len] = '\0';
	}

	/*
	 * Remove hooks
	 */
	len = strlen(cpy)-1;
	if(cpy[0] == '[') {
		memmove(&cpy[0], &cpy[1], len);
		cpy[len] = '\0';
	}
	if(cpy[len-1] == ']') {
		cpy[len-1] = '\0';
	}

	type = host2ip(cpy, &ip);
	CuAssertTrue(gtc, type == AF_INET || type == AF_INET6);
	FREE(cpy);

	http_server = socket(type, SOCK_STREAM, 0);
	CuAssertTrue(gtc, http_server >= 0);

	switch(type) {
		case AF_INET: {
			memset((char *)&addr4, '\0', sizeof(addr4));
			addr4.sin_family = type;
			addr4.sin_addr.s_addr = htonl(INADDR_ANY);
			addr4.sin_port = htons(port);
		} break;
		case AF_INET6: {
			memset((char *)&addr6, '\0', sizeof(addr6));
			addr6.sin6_family = type;
			addr6.sin6_flowinfo = 0;
			addr6.sin6_port = htons(port);
			addr6.sin6_addr = in6addr_loopback;
		} break;
		default: {
		} break;
	}
	FREE(ip);

	r = setsockopt(http_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, r >= 0);

	switch(type) {
		case AF_INET: {
			r = bind(http_server, (struct sockaddr *)&addr4, sizeof(addr4));
		} break;
		case AF_INET6: {
			r = bind(http_server, (struct sockaddr *)&addr6, sizeof(addr6));
		} break;
	}
	CuAssertTrue(gtc, r >= 0);
	r = listen(http_server, 0);
	CuAssertTrue(gtc, r >= 0);
}

static void test(void *param) {
	printf("[ - %-46s ]\n", tests[testnr].desc);
	fflush(stdout);

	http_loop = 1;
	is_ssl_init = 0;
	is_ssl = 0;
	doquit = 0;

	if(testnr != 17) {
		http_start(tests[testnr].url, tests[testnr].port);
		uv_thread_create(&pth, http_wait, NULL);
	}

	if(tests[testnr].getpost == GET) {
		http_get_content(tests[testnr].url, callback, _userdata);
	}
	if(tests[testnr].getpost == POST) {
		http_post_content(tests[testnr].url, tests[testnr].type, tests[testnr].post, callback, _userdata);
	}

	started = 1;
}

static void test_http(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();
	started = 0;
	testnr = 0;

	gtc = tc;

	_userdata = MALLOC(0);
	CuAssertPtrNotNull(tc, _userdata);

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "http.c");

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

	plua_gc();
	http_gc();
	ssl_gc();
	storage_gc();
	eventpool_gc();

	FREE(_userdata);

	CuAssertIntEquals(tc, 18, testnr);
	CuAssertIntEquals(tc, 0, xfree());

}

static void test_http_threaded(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;
	testnr = 0;

	_userdata = MALLOC(0);
	CuAssertPtrNotNull(tc, _userdata);

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "http.c");

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

	plua_gc();
	http_gc();
	ssl_gc();
	storage_gc();
	eventpool_gc();

	FREE(_userdata);
	CuAssertIntEquals(tc, 18, testnr);
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
