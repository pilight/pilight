/*
	Copyright (C) 2013 CurlyMo & Bram1337

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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "conrad_rsl_contact.h"

static void conradRSLCnCreateMessage(int id, int state) {
	conrad_rsl_contact->message = json_mkobject();
	json_append_member(conrad_rsl_contact->message, "id", json_mknumber(id));
	if(state == 1) {
		json_append_member(conrad_rsl_contact->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(conrad_rsl_contact->message, "state", json_mkstring("closed"));
	}
}

static void conradRSLCnParseCode(void) {
	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<conrad_rsl_contact->rawlen; x+=2) {
		if(conrad_rsl_contact->code[x+1] == 1) {
			conrad_rsl_contact->binary[x/2]=1;
		} else {
			conrad_rsl_contact->binary[x/2]=0;
		}
	}

	int id = binToDecRev(conrad_rsl_contact->binary, 6, 31);
	int check = binToDecRev(conrad_rsl_contact->binary, 0, 3);
	int check1 = conrad_rsl_contact->binary[32];
	int state = conrad_rsl_contact->binary[4];

	if(check == 5 && check1 == 1) {
		conradRSLCnCreateMessage(id, state);
	}
}

#ifndef MODULE
__attribute__((weak))
#endif
void conradRSLCnInit(void) {

	protocol_register(&conrad_rsl_contact);
	protocol_set_id(conrad_rsl_contact, "conrad_rsl_contact");
	protocol_device_add(conrad_rsl_contact, "conrad_rsl_contact", "Conrad RSL Contact Sensor");
	protocol_plslen_add(conrad_rsl_contact, 190);
	conrad_rsl_contact->devtype = SWITCH;
	conrad_rsl_contact->hwtype = RF433;
	conrad_rsl_contact->pulse = 5;
	conrad_rsl_contact->rawlen = 66;
	conrad_rsl_contact->binlen = 33;

	options_add(&conrad_rsl_contact->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(([0-9]|([1-9][0-9])|([1-9][0-9]{2})|([1-9][0-9]{3})|([1-9][0-9]{4})|([1-9][0-9]{5})|([1-9][0-9]{6})|((6710886[0-3])|(671088[0-5][0-9])|(67108[0-7][0-9]{2})|(6710[0-7][0-9]{3})|(671[0--1][0-9]{4})|(670[0-9]{5})|(6[0-6][0-9]{6})|(0[0-5][0-9]{7}))))$");
	options_add(&conrad_rsl_contact->options, 't', "opened", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&conrad_rsl_contact->options, 'f', "closed", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&conrad_rsl_contact->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	conrad_rsl_contact->parseCode=&conradRSLCnParseCode;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "conrad_rsl_contact";
	module->version = "0.1";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	conradRSLCnInit();
}
#endif
