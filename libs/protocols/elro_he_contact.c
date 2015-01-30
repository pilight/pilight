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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "elro_he_contact.h"

static void elroHEContactCreateMessage(int systemcode, int unitcode, int state) {
	elro_he_contact->message = json_mkobject();
	json_append_member(elro_he_contact->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(elro_he_contact->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(elro_he_contact->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(elro_he_contact->message, "state", json_mkstring("closed"));
	}
}

static void elroHEContactParseBinary(void) {
	int systemcode = binToDec(elro_he_contact->binary, 0, 4);
	int unitcode = binToDec(elro_he_contact->binary, 5, 9);
	int state = elro_he_contact->binary[11];
	elroHEContactCreateMessage(systemcode, unitcode, state);
}

#ifndef MODULE
__attribute__((weak))
#endif
void elroHEContactInit(void) {

	protocol_register(&elro_he_contact);
	protocol_set_id(elro_he_contact, "elro_he_contact");
	protocol_device_add(elro_he_contact, "elro_he_contact", "Elro Home Easy Contact");
	protocol_plslen_add(elro_he_contact, 288);
	protocol_plslen_add(elro_he_contact, 300);
	elro_he_contact->devtype = CONTACT;
	elro_he_contact->hwtype = RF433;
	elro_he_contact->pulse = 3;
	elro_he_contact->rawlen = 50;
	elro_he_contact->binlen = 12;
	elro_he_contact->lsb = 3;

	options_add(&elro_he_contact->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_he_contact->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_he_contact->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_he_contact->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_he_contact->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_he_contact->parseBinary=&elroHEContactParseBinary;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "elro_he_contact";
	module->version = "1.5";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	elroHEContactInit();
}
#endif
