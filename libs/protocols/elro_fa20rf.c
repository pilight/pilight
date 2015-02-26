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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "elro_fa20rf.h"

static void elro_fa20rfCreateMessage(int unitcode, int state) {
	elro_fa20rf->message = json_mkobject();
	json_append_member(elro_fa20rf->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(elro_fa20rf->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(elro_fa20rf->message, "state", json_mkstring("closed"));
	}
}

static void elro_fa20rfParseBinary(void) {

	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<elro_fa20rf->rawlen; x+=2) {
		if(elro_fa20rf->code[x+1] == 1) {
			elro_fa20rf->binary[x/2]=1;
		} else {
			elro_fa20rf->binary[x/2]=0;
		}
	}

	int unitcode = binToDec(elro_fa20rf->binary, 0, 23);
	int state = elro_fa20rf->binary[24];
	elro_fa20rfCreateMessage(unitcode, state);
}

#ifndef MODULE
__attribute__((weak))
#endif
void elro_fa20rfInit(void) {

	protocol_register(&elro_fa20rf);
	protocol_set_id(elro_fa20rf, "elro_fa20rf");
	protocol_device_add(elro_fa20rf, "elro_fa20rf", "elro_fa20rf contact sensor");
	protocol_plslen_add(elro_fa20rf, 392);
	elro_fa20rf->devtype = CONTACT;
	elro_fa20rf->hwtype = RF433;
	elro_fa20rf->pulse = 7;
	elro_fa20rf->rawlen = 52;
	elro_fa20rf->binlen = 12;
	
	options_add(&elro_fa20rf->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_fa20rf->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_fa20rf->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_fa20rf->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_fa20rf->parseBinary=&elro_fa20rfParseBinary;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "elro_fa20rf";
	module->version = "0.1";
	module->reqversion = "6.0";
	module->reqcommit = NULL;
}

void init(void) {
	elro_fa20rfInit();
}
#endif
