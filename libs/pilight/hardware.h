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

#ifndef _HARDWARE_H_
#define _HARDWARE_H_

typedef enum {
	NONE,
	RF433,
	RF868,
	SENSOR,
	HWRELAY,
	API
} hwtype_t;

#include "options.h"
#include "json.h"

typedef struct conf_hardware_t conf_hardware_t;

typedef struct hardware_t {
	char *id;
	hwtype_t type;
	struct options_t *options;

	unsigned short (*init)(void);
	unsigned short (*deinit)(void);
	int (*receive)(void);
	int (*send)(int *code);
	unsigned short (*settings)(JsonNode *json);
} hardware_t;

struct conf_hardware_t {
	hardware_t *hardware;
	struct conf_hardware_t *next;
};

typedef struct hwlst_t {
	struct hardware_t *listener;
	struct hwlst_t *next;
} hwlst_t;

struct hwlst_t *hwlst;
struct conf_hardware_t *conf_hardware;

void hardware_init(void);
void hardware_register(hardware_t **hw);
void hardware_set_id(hardware_t *hw, const char *id);
int hardware_gc(void);
int hardware_set_file(char *file);
int hardware_read(void);
int hardware_write(char *content);

#endif
