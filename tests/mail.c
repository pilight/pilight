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
#include "../libs/pilight/core/mail.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/ssl.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"

/*
 * FIXME
 * - 300 urls
 */

static struct tests_t {
	char *desc;
	char *host;
	int success;
	int port;
	int ssl;
	int status;
	int recvmsgnr;
	int sendmsgnr;
	char sendmsg[12][1024];
	char recvmsg[12][1024];
} tests[] = {
	{ "gmail plain", "127.0.0.1", 0, 10025, 0, 0, 0, 0, {
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
			"Subject: test\r\n"
				"From: <info@pilight.org>\r\n"
				"To: <info@pilight.org>\r\n"
				"Content-Type: text/plain\r\n"
				"Mime-Version: 1.0\r\n"
				"X-Mailer: Emoticode smtp_send\r\n"
				"Content-Transfer-Encoding: 7bit\r\n"
				"\r\n"
				"test\r\n"
				".\r\n",
			"RSET\r\n",
			"QUIT\r\n"
		}
	},
	{ "gmail switch to ssl", "127.0.0.1", 0, 10587, 0, 0, 0, 0, {
			"220 smtp.gmail.com ESMTP o143sm14034035wmd.7 - gsmtp\r\n",
			"250-smtp.gmail.com at your service, [108.177.96.109]\r\n"
				"250-SIZE 35882577\r\n"
				"250-8BITMIME\r\n"
				"250-STARTTLS\r\n"
				"250-ENHANCEDSTATUSCODES\r\n"
				"250-PIPELINING\r\n"
				"250-CHUNKING\r\n"
				"250 SMTPUTF8\r\n",
			"220 2.0.0 Ready to start TLS\r\n",
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
			"STARTTLS\r\n",
			"EHLO pilight\r\n",
			"AUTH PLAIN AHBpbGlnaHQAdGVzdA==\r\n",
			"MAIL FROM: <info@pilight.org>\r\n",
			"RCPT TO: <info@pilight.org>\r\n",
			"DATA\r\n",
			"Subject: test\r\n"
				"From: <info@pilight.org>\r\n"
				"To: <info@pilight.org>\r\n"
				"Content-Type: text/plain\r\n"
				"Mime-Version: 1.0\r\n"
				"X-Mailer: Emoticode smtp_send\r\n"
				"Content-Transfer-Encoding: 7bit\r\n"
				"\r\n"
				"test\r\n"
				".\r\n",
			"RSET\r\n",
			"QUIT\r\n"
		}
	},
	{ "gmail full ssl", "127.0.0.1", 0, 10465, 1, 0, 0, 0, {
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
			"Subject: test\r\n"
				"From: <info@pilight.org>\r\n"
				"To: <info@pilight.org>\r\n"
				"Content-Type: text/plain\r\n"
				"Mime-Version: 1.0\r\n"
				"X-Mailer: Emoticode smtp_send\r\n"
				"Content-Transfer-Encoding: 7bit\r\n"
				"\r\n"
				"test\r\n"
				".\r\n",
			"RSET\r\n",
			"QUIT\r\n"
		}
	},
	{ "tele2 plain", "127.0.0.1", 0, 10587, 0, 0, 0, 0, {
			"220 smtp04.tele2.isp-net.nl ESMTP Postfix (Linux)\r\n",
			"250-smtp04.tele2.isp-net.nl\r\n"
				"250-PIPELINING\r\n"
				"250-SIZE 10485760\r\n"
				"250-VRFY\r\n"
				"250-ETRN\r\n"
				"250-STARTTLS\r\n"
				"250-AUTH PLAIN LOGIN\r\n"
				"250-ENHANCEDSTATUSCODES\r\n"
				"250-8BITMIME\r\n"
				"250 DSN\r\n",
			"235 2.7.0 Authentication successful\r\n",
			"250 2.1.0 Ok\r\n",
			"250 2.1.5 Ok\r\n",
			"354 End data with <CR><LF>.<CR><LF>\r\n",
			"250 2.0.0 Ok: queued as 5272B8B\r\n",
			"250 2.0.0 Ok\r\n",
			"221 2.0.0 Bye\r\n"
		},
		{
			"EHLO pilight\r\n",
			"AUTH PLAIN AHBpbGlnaHQAdGVzdA==\r\n",
			"MAIL FROM: <info@pilight.org>\r\n",
			"RCPT TO: <info@pilight.org>\r\n",
			"DATA\r\n",
			"Subject: test\r\n"
				"From: <info@pilight.org>\r\n"
				"To: <info@pilight.org>\r\n"
				"Content-Type: text/plain\r\n"
				"Mime-Version: 1.0\r\n"
				"X-Mailer: Emoticode smtp_send\r\n"
				"Content-Transfer-Encoding: 7bit\r\n"
				"\r\n"
				"test\r\n"
				".\r\n",
			"RSET\r\n",
			"QUIT\r\n"
		}
	},
	{ "gmail ehlo error", "127.0.0.1", 0, 10587, 0, -1, 0, 0,
		{
			"220 smtp.gmail.com ESMTP im3sm19207876wjb.13 - gsmtp\r\n",
			"501-5.5.4 Empty HELO/EHLO argument not allowed, closing connection.\r\n"
			"501 5.5.4  https://support.google.com/mail/?p=helo p184sm18287648wmg.3 - gsmtp"
		},
		{
			"EHLO pilight\r\n"
		}
	},
	{ "gmail invalid base64", "127.0.0.1", 0, 10587, 0, -1, 0, 0,
		{
			"220 smtp.gmail.com ESMTP im3sm19207876wjb.13 - gsmtp\r\n",
			"250-smtp.gmail.com at your service, [108.177.96.109]\r\n"
				"250-SIZE 35882577\r\n"
				"250-8BITMIME\r\n"
				"250-AUTH LOGIN PLAIN XOAUTH2 PLAIN-CLIENTTOKEN OAUTHBEARER XOAUTH\r\n"
				"250-ENHANCEDSTATUSCODES\r\n"
				"250-PIPELINING\r\n"
				"250-CHUNKING\r\n"
				"250 SMTPUTF8\r\n",
			"501 5.5.2 Cannot Decode response h3sm23804602wjp.45 - gsmtp"
		},
		{
			"EHLO pilight\r\n",
			"AUTH PLAIN AHBpbGlnaHQAdGVzdA==\r\n",
		}
	},
	{ "gmail wrong username/password", "127.0.0.1", 0, 10587, 0, -1, 0, 0,
		{
			"220 smtp.gmail.com ESMTP im3sm19207876wjb.13 - gsmtp\r\n",
			"250-smtp.gmail.com at your service, [108.177.96.109]\r\n"
				"250-SIZE 35882577\r\n"
				"250-8BITMIME\r\n"
				"250-AUTH LOGIN PLAIN XOAUTH2 PLAIN-CLIENTTOKEN OAUTHBEARER XOAUTH\r\n"
				"250-ENHANCEDSTATUSCODES\r\n"
				"250-PIPELINING\r\n"
				"250-CHUNKING\r\n"
				"250 SMTPUTF8\r\n",
			"535-5.7.8 Username and Password not accepted. Learn more at\r\n"
			"535 5.7.8  https://support.google.com/mail/?p=BadCredentials h3sm23804602wjp.45 - gsmtp\r\n"
		},
		{
			"EHLO pilight\r\n",
			"AUTH PLAIN AHBpbGlnaHQAdGVzdA==\r\n",
		}
	},
	{ "invalid smtp host", "WvQTxNJ13BJUBC62R8PM", -1, 10587, 0, -1, 0, 0, { "foo" }, { "foo" }},
	{ "gmail plain (ipv6)", "::1", 0, 10025, 0, 0, 0, 0, {
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
			"Subject: test\r\n"
				"From: <info@pilight.org>\r\n"
				"To: <info@pilight.org>\r\n"
				"Content-Type: text/plain\r\n"
				"Mime-Version: 1.0\r\n"
				"X-Mailer: Emoticode smtp_send\r\n"
				"Content-Transfer-Encoding: 7bit\r\n"
				"\r\n"
				"test\r\n"
				".\r\n",
			"RSET\r\n",
			"QUIT\r\n"
		}
	},
};

