/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#ifdef WEBSERVER_HTTPS
#include "../../mbedtls/mbedtls/ssl.h"
#endif

typedef struct connection_t {
	int fd;
	char *request;
  const char *request_method;
  const char *uri;
  const char *http_version;
  const char *query_string;

  int num_headers;
  struct headers {
    const char *name;
    const char *value;
  } http_headers[30];

  char *content;
  size_t content_len;

  int is_websocket;
	int ping;
  int status_code;
	void *connection_param;
  void *callback_param;
  unsigned int flags;
	unsigned short timer;

	unsigned int pull;
	unsigned int push;

	// char *fin;
	// size_t fin_size;

#ifdef WEBSERVER_HTTPS
	int is_ssl;
	int handshake;
	mbedtls_ssl_context ssl;
#endif
} connection_t;

int webserver_gc(void);
int webserver_start(void);
void *webserver_broadcast(void *param);
void webserver_create_header(char **p, const char *message, char *mimetype, unsigned long len);
int http_parse_request(char *buffer, struct connection_t *c);
const char *http_get_header(const struct connection_t *conn, const char *s);

#endif
