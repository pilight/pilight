/*
	Copyright (C) 2017 CurlyMo & wo-rasp

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
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "../protocol.h"
#include "iwds07.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	415
#define MAX_PULSE_LENGTH	1245
#define AVG_PULSE_LENGTH	830
#define FOOTER						14110
#define RAW_LENGTH				50

static int validate(void) {
	if(iwds07->rawlen == RAW_LENGTH) {
		if(iwds07->raw[iwds07->rawlen-1] >= (FOOTER*0.9) &&
			iwds07->raw[iwds07->rawlen-1] <= (FOOTER*1.1)) {
			return 0;
		}
	}
	return -1;
}

static void createMessage(int unit, int battery, int state) {
	iwds07->message=json_mkobject();
	json_append_member(iwds07->message, "unit", json_mknumber(unit, 0));
	json_append_member(iwds07->message, "battery", json_mknumber(battery, 0));
	if(state == 1) {
		json_append_member(iwds07->message, "state", json_mkstring("closed"));
	} else {
		json_append_member(iwds07->message, "state", json_mkstring("opened"));
	}
}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], i=0, x=0;
	int unit=0, battery=-1, state=-1;

	if(iwds07->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "iwds07: parsecode - invalid parameter passed %d", iwds07->rawlen);
		return;
	}

	for(x=0;x<iwds07->rawlen-2;x+=2) {
		if(iwds07->raw[x] < AVG_PULSE_LENGTH) {
			binary[i++]=0;
		} else {
			binary[i++]=1;
		}
	}

	unit = binToDec(binary, 0, 19);
	battery = binToDec(binary, 20, 20);
	state = binToDec(binary, 21, 21);
	createMessage(unit, battery, state);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void iwds07Init(void) {
	protocol_register(&iwds07);
	protocol_set_id(iwds07, "GS-iwds07");
	protocol_device_add(iwds07, "GS-iwds07", "lightinthebox GS-iwds07 contacts");
	iwds07->devtype = CONTACT;
	iwds07->hwtype = RF433;
	iwds07->minrawlen = RAW_LENGTH;
	iwds07->maxrawlen = RAW_LENGTH;
	iwds07->maxgaplen = FOOTER-200;
	iwds07->mingaplen = FOOTER+200;

	options_add(&iwds07->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&iwds07->options, 'b', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");
	options_add(&iwds07->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&iwds07->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&iwds07->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	iwds07->parseCode=&parseCode;
	iwds07->validate=&validate;
}
#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "iwds07";
	module->version = "1.00";
	module->reqversion = "7.0";
	module->reqcommit = "100";
}

void init(void) {
	iwds07Init();
}
#endif
