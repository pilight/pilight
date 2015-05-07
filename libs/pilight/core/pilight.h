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

#ifndef _PILIGHT_H_
#define _PILIGHT_H_

typedef enum origin_t origin_t;
typedef enum runmode_t runmode_t;

#include "defines.h"
#include "json.h"
#include "mem.h"

enum runmode_t {
	STANDALONE,
	ADHOC
};

enum origin_t {
	RECEIVER = 0,
	SENDER,
	MASTER,
	NODE,
	FW,
	STATS,
	ACTION,
	RULE,
	PROTOCOL
};

#include "../config/devices.h"

struct pilight_t {
	void (*broadcast)(char *name, JsonNode *message, enum origin_t origin);
	int (*send)(JsonNode *json, enum origin_t origin);
	int (*control)(struct devices_t *dev, char *state, JsonNode *values, enum origin_t origin);
	runmode_t runmode;
	/* pilight actually runs in this stage and the configuration is fully validated */
	int running;
	int debuglevel;
} pilight_t;

extern struct pilight_t pilight;

extern char pilight_uuid[UUID_LENGTH];

#endif
