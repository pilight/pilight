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

typedef struct devices_t devices_t;

struct devices_t {
	char id[25];
	char desc[50];
	struct devices_t *next;
};

typedef struct conflicts_t conflicts_t;

struct conflicts_t {
	char id[25];
	struct conflicts_t *next;
};

typedef struct protocol_t protocol_t;

struct protocol_t {
	char id[25];
	devtype_t type;
	int header;
	int pulse;
	int footer;
	int rawLength;
	int binLength;
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
};

typedef struct {
	int nr;
	protocol_t *listeners[8]; // Change this to the number of available protocols
} protocols_t;

protocols_t protocols;

void protocol_register(protocol_t *proto);
void protocol_unregister(protocol_t *proto);
void protocol_add_device(protocol_t *proto, const char *id, const char *desc);
void protocol_add_conflict(protocol_t *proto, const char *id);
int protocol_has_device(protocol_t **proto, const char *id);

#endif
