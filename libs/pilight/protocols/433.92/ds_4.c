/*
	Copyright (C) 2017 Torwag
	
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
#include "ds_4.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	350
#define MAX_PULSE_LENGTH	550
#define AVG_PULSE_LENGTH	470
#define RAW_LENGTH		50

static int validate(void) {
	if(ds_4->rawlen == RAW_LENGTH) {
		if(ds_4->raw[ds_4->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   ds_4->raw[ds_4->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int unitcode, int state) {
	ds_4->message = json_mkobject();
	json_append_member(ds_4->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 14) {
		json_append_member(ds_4->message, "state", json_mkstring("closed"));
	} else if(state == 10) {
		json_append_member(ds_4->message, "state", json_mkstring("opened"));
	} else if(state == 7) {
	  json_append_member(ds_4->message, "state", json_mkstring("tampered"));
	} else {
	  json_append_member(ds_4->message, "state", json_mknumber(state, 0));
	}
}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], x = 0, i = 0;

	if(ds_4->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "ds_4: parsecode - invalid parameter passed %d", ds_4->rawlen);
		return;
	}

	for(x=0;x<ds_4->rawlen-2;x+=2) {
		if(ds_4->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}
	
	int unitcode = binToDecRev(binary, 0, 15);
	int state = binToDecRev(binary, 20, 23);
	createMessage(unitcode, state);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void ds_4Init(void) {

	protocol_register(&ds_4);
	protocol_set_id(ds_4, "ds_4");
	protocol_device_add(ds_4, "ds_4", "ds_4 contact sensor");
	ds_4->devtype = CONTACT;
	ds_4->hwtype = RF433;
	ds_4->minrawlen = RAW_LENGTH;
	ds_4->maxrawlen = RAW_LENGTH;
	ds_4->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	ds_4->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&ds_4->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&ds_4->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ds_4->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ds_4->options, 't', "tampered", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	ds_4->parseCode=&parseCode;
	ds_4->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ds_4";
	module->version = "0.1";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	ds_4Init();
}
#endif
