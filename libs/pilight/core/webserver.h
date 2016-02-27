/*
	Copyright (C) 2013 CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#ifdef WEBSERVER_HTTPS
#include "../../mbedtls/mbedtls/ssl.h"
#endif

typedef struct connection_t {
	int fd;
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
void webserver_create_header(char **p, const char *message, char *mimetype, unsigned int len);
int http_parse_request(char *buffer, struct connection_t *c);
const char *http_get_header(const struct connection_t *conn, const char *s);

#endif
