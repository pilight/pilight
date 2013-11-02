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
	WEATHER,
	RELAY,
	SCREEN
} devtype_t;

typedef struct protocol_devices_t {
	char *id;
	char *desc;
	struct protocol_devices_t *next;
} protocol_devices_t;

typedef struct protocol_conflicts_t {
	char *id;
	struct protocol_conflicts_t *next;
} protocol_conflicts_t;

typedef struct protocol_plslen_t {
	int length;
	struct protocol_plslen_t *next;
} protocol_plslen_t;

typedef struct protocol_settings_t {
	char *name;
	char *cur_value;
	char *old_value;
	unsigned short type;
	struct protocol_settings_t *next;
} protocol_settings_t;

typedef struct protocol_t {
	char *id;
	devtype_t type;
	int header;
	int pulse;
	struct protocol_plslen_t *plslen;	
	int footer;
	int rawlen;
	int binlen;
	short txrpt;
	short rxrpt;
	unsigned short lsb;
	struct options_t *options;
	JsonNode *message;

	int repeats;
	unsigned long first;
	unsigned long second;	
	
	int bit;
	int recording;
	int raw[255];
	int code[255];
	int pCode[255];
	int binary[128]; // Max. the half the raw length

	struct protocol_devices_t *devices;
	struct protocol_conflicts_t *conflicts;
	struct protocol_settings_t *settings;

	void (*parseRaw)(void);
	void (*parseCode)(void);
	void (*parseBinary)(void);
	int (*createCode)(JsonNode *code);
	int (*checkValues)(JsonNode *code);
	void (*printHelp)(void);
} protocol_t;

typedef struct protocols_t {
	struct protocol_t *listener;
	char *name;
	struct protocols_t *next;
} protocols_t;

struct protocols_t *protocols;

void protocol_init(void);
void protocol_plslen_add(protocol_t *proto, int plslen);
void protocol_register(protocol_t **proto);
void protocol_device_add(protocol_t *proto, const char *id, const char *desc);
void protocol_conflict_add(protocol_t *proto, const char *id);
void protocol_setting_add_string(protocol_t *proto, const char *name, const char *value);
void protocol_setting_add_number(protocol_t *proto, const char *name, int value);
int protocol_setting_get_string(protocol_t *proto, const char *name, char **out);
int protocol_setting_get_number(protocol_t *proto, const char *name, int *out);
int protocol_setting_update_string(protocol_t *proto, const char *name, const char *value);
int protocol_setting_update_number(protocol_t *proto, const char *name, int value);
int protocol_setting_check_number(protocol_t *proto, const char *name, int value);
int protocol_setting_check_string(protocol_t *proto, const char *name, const char *value);
int protocol_setting_restore(protocol_t *proto, const char *name);
void protocol_setting_remove(protocol_t **proto, const char *name);
void protocol_conflict_remove(protocol_t **proto, const char *id);
int protocol_device_exists(protocol_t *proto, const char *id);
int protocol_gc(void);

#endif
