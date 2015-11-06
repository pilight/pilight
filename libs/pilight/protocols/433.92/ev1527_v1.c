/*
	Copyright (C) 2015 CurlyMo and Meloen

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
#include "ev1527_v1.h"

#define PULSE_MULTIPLIER	5
#define MIN_PULSE_LENGTH	290
#define MAX_PULSE_LENGTH	330
#define AVG_PULSE_LENGTH	310
#define RAW_LENGTH				50

static int validate(void) {
	printf("raw len %d\n", ev1527_v1->rawlen);
	printf("raw %d\n", ev1527_v1->raw[ev1527_v1->rawlen-1]);
	if(ev1527_v1->rawlen == RAW_LENGTH) {
		if(ev1527_v1->raw[ev1527_v1->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   ev1527_v1->raw[ev1527_v1->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int unitcode, int state) {
	ev1527_v1->message = json_mkobject();
	json_append_member(ev1527_v1->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(ev1527_v1->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(ev1527_v1->message, "state", json_mkstring("closed"));
	}
}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], x = 0, i = 0;

	for(x=0;x<ev1527_v1->rawlen-2;x+=2) {
		if(ev1527_v1->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
			printf("0");
		} else {
			binary[i++] = 0;
			printf("1");
		}
	}
	printf("\n");
	int unitcode = binToDec(binary, 0, 18);
	int state = binary[19];
//	createMessage(unitcode, state);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void ev1527Init_v1(void) {

	protocol_register(&ev1527_v1);
	protocol_set_id(ev1527_v1, "ev1527_v1");
	protocol_device_add(ev1527_v1, "ev1527_v1", "ev1527_v1 contact sensor");
	ev1527_v1->devtype = CONTACT;
	ev1527_v1->hwtype = RF433;
	ev1527_v1->minrawlen = RAW_LENGTH;
	ev1527_v1->maxrawlen = RAW_LENGTH;
	ev1527_v1->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	ev1527_v1->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&ev1527_v1->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&ev1527_v1->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ev1527_v1->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	ev1527_v1->parseCode=&parseCode;
	ev1527_v1->validate=&validate;
}

#if !defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ev1527_v1";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	ev1527Init_v1();
}
#endif
