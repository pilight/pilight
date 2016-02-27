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
