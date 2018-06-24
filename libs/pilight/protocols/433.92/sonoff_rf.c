/*
	Copyright (C) 2014 CurlyMo

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
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "sonoff_rf.h"

#define AVG_PULSE_LENGTH 500
#define GAP_MIN_LENGTH 7800
#define GAP_MAX_LENGTH 8200
#define RAW_LENGTH 50

static int validate(void) {
	if(sonoff_rf->rawlen == RAW_LENGTH) {
		if(sonoff_rf->raw[RAW_LENGTH-1] >= GAP_MIN_LENGTH) {
			return 0;
		}
	}
	return -1;
}

/*
example raw code:
212 815 724 320 715 327 710 329 703 322 196 833 712 335 183 844 704 323 711 321 198 836 194 834 199 827 726 325 183 837 195 826 723 317 196 834 713 324 193 836 196 834 195 828 720 320 197 828 206 7996
*/

static void parseCode(void) {
	int binary[RAW_LENGTH/2];
	int id = 0;
	int buttons=0;
	int x = 0, i = 0;
	if(sonoff_rf->rawlen!=RAW_LENGTH) {
		logprintf(LOG_ERR, "sonoff_rf: parsecode - invalid parameter passed %d", sonoff_rf->rawlen);
		return;
	}
	for(x=0;x<RAW_LENGTH;x+=2) {
		if(sonoff_rf->raw[x] < AVG_PULSE_LENGTH) {
			binary[i++] = 0;
		} else {
			binary[i++] = 1;
		} 
	}
	id = binToDec(binary, 0, 19);
	id = id & 0xFFFFF;
	buttons = binToDec(binary, 20, 23);
	buttons = buttons & 0xF;
	sonoff_rf->message = json_mkobject();
	json_append_member(sonoff_rf->message, "id", json_mknumber(id, 0));
	json_append_member(sonoff_rf->message, "buttonmask", json_mknumber(buttons, 0));
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void sonoffRfInit(void) {
	protocol_register(&sonoff_rf);
	protocol_set_id(sonoff_rf, "sonoff_rf_sensor");
	protocol_device_add(sonoff_rf, "sonoff_rf_sensor", "sonoff remote control");
	sonoff_rf->devtype = SWITCH;
	sonoff_rf->hwtype = RF433;
	sonoff_rf->maxgaplen = GAP_MAX_LENGTH;
	sonoff_rf->mingaplen = GAP_MIN_LENGTH;
	sonoff_rf->minrawlen = RAW_LENGTH;
	sonoff_rf->maxrawlen = RAW_LENGTH;

	options_add(&sonoff_rf->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]{1-4}$");

	sonoff_rf->parseCode=&parseCode;
	sonoff_rf->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "sonoff_rf_sensor";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "38";
}

void init(void) {
	secudoSmokeInit();
}
#endif
