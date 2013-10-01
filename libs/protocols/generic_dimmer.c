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

#include "settings.h"
#include "log.h"
#include "protocol.h"
#include "binary.h"
#include "gc.h"
#include "generic_dimmer.h"

void genDimCreateMessage(int id, int state, int dimlevel) {
	generic_dimmer->message = json_mkobject();
	json_append_member(generic_dimmer->message, "id", json_mknumber(id));
	if(dimlevel >= 0) {
		state = 1;
		json_append_member(generic_dimmer->message, "dimlevel", json_mknumber(dimlevel));
	}
	if(state == 1) {
		json_append_member(generic_dimmer->message, "state", json_mkstring("on"));
	} else {
		json_append_member(generic_dimmer->message, "state", json_mkstring("off"));
	}
}

int genDimCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	int dimlevel = -1;
	int max = 0;
	int min = 10;
	char *tmp;

	protocol_setting_get_number(generic_dimmer, "min", &min);
	protocol_setting_get_number(generic_dimmer, "max", &max);	

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "dimlevel", &tmp) == 0)
		dimlevel = atoi(tmp);		
	if(json_find_string(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_string(code, "on", &tmp) == 0)
		state=1;

	if(id == -1 || (dimlevel == -1 && state == -1)) {
		logprintf(LOG_ERR, "generic_dimmer: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(dimlevel != -1 && (dimlevel > max || dimlevel < min)) {
		logprintf(LOG_ERR, "arctech_dimmer: invalid dimlevel range");
		return EXIT_FAILURE;		
	} else if(dimlevel >= 0 && state == 0) {
		logprintf(LOG_ERR, "generic_dimmer: dimlevel and state cannot be combined");
		return EXIT_FAILURE;
	} else {
		if(dimlevel >= 0) {
			state = -1;
		}
		genDimCreateMessage(id, state, dimlevel);
	}
	return EXIT_SUCCESS;

}

void genDimPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -d --dimlevel=dimlevel\t\tsend a specific dimlevel\n");	
}

void genDimInit(void) {
	
	protocol_register(&generic_dimmer);
	generic_dimmer->id = malloc(15);
	strcpy(generic_dimmer->id, "generic_dimmer");
	protocol_device_add(generic_dimmer, "generic_dimmer", "Generic dimmers");
	generic_dimmer->type = DIMMER;

	options_add(&generic_dimmer->options, 'd', "dimlevel", has_value, config_value, "^([0-9]{1,})$");
	options_add(&generic_dimmer->options, 't', "on", no_value, config_state, NULL);
	options_add(&generic_dimmer->options, 'f', "off", no_value, config_state, NULL);
	options_add(&generic_dimmer->options, 'i', "id", has_value, config_id, "^([0-9]{1,})$");

	protocol_setting_add_number(generic_dimmer, "min", 0);
	protocol_setting_add_number(generic_dimmer, "max", 10);	
	protocol_setting_add_string(generic_dimmer, "states", "on,off");
	protocol_setting_add_number(generic_dimmer, "readonly", 1);

	generic_dimmer->printHelp=&genDimPrintHelp;
	generic_dimmer->createCode=&genDimCreateCode;
}
