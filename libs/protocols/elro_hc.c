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
#include "elro_hc.h"

void elroHCCreateMessage(int systemcode, int unitcode, int state) {
	elro_hc->message = json_mkobject();
	json_append_member(elro_hc->message, "systemcode", json_mknumber(systemcode));
	json_append_member(elro_hc->message, "unitcode", json_mknumber(unitcode));
	if(state == 0) {
		json_append_member(elro_hc->message, "state", json_mkstring("on"));
	} else {
		json_append_member(elro_hc->message, "state", json_mkstring("off"));
	}
}

void elroHCParseBinary(void) {
	int x = 0;
	for(x=0;x<elro_hc->binlen;x++) {
		elro_hc->binary[x] ^= 1;
	}
	int systemcode = binToDecRev(elro_hc->binary, 0, 4);
	int unitcode = binToDecRev(elro_hc->binary, 5, 9);
	int state = elro_hc->binary[11];
	elroHCCreateMessage(systemcode, unitcode, state);
}

void elroHCCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_hc->raw[i]=(elro_hc->plslen->length);
		elro_hc->raw[i+1]=(elro_hc->pulse*elro_hc->plslen->length);
		elro_hc->raw[i+2]=(elro_hc->pulse*elro_hc->plslen->length);
		elro_hc->raw[i+3]=(elro_hc->plslen->length);
	}
}

void elroHCCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_hc->raw[i]=(elro_hc->plslen->length);
		elro_hc->raw[i+1]=(elro_hc->pulse*elro_hc->plslen->length);
		elro_hc->raw[i+2]=(elro_hc->plslen->length);
		elro_hc->raw[i+3]=(elro_hc->pulse*elro_hc->plslen->length);
	}
}
void elroHCClearCode(void) {
	elroHCCreateHigh(0,47);
}

void elroHCCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHCCreateLow(19-(x+3), 19-x);
		}
	}
}

void elroHCCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHCCreateLow(39-(x+3), 39-x);
		}
	}
}

void elroHCCreateState(int state) {
	if(state == 0) {	
		elroHCCreateLow(44, 47);
		elroHCCreateLow(40, 43);
	} else {
		elroHCCreateLow(40, 43);
	}
}

void elroHCCreateFooter(void) {
	elro_hc->raw[48]=(elro_hc->plslen->length);
	elro_hc->raw[49]=(PULSE_DIV*elro_hc->plslen->length);
}

int elroHCCreateCode(JsonNode *code) {
	int systemcode = -1;
	int unitcode = -1;
	int state = -1;
	char *tmp;

	if(json_find_string(code, "systemcode", &tmp) == 0) {
		systemcode=atoi(tmp);
	}
	if(json_find_string(code, "unitcode", &tmp) == 0) {
		unitcode = atoi(tmp);
	}
	if(json_find_string(code, "off", &tmp) == 0) {
		state=1;
	} else if(json_find_string(code, "on", &tmp) == 0) {
		state=0;
	}

	if(systemcode == -1 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "elro_hc: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "elro_hc: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_hc: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		elroHCCreateMessage(systemcode, unitcode, state);
		elroHCClearCode();
		elroHCCreateSystemCode(systemcode);
		elroHCCreateUnitCode(unitcode);
		elroHCCreateState(state);
		elroHCCreateFooter();
	}
	return EXIT_SUCCESS;
}

void elroHCPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

void elroHCInit(void) {

	protocol_register(&elro_hc);
	protocol_set_id(elro_hc, "elro_hc");
	protocol_device_add(elro_hc, "elro_hc", "Elro Home Control Switches");
	protocol_plslen_add(elro_hc, 296);
	elro_hc->devtype = SWITCH;
	elro_hc->hwtype = RX433;
	elro_hc->pulse = 3;
	elro_hc->rawlen = 50;
	elro_hc->binlen = 12;
	elro_hc->lsb = 3;

	options_add(&elro_hc->options, 's', "systemcode", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_hc->options, 'u', "unitcode", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_hc->options, 't', "on", no_value, config_state, NULL);
	options_add(&elro_hc->options, 'f', "off", no_value, config_state, NULL);

	protocol_setting_add_string(elro_hc, "states", "on,off");	
	protocol_setting_add_number(elro_hc, "readonly", 0);
	
	elro_hc->parseBinary=&elroHCParseBinary;
	elro_hc->createCode=&elroHCCreateCode;
	elro_hc->printHelp=&elroHCPrintHelp;
}
