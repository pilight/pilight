/*
	Copyright (C) 2013 - 2014 CurlyMo

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

#ifndef _DEVICES_H_
#define _DEVICES_H_

#include <pthread.h>
#include "protocol.h"
#include "threads.h"
#include "config.h"

typedef struct devices_settings_t devices_settings_t;
typedef struct devices_values_t devices_values_t;

/*
|------------------|
| devices_devices_t|
|------------------|
| id               |
| name		       |
| protocols	       | --> protocols_t <protocol.h>
| settings	       | ---
|------------------|   |
				       |
|------------------|   |
|devices_settings_t| <--
|------------------|
| name             |
| values	       | ---
|------------------|   |
				       |
|------------------|   |
| devices_values_t | <--
|------------------|
| value            |
| type		       |
|------------------|
*/

struct devices_values_t {
	union {
		char *string_;
		double number_;
	};
	int decimals;
	char *name;
	int type;
	struct devices_values_t *next;
};

struct devices_settings_t {
	char *name;
	struct devices_values_t *values;
	struct devices_settings_t *next;
};

struct devices_t {
	char *id;
	char dev_uuid[21];
	char ori_uuid[21];
	int cst_uuid;
	int nrthreads;
	time_t timestamp;
	struct protocols_t *protocols;
	struct devices_settings_t *settings;
	struct threadqueue_t **threads;
	struct devices_t *next;
};

struct config_t *config_devices;

int devices_update(char *protoname, JsonNode *message, JsonNode **out);
int devices_get(char *sid, struct devices_t **dev);
int devices_valid_state(char *sid, char *state);
int devices_valid_value(char *sid, char *name, char *value);
struct JsonNode *devices_values(void);
void devices_init(void);
int devices_gc(void);

#endif