static CuTest *gtc = NULL;
static uv_thread_t pth;
static uv_thread_t pth1;
static int testnr = 0;
static int mail_server = 0;
static int mail_client = 0;
static int doquit = 0;
static int is_ssl = 0;
static int force_ssl = 0;
static int is_ssl_init = 0;
static int is_ssl_hello = 0;
static mbedtls_ssl_context ssl_ctx;
static int mail_loop = 1;
static int steps = 0;
static int threaded = 0;
static int started = 0;
static int received = 0;
static struct mail_t *mail = NULL;

static char *sender = "info@pilight.org";
static char *user = "pilight";
static char *password = "test";
static char *subject = "test";
static char *message = "test";
static char *to = "info@pilight.org";

static void test(void *param);
uv_timer_t *timer_req = NULL;

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void stop(uv_timer_t *handle) {
	uv_close((uv_handle_t *)handle, close_cb);
	mail_loop = 0;

	if(is_ssl == 1) {
		mbedtls_ssl_free(&ssl_ctx);
	}

	uv_stop(uv_default_loop());
	uv_thread_join(&pth);
}

static void callback(int status, struct mail_t *mail) {
	CuAssertIntEquals(gtc, tests[testnr].status, status);

	testnr++;

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 100, 0);

	if(status == -1 && mail_server > 0 && mail_client > 0) {
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
	}
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

	if((message = MALLOC(BUFFER_SIZE+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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

	if(tests[testnr].ssl == 1) {
		is_ssl = 1;
	} else {
		is_ssl = 0;
	}
	while(mail_loop) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;

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
			if(is_ssl == 1 && is_ssl_init == 0) {
				is_ssl_init = 1;
				r = mbedtls_ssl_setup(&ssl_ctx, &ssl_server_conf);
				CuAssertTrue(gtc, r >= 0);

				r = mbedtls_ssl_session_reset(&ssl_ctx);
				CuAssertTrue(gtc, r >= 0);

				mbedtls_ssl_set_bio(&ssl_ctx, &mail_client, mbedtls_net_send, mbedtls_net_recv, NULL);
			}
			if(FD_ISSET(mail_server, &fdsread)) {
				FD_ZERO(&fdsread);
				if(mail_client == 0) {
					mail_client = accept(mail_server, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
					CuAssertTrue(gtc, mail_client > 0);

					memset(message, '\0', BUFFER_SIZE);
					uv_ip4_name((void *)&(addr.sin_addr), message, BUFFER_SIZE);
					dowrite = 1;
				}
			}
			if(FD_ISSET(mail_client, &fdsread)) {
				FD_ZERO(&fdsread);
				memset(message, '\0', BUFFER_SIZE);
				if(is_ssl == 1 || force_ssl == 1) {
					r = mbedtls_ssl_read(&ssl_ctx, (unsigned char *)message, BUFFER_SIZE);
					if(r == MBEDTLS_ERR_SSL_WANT_READ) {
						doread = 1;
						goto next;
					}
				} else {
					r = recv(mail_client, message, BUFFER_SIZE, 0);
				}

				CuAssertTrue(gtc, r >= 0);
				CuAssertStrEquals(gtc, tests[testnr].recvmsg[tests[testnr].recvmsgnr++], message);
				if(strstr(message, "EHLO") != NULL && is_ssl_init == 1) {
					is_ssl_hello = 1;
					steps = -1;
				}
				if(strcmp(message, "STARTTLS\r\n") == 0) {
					is_ssl = 1;
				}
				if(strcmp(message, "QUIT\r\n") == 0) {
					doquit = 1;
				}
				steps++;
				dowrite = 1;
			}
			if(FD_ISSET(mail_client, &fdswrite)) {
				FD_ZERO(&fdswrite);
				strcpy(message, tests[testnr].sendmsg[tests[testnr].sendmsgnr]);
				len = strlen(message);
				if((is_ssl == 1 && is_ssl_init == 1 && is_ssl_hello == 1) || force_ssl == 1) {
					r = mbedtls_ssl_write(&ssl_ctx, (unsigned char *)message, len);
					if(r == MBEDTLS_ERR_SSL_WANT_READ) {
						dowrite = 1;
						goto next;
					}
				} else {
					r = send(mail_client, message, len, 0);
				}
				received = 0;
				if(doquit == 1) {
					doquit = 2;
				}
				CuAssertIntEquals(gtc, len, r);
				tests[testnr].sendmsgnr++;
				doread = 1;
				steps++;
			}
		}
next:
		if(dowrite == 1) {
			FD_SET((unsigned long)mail_client, &fdswrite);
		}
		if(doread == 1) {
			FD_SET((unsigned long)mail_client, &fdsread);
		}
	}

clear:
	FREE(message);
	return;
}

static void mail_start(char *host, int port) {
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	char *ip = NULL;
	int type = 0;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	type = host2ip(host, &ip);
	CuAssertTrue(gtc, type == AF_INET || type == AF_INET6);

	mail_server = socket(type, SOCK_STREAM, 0);
	CuAssertTrue(gtc, mail_server > 0);

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

	r = setsockopt(mail_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, r >= 0);

	switch(type) {
		case AF_INET: {
			r = bind(mail_server, (struct sockaddr *)&addr4, sizeof(addr4));
		} break;
		case AF_INET6: {
			r = bind(mail_server, (struct sockaddr *)&addr6, sizeof(addr6));
		} break;
	}
	CuAssertTrue(gtc, r >= 0);

	r = listen(mail_server, 0);
	CuAssertTrue(gtc, r >= 0);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test(void *param) {
	mail_loop = 1;
	is_ssl_init = 0;
	is_ssl_hello = 0;
	is_ssl = 0;
	force_ssl = 0;
	steps = 0;
	doquit = 0;

	int success = tests[testnr].success;

	tests[testnr].recvmsgnr = 0;
	tests[testnr].sendmsgnr = 0;

	if(tests[testnr].ssl == 1) {
		force_ssl = 1;
	}

	if(success == 0) {
		mail_start(tests[testnr].host, tests[testnr].port);
		uv_thread_create(&pth, mail_wait, NULL);
	}


	mail->subject = subject;
	mail->message = message;
	mail->from = sender;
	mail->to = to;

	CuAssertIntEquals(gtc, success, sendmail(tests[testnr].host, user, password, tests[testnr].port, tests[testnr].ssl, mail, callback));

	if(success == 0) {
		started = 1;
	}
}

static void test_mail(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	started = 0;
	testnr = 0;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "mail.c");

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("mail.json", CONFIG_SETTINGS));

	ssl_init();

	if((mail = MALLOC(sizeof(struct mail_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	while(testnr < sizeof(tests)/sizeof(tests[0])) {
		printf("[ - %-46s ]\n", tests[testnr].desc);
		fflush(stdout);

		int success = tests[testnr].success;
		threaded = 0;
		test(NULL);

		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		if(success == 0) {
			uv_thread_join(&pth);
		}
	}

	FREE(mail);

	ssl_gc();
	storage_gc();
	plua_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 9, testnr);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_mail_threaded(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	started = 0;
	testnr = 0;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	plua_init();

	test_set_plua_path(tc, __FILE__, "mail.c");

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("mail.json", CONFIG_SETTINGS));

	ssl_init();

	if((mail = MALLOC(sizeof(struct mail_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	while(testnr < sizeof(tests)/sizeof(tests[0])) {
		printf("[ - %-46s ]\n", tests[testnr].desc);
		fflush(stdout);

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

	FREE(mail);

	ssl_gc();
	storage_gc();
	plua_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 9, testnr);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_mail_dot_message(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	started = 0;
	testnr = 0;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	if((mail = MALLOC(sizeof(struct mail_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	mail->subject = subject;
	mail->message = ".";
	mail->from = sender;
	mail->to = to;

	CuAssertIntEquals(gtc, -1, sendmail("127.0.0.1", user, password, 10025, 0, mail, callback));

	FREE(mail);
	CuAssertIntEquals(tc, 0, xfree());
}

static void callback1(int status, struct mail_t *mail) {
	CuAssertIntEquals(gtc, -1, status);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 100, 0);
}

static void test_mail_inactive_server(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;
	started = 0;
	testnr = 0;

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	if((mail = MALLOC(sizeof(struct mail_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	mail->subject = subject;
	mail->message = message;
	mail->from = sender;
	mail->to = to;

	CuAssertIntEquals(gtc, 0, sendmail("127.0.0.1", user, password, 10025, 0, mail, callback1));

	plua_init();

	test_set_plua_path(tc, __FILE__, "mail.c");

	eventpool_init(EVENTPOOL_NO_THREADS);
	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("mail.json", CONFIG_SETTINGS));

	ssl_init();

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	FREE(mail);

	ssl_gc();
	storage_gc();
	plua_gc();
	eventpool_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_mail(void) {
	CuSuite *suite = CuSuiteNew();

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\"},"\
		"\"hardware\":{},\"registry\":{}}";

	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("mail.c", "", &file);

	FILE *f = fopen("mail.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	SUITE_ADD_TEST(suite, test_mail);
	SUITE_ADD_TEST(suite, test_mail_threaded);
	SUITE_ADD_TEST(suite, test_mail_dot_message);
	SUITE_ADD_TEST(suite, test_mail_inactive_server);

	return suite;
}
