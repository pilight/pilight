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
#include "sartano.h"

void sartanoCreateMessage(int systemcode, int unitcode, int state) {
	sartano->message = json_mkobject();
	json_append_member(sartano->message, "systemcode", json_mknumber(systemcode));
	json_append_member(sartano->message, "unitcode", json_mknumber(unitcode));
	if(state == 0) {
		json_append_member(sartano->message, "state", json_mkstring("on"));
	} else {
		json_append_member(sartano->message, "state", json_mkstring("off"));
	}
}

void sartanoParseBinary(void) {
	int systemcode = binToDec(sartano->binary, 0, 4);
	int unitcode = binToDec(sartano->binary, 5, 9);
	int state = sartano->binary[11];
	sartanoCreateMessage(systemcode, unitcode, state);
}

void sartanoCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		sartano->raw[i]=(sartano->plslen->length);
		sartano->raw[i+1]=(sartano->pulse*sartano->plslen->length);
		sartano->raw[i+2]=(sartano->pulse*sartano->plslen->length);
		sartano->raw[i+3]=(sartano->plslen->length);
	}
}

void sartanoCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		sartano->raw[i]=(sartano->plslen->length);
		sartano->raw[i+1]=(sartano->pulse*sartano->plslen->length);
		sartano->raw[i+2]=(sartano->plslen->length);
		sartano->raw[i+3]=(sartano->pulse*sartano->plslen->length);
	}
}
void sartanoClearCode(void) {
	sartanoCreateLow(0,47);
}

void sartanoCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			sartanoCreateHigh(x, x+3);
		}
	}
}

void sartanoCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			sartanoCreateHigh(20+x, 20+x+3);
		}
	}
}

void sartanoCreateState(int state) {
	if(state == 1) {
		sartanoCreateHigh(44, 47);
	}
}

void sartanoCreateFooter(void) {
	sartano->raw[48]=(sartano->plslen->length);
	sartano->raw[49]=(PULSE_DIV*sartano->plslen->length);
}

int sartanoCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "sartano: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "sartano: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "sartano: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		sartanoCreateMessage(systemcode, unitcode, state);
		sartanoClearCode();
		sartanoCreateSystemCode(systemcode);
		sartanoCreateUnitCode(unitcode);
		sartanoCreateState(state);
		sartanoCreateFooter();
	}
	return EXIT_SUCCESS;
}

void sartanoPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

void sartanoInit(void) {

	protocol_register(&sartano);
	protocol_set_id(sartano, "sartano");
	protocol_device_add(sartano, "elro", "Elro Switches");
	protocol_plslen_add(sartano, 291);
	sartano->devtype = SWITCH;
	sartano->hwtype = RX433;
	sartano->pulse = 3;
	sartano->rawlen = 50;
	sartano->binlen = 12;
	sartano->lsb = 3;

	options_add(&sartano->options, 's', "systemcode", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&sartano->options, 'u', "unitcode", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&sartano->options, 't', "on", no_value, config_state, NULL);
	options_add(&sartano->options, 'f', "off", no_value, config_state, NULL);

	protocol_setting_add_string(sartano, "states", "on,off");	
	protocol_setting_add_number(sartano, "readonly", 0);
	
	sartano->parseBinary=&sartanoParseBinary;
	sartano->createCode=&sartanoCreateCode;
	sartano->printHelp=&sartanoPrintHelp;
}
