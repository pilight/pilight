/*
	Copyright (C) 2015 CurlyMo

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

#ifndef _EVENTPOOL_STRUCTS_H_
#define _EVENTPOOL_STRUCTS_H_

#include "defines.h"
#include "eventpool_structs.h"

typedef struct reason_ssdp_received_t {
	char name[17];
	char uuid[UUID_LENGTH+1];
	char ip[INET_ADDRSTRLEN+1];
	int port;
} reason_ssdp_received_t;

typedef struct reason_received_pulsetrain_t {
	int length;
	int pulses[MAXPULSESTREAMLENGTH+1];
	char *hardware;
} reason_received_pulsetrain_t;

typedef struct reason_code_received_t {
	char message[1025];
	char origin[256];
	char *protocol;
	char *uuid;

	int repeat;
} reason_code_received_t;

typedef struct reason_code_sent_t {
	char message[1025];
	char settings[1025];
	char protocol[256];
	char origin[256];
	char uuid[UUID_LENGTH+1];

	int type;
	int repeat;
} reason_code_sent_t;

typedef struct reason_socket_disconnected_t {
	int fd;
} reason_socket_disconnected_t;

typedef struct reason_socket_connected_t {
	int fd;
} reason_socket_connected_t;

typedef struct reason_ssdp_disconnected_t {
	int fd;
} reason_ssdp_disconnected_t;

typedef struct reason_webserver_connected_t {
	int fd;
} reason_webserver_connected_t;

typedef struct reason_socket_received_t {
	int fd;
	char *buffer;
	char type[256];
} reason_socket_received_t;

typedef struct reason_socket_send_t {
	int fd;
	char *buffer;
	char type[256];
} reason_socket_send_t;

typedef struct reason_send_code_t {
	int origin;
	char message[1025];
	int rawlen;
	int txrpt;
	int pulses[MAXPULSESTREAMLENGTH+1];
	char protocol[256];
	int hwtype;
	char settings[1025];
	char uuid[UUID_LENGTH+1];
} reason_send_code_t;

typedef struct reason_config_update_t {
	char origin[256];
	int type;
	time_t timestamp;
	int nrdev;
	char devices[256][256];
	int nrval;
	struct {
		char name[256];
		union {
			char string_[256];
			double number_;
		};
		int decimals;
		int type;
	} values[256];
	char *uuid;
} reason_config_update_t;
#endif
