/*
	Copyright (C) 2013 CurlyMo

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
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "generic_switch.h"

void genSwitchCreateMessage(int id, int state) {
	generic_switch->message = json_mkobject();
	json_append_member(generic_switch->message, "id", json_mknumber(id));
	if(state == 1) {
		json_append_member(generic_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(generic_switch->message, "state", json_mkstring("off"));
	}
}

int genSwitchCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_string(code, "on", &tmp) == 0)
		state=1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "generic_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		genSwitchCreateMessage(id, state);
	}

	return EXIT_SUCCESS;

}

void genSwitchPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void genSwitchInit(void) {

	protocol_register(&generic_switch);
	protocol_set_id(generic_switch, "generic_switch");
	protocol_device_add(generic_switch, "generic_switch", "Generic switches");
	generic_switch->devtype = SWITCH;

	options_add(&generic_switch->options, 't', "on", no_value, config_state, NULL);
	options_add(&generic_switch->options, 'f', "off", no_value, config_state, NULL);
	options_add(&generic_switch->options, 'i', "id", has_value, config_id, "^([0-9]{1,})$");

	protocol_setting_add_string(generic_switch, "states", "on,off");
	protocol_setting_add_number(generic_switch, "readonly", 1);

	generic_switch->printHelp=&genSwitchPrintHelp;
	generic_switch->createCode=&genSwitchCreateCode;
}
