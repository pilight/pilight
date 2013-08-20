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

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "options.h"
#include "json.h"

typedef enum {
	RAW,
	SWITCH,
	DIMMER,
	WEATHER
} devtype_t;

typedef struct devices_t {
	char *id;
	char *desc;
	struct devices_t *next;
} devices_t;

typedef struct conflicts_t {
	char *id;
	struct conflicts_t *next;
} conflicts_t;

typedef struct protocol_t {
	char *id;
	devtype_t type;
	int header;
	int pulse;
	int footer;
	int rawLength;
	int binLength;
	unsigned short lsb;
	struct options_t *options;
	JsonNode *message;

	int bit;
	int recording;
	int raw[255];
	int code[255];
	int pCode[255];
	int binary[128]; // Max. the half the raw length

	struct devices_t *devices;
	struct conflicts_t *conflicts;

	void (*parseRaw)(void);
	void (*parseCode)(void);
	void (*parseBinary)(void);
	int (*createCode)(JsonNode *code);
	void (*printHelp)(void);
} protocol_t;

typedef struct protocols_t {
	struct protocol_t *listener;
	struct protocols_t *next;
} protocols_t;

struct protocols_t *protocols;

void protocol_register(protocol_t **proto);
void protocol_add_device(protocol_t *proto, const char *id, const char *desc);
void protocol_add_conflict(protocol_t *proto, const char *id);
void protocol_remove_conflict(protocol_t **proto, const char *id);
int protocol_has_device(protocol_t *proto, const char *id);

#endif
