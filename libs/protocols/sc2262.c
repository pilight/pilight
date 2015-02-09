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
#include "sc2262.h"

static void sc2262CreateMessage(int systemcode, int unitcode, int state) {
	sc2262->message = json_mkobject();
	json_append_member(sc2262->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(sc2262->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(sc2262->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(sc2262->message, "state", json_mkstring("closed"));
	}
}

static void sc2262ParseBinary(void) {
	int systemcode = binToDec(sc2262->binary, 0, 4);
	int unitcode = binToDec(sc2262->binary, 5, 9);
	int state = sc2262->binary[11];
	sc2262CreateMessage(systemcode, unitcode, state);
}

#ifndef MODULE
__attribute__((weak))
#endif
void sc2262Init(void) {

	protocol_register(&sc2262);
	protocol_set_id(sc2262, "sc2262");
	protocol_device_add(sc2262, "sc2262", "sc2262 contact sensor");
	protocol_plslen_add(sc2262, 432);
	sc2262->devtype = CONTACT;
	sc2262->hwtype = RF433;
	sc2262->pulse = 3;
	sc2262->rawlen = 50;
	sc2262->binlen = 12;
	sc2262->lsb = 3;

	options_add(&sc2262->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&sc2262->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&sc2262->options, 't', "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&sc2262->options, 'f', "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&sc2262->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	sc2262->parseBinary=&sc2262ParseBinary;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "sc2262";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "99";
}

void init(void) {
	sc2262Init();
}
#endif
