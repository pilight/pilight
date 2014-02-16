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

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "silvercrest.h"

void silvercrestCreateMessage(int systemcode, int unitcode, int state) {
	silvercrest->message = json_mkobject();
	json_append_member(silvercrest->message, "systemcode", json_mknumber(systemcode));
	json_append_member(silvercrest->message, "unitcode", json_mknumber(unitcode));
	if(state == 0) {
		json_append_member(silvercrest->message, "state", json_mkstring("on"));
	} else {
		json_append_member(silvercrest->message, "state", json_mkstring("off"));
	}
}

void silvercrestParseBinary(void) {
	int systemcode = binToDec(silvercrest->binary, 0, 4);
	int unitcode = binToDec(silvercrest->binary, 5, 9);
	int state = silvercrest->binary[11];
	silvercrestCreateMessage(systemcode, unitcode, state);
}

void silvercrestCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		silvercrest->raw[i]=(silvercrest->plslen->length);
		silvercrest->raw[i+1]=(silvercrest->pulse*silvercrest->plslen->length);
		silvercrest->raw[i+2]=(silvercrest->pulse*silvercrest->plslen->length);
		silvercrest->raw[i+3]=(silvercrest->plslen->length);
	}
}

void silvercrestCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		silvercrest->raw[i]=(silvercrest->plslen->length);
		silvercrest->raw[i+1]=(silvercrest->pulse*silvercrest->plslen->length);
		silvercrest->raw[i+2]=(silvercrest->plslen->length);
		silvercrest->raw[i+3]=(silvercrest->pulse*silvercrest->plslen->length);
	}
}
void silvercrestClearCode(void) {
	silvercrestCreateLow(0,47);
}

void silvercrestCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			silvercrestCreateHigh(x, x+3);
		}
	}
}

void silvercrestCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			silvercrestCreateHigh(20+x, 20+x+3);
		}
	}
}

void silvercrestCreateState(int state) {
	if(state == 1) {
		silvercrestCreateHigh(44, 47);
	}
}

void silvercrestCreateFooter(void) {
	silvercrest->raw[48]=(silvercrest->plslen->length);
	silvercrest->raw[49]=(PULSE_DIV*silvercrest->plslen->length);
}

int silvercrestCreateCode(JsonNode *code) {
	int systemcode = -1;
	int unitcode = -1;
	int state = -1;
	int tmp;

	json_find_number(code, "systemcode", &systemcode);
	json_find_number(code, "unitcode", &unitcode);
	if(json_find_number(code, "off", &tmp) == 0)
		state=1;
	else if(json_find_number(code, "on", &tmp) == 0)
		state=0;

	if(systemcode == -1 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "silvercrest: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "silvercrest: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "silvercrest: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		silvercrestCreateMessage(systemcode, unitcode, state);
		silvercrestClearCode();
		silvercrestCreateSystemCode(systemcode);
		silvercrestCreateUnitCode(unitcode);
		silvercrestCreateState(state);
		silvercrestCreateFooter();
	}
	return EXIT_SUCCESS;
}

void silvercrestPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

void silvercrestInit(void) {

	protocol_register(&silvercrest);
	protocol_set_id(silvercrest, "silvercrest");
	protocol_device_add(silvercrest, "silvercrest", "Silvercrest Switches");
	protocol_conflict_add(silvercrest, "mumbi");
	protocol_plslen_add(silvercrest, 312);
	silvercrest->devtype = SWITCH;
	silvercrest->hwtype = RF433;
	silvercrest->pulse = 3;
	silvercrest->rawlen = 50;
	silvercrest->binlen = 12;
	silvercrest->lsb = 3;

	options_add(&silvercrest->options, 's', "systemcode", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&silvercrest->options, 'u', "unitcode", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&silvercrest->options, 't', "on", no_value, config_state, NULL);
	options_add(&silvercrest->options, 'f', "off", no_value, config_state, NULL);

	protocol_setting_add_string(silvercrest, "states", "on,off");	
	protocol_setting_add_number(silvercrest, "readonly", 0);
	
	silvercrest->parseBinary=&silvercrestParseBinary;
	silvercrest->createCode=&silvercrestCreateCode;
	silvercrest->printHelp=&silvercrestPrintHelp;
}
