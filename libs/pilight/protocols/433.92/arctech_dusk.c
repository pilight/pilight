/*
	Copyright (C) 2014 CurlyMo & lvdp

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "arctech_dusk.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	250
#define MAX_PULSE_LENGTH	282
#define AVG_PULSE_LENGTH	277
#define RAW_LENGTH				132

static int validate(void) {
	if(arctech_dusk->rawlen == RAW_LENGTH) {
		if(arctech_dusk->raw[arctech_dusk->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   arctech_dusk->raw[arctech_dusk->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV) &&
			 arctech_dusk->raw[1] >= AVG_PULSE_LENGTH*(PULSE_MULTIPLIER*3)) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(char **message) {
	int binary[RAW_LENGTH/4], x = 0, i = 0;

	if(arctech_dusk->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "arctech_dusk: parsecode - invalid parameter passed %d", arctech_dusk->rawlen);
		return;
	}

	for(x=0;x<arctech_dusk->rawlen;x+=4) {
		if(arctech_dusk->raw[x+3] > AVG_PULSE_LENGTH*PULSE_MULTIPLIER) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	int unit = binToDecRev(binary, 28, 31);
	int state = binary[27];
	int all = binary[26];
	int id = binToDecRev(binary, 0, 25);

	x = snprintf((*message), 255, "{\"id\":%d,", id);
	if(all == 1) {
		x += snprintf(&(*message)[x], 255-x, "\"all\":1,");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"unit\":%d,", unit);
	}

	if(state == 1) {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"dusk\"");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"dawn\"");
	}
	x += snprintf(&(*message)[x], 255-x, "}");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void arctechDuskInit(void) {

	protocol_register(&arctech_dusk);
	protocol_set_id(arctech_dusk, "arctech_dusk");
	protocol_device_add(arctech_dusk, "kaku_dusk", "KlikAanKlikUit Dusk Sensor");
	arctech_dusk->devtype = DUSK;
	arctech_dusk->hwtype = RF433;
	arctech_dusk->minrawlen = RAW_LENGTH;
	arctech_dusk->maxrawlen = RAW_LENGTH;
	arctech_dusk->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	arctech_dusk->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&arctech_dusk->options, "u", "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_dusk->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
	options_add(&arctech_dusk->options, "t", "dusk", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_dusk->options, "f", "dawn", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	arctech_dusk->parseCode=&parseCode;
	arctech_dusk->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "arctech_dusk";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	arctechDuskInit();
}
#endif
