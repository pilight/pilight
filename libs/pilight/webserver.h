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

#include "../websockets/libwebsockets.h"

int webserver_gc(void);
int webserver_ishex(int x);
int webserver_urldecode(const char *s, char *dec);
void webserver_create_header(unsigned char **p, const char *message, char *mimetype, unsigned int len);
void webserver_create_wsi(struct libwebsocket **wsi, int fd, unsigned char *stream, size_t size);
void webserver_create_401(unsigned char **p);
void webserver_create_404(const char *in, unsigned char **p);
int base64decode(unsigned char *dest, unsigned char *src, int l);
int webserver_callback_http(struct libwebsocket_context *webcontext, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len);
int webserver_callback_data(struct libwebsocket_context *webcontext, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len);
void webserver_queue(char *message);
void *webserver_broadcast(void *param);
void *webserver_clientize(void *param);
void *webserver_start(void *param);

#endif