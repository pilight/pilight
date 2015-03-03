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

#include "pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "ev1527.h"

static void ev1527CreateMessage(int unitcode, int state) {
	ev1527->message = json_mkobject();
	json_append_member(ev1527->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(ev1527->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(ev1527->message, "state", json_mkstring("closed"));
	}
}

static void ev1527ParseBinary(void) {

	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<ev1527->rawlen; x+=2) {
		if(ev1527->code[x+1] == 1) {
			ev1527->binary[x/2]=1;
		} else {
			ev1527->binary[x/2]=0;
		}
	}

	int unitcode = binToDec(ev1527->binary, 0, 19);
	int state = ev1527->binary[20];
	ev1527CreateMessage(unitcode, state);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void ev1527Init(void) {

	protocol_register(&ev1527);
	protocol_set_id(ev1527, "ev1527");
	protocol_device_add(ev1527, "ev1527", "ev1527 contact sensor");
	protocol_plslen_add(ev1527, 256);
	protocol_plslen_add(ev1527, 306);
	ev1527->devtype = CONTACT;
	ev1527->hwtype = RF433;
	ev1527->pulse = 3;
	ev1527->rawlen = 50;
	ev1527->binlen = 12;
	
	options_add(&ev1527->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&ev1527->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ev1527->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ev1527->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	ev1527->parseBinary=&ev1527ParseBinary;
}

#if !defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ev1527";
	module->version = "0.3";
	module->reqversion = "6.0";
	module->reqcommit = NULL;
}

void init(void) {
	ev1527Init();
}
#endif
