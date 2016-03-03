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

/* Start the socket server */
int socket_start(unsigned short);
int socket_connect(char *, unsigned short);
int socket_connect1(char *, unsigned short, int (*callback)(struct eventpool_fd_t *, int));
int socket_timeout_connect(int, struct sockaddr *, int);
void socket_close(int);
int socket_write(int, const char *, ...);
int socket_send(int, char *);
int socket_recv(int, char **, size_t *);
int socket_read(int, char **, time_t);
void *socket_wait(void *);
int socket_gc(void);
unsigned int socket_get_port(void);
int socket_get_fd(void);
int socket_get_clients(int);

#endif
