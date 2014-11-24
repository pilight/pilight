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

#ifndef _RULES_H_
#define _RULES_H_

#include "json.h"
#include "config.h"

typedef struct rules_values_t {
	char *device;
	char *name;
	struct devices_settings_t *settings;
	struct rules_values_t *next;
} rules_values_t;

typedef struct rules_t {
	char *rule;
	char *name;
	char **devices;
	int nrdevices;
	int status;
	unsigned short active;
	struct JsonNode *arguments;
	struct rules_values_t *values;
	struct rules_t *next;
} rules_t;

struct config_t *config_rules;

void rules_init(void);
int rules_gc(void);
struct rules_t *rules_get(void);

#endif
