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

#include "pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "elro_800_contact.h"

static void elro800ContactCreateMessage(int systemcode, int unitcode, int state) {
	elro_800_contact->message = json_mkobject();
	json_append_member(elro_800_contact->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(elro_800_contact->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(elro_800_contact->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(elro_800_contact->message, "state", json_mkstring("closed"));
	}
}

static void elro800ContactParseBinary(void) {
	int systemcode = binToDec(elro_800_contact->binary, 0, 4);
	int unitcode = binToDec(elro_800_contact->binary, 5, 9);
	int state = elro_800_contact->binary[11];
	elro800ContactCreateMessage(systemcode, unitcode, state);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void elro800ContactInit(void) {

	protocol_register(&elro_800_contact);
	protocol_set_id(elro_800_contact, "elro_800_contact");
	protocol_device_add(elro_800_contact, "elro_800_contact", "Elro Series 800 Contact");
	protocol_plslen_add(elro_800_contact, 288);
	protocol_plslen_add(elro_800_contact, 300);
	elro_800_contact->devtype = CONTACT;
	elro_800_contact->hwtype = RF433;
	elro_800_contact->pulse = 3;
	elro_800_contact->rawlen = 50;
	elro_800_contact->binlen = 12;
	elro_800_contact->lsb = 3;

	options_add(&elro_800_contact->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_800_contact->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_800_contact->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_800_contact->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_800_contact->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_800_contact->parseBinary=&elro800ContactParseBinary;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "elro_800_contact";
	module->version = "1.6";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	elro800ContactInit();
}
#endif
