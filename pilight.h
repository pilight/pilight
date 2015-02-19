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

#include "defines.h"
#include "json.h"
#include "mem.h"
#include "devices.h"

typedef enum runmode_t {
	STANDALONE,
	ADHOC
} runmode_t;

extern struct pilight_t {
    void (*broadcast)(char *name, JsonNode *message);
    int (*send)(JsonNode *json);
    int (*control)(struct devices_t *dev, char *state, JsonNode *values);
    void (*receive)(int *rawcode, int rawlen, int plslen, int hwtype);
    runmode_t runmode;
} pilight;

char pilight_uuid[UUID_LENGTH];

#endif
