/*
	Copyright (C) 2013 CurlyMo & Bram1337

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
#include "conrad_rsl_contact.h"

void conradRSLCnCreateMessage(int id, int state) {
	conrad_rsl_contact->message = json_mkobject();
	json_append_member(conrad_rsl_contact->message, "id", json_mknumber(id));
	if(state == 1) {
		json_append_member(conrad_rsl_contact->message, "state", json_mkstring("on"));
	} else {
		json_append_member(conrad_rsl_contact->message, "state", json_mkstring("off"));
	}
}

void conradRSLCnParseCode(void) {
	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<conrad_rsl_contact->rawlen; x+=2) {
		if(conrad_rsl_contact->code[x+1] == 1) {
			conrad_rsl_contact->binary[x/2]=1;
		} else {
			conrad_rsl_contact->binary[x/2]=0;
		}
	}

	int id = binToDecRev(conrad_rsl_contact->binary, 6, 31);
	int check = binToDecRev(conrad_rsl_contact->binary, 0, 3);
	int check1 = conrad_rsl_contact->binary[32];
	int state = conrad_rsl_contact->binary[4];

	if(check == 5 && check1 == 1) {
		conradRSLCnCreateMessage(id, state);
	}
}

void conradRSLCnCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		conrad_rsl_contact->raw[i]=((conrad_rsl_contact->pulse+1)*conrad_rsl_contact->plslen->length);
		conrad_rsl_contact->raw[i+1]=conrad_rsl_contact->plslen->length*2;
	}
}

void conradRSLCnCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		conrad_rsl_contact->raw[i]=conrad_rsl_contact->plslen->length*2;
		conrad_rsl_contact->raw[i+1]=((conrad_rsl_contact->pulse+1)*conrad_rsl_contact->plslen->length);
	}
}

void conradRSLCnClearCode(void) {
	conradRSLCnCreateLow(0,65);
}

void conradRSLCnCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			conradRSLCnCreateHigh(12+x, 12+x+1);
		}
	}
}

void conradRSLCnCreateStart(int start) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(start, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			conradRSLCnCreateHigh(2+x, 2+x+1);
		}
	}
}

void conradRSLCnCreateState(int state) {
	if(state == 1) {
		conradRSLCnCreateHigh(8, 9);
	}
	conradRSLCnCreateHigh(10, 11);
}

void conradRSLCnCreateFooter(void) {
	conrad_rsl_contact->raw[64]=(conrad_rsl_contact->plslen->length);
	conrad_rsl_contact->raw[65]=(PULSE_DIV*conrad_rsl_contact->plslen->length);
}

int conradRSLCnCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0) {
		id=atoi(tmp);
	}
	if(json_find_string(code, "off", &tmp) == 0) {
		state=0;
	} else if(json_find_string(code, "on", &tmp) == 0) {
		state=1;
	}

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "conrad_rsl_contact: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 67108863 || id < 0) {
		logprintf(LOG_ERR, "conrad_rsl_contact: invalid programcode range");
		return EXIT_FAILURE;
	} else {
		conradRSLCnCreateMessage(id, state);
		conradRSLCnClearCode();
		conradRSLCnCreateStart(5);
		conradRSLCnCreateId(id);
		conradRSLCnCreateState(state);
		conradRSLCnCreateFooter();
	}
	return EXIT_SUCCESS;
}

void conradRSLCnPrintHelp(void) {
	printf("\t -i --id=id\tcontrol a device with this id\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

void conradRSLCnInit(void) {

	protocol_register(&conrad_rsl_contact);
	protocol_set_id(conrad_rsl_contact, "conrad_rsl_contact");
	protocol_device_add(conrad_rsl_contact, "conrad_rsl_contact", "Conrad RSL Contact Sensor");
	protocol_plslen_add(conrad_rsl_contact, 190);
	conrad_rsl_contact->devtype = SWITCH;
	conrad_rsl_contact->hwtype = RX433;
	conrad_rsl_contact->pulse = 5;
	conrad_rsl_contact->rawlen = 66;
	conrad_rsl_contact->binlen = 33;

	options_add(&conrad_rsl_contact->options, 'i', "id", has_value, config_id, "^(([0-9]|([1-9][0-9])|([1-9][0-9]{2})|([1-9][0-9]{3})|([1-9][0-9]{4})|([1-9][0-9]{5})|([1-9][0-9]{6})|((6710886[0-3])|(671088[0-5][0-9])|(67108[0-7][0-9]{2})|(6710[0-7][0-9]{3})|(671[0--1][0-9]{4})|(670[0-9]{5})|(6[0-6][0-9]{6})|(0[0-5][0-9]{7}))))$");
	options_add(&conrad_rsl_contact->options, 't', "on", no_value, config_state, NULL);
	options_add(&conrad_rsl_contact->options, 'f', "off", no_value, config_state, NULL);

	protocol_setting_add_string(conrad_rsl_contact, "states", "on,off");
	protocol_setting_add_number(conrad_rsl_contact, "readonly", 0);

	conrad_rsl_contact->parseCode=&conradRSLCnParseCode;
	conrad_rsl_contact->createCode=&conradRSLCnCreateCode;
	conrad_rsl_contact->printHelp=&conradRSLCnPrintHelp;
}
