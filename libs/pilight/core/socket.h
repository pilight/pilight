/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _SOCKETS_H_
#define _SOCKETS_H_

#include <time.h>
#include "eventpool.h"

#define SOCKET_SERVER	0
#define SOCKET_CLIENT	1

/* Start the socket server */
void socket_override(int);
int socket_start(unsigned short, void (*)(int, char *, ssize_t));
int socket_connect(char *, unsigned short, void (*)(int, char *, ssize_t));
void socket_close(int);
int socket_write(int, const char *, ...);
ssize_t socket_recv(char *, int, char **, ssize_t *);
int socket_gc(void);
unsigned int socket_get_port(void);

#endif
