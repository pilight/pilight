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
#include "conrad_rsl_switch.h"

void conradRSLSwCreateMessage(int id, int state) {
	conrad_rsl_switch->message = json_mkobject();
	json_append_member(conrad_rsl_switch->message, "id", json_mknumber(id));
	if(state == 1) {
		json_append_member(conrad_rsl_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(conrad_rsl_switch->message, "state", json_mkstring("off"));
	}
}

void conradRSLSwParseCode(void) {
	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<conrad_rsl_switch->rawlen; x+=2) {
		if(conrad_rsl_switch->code[x+1] == 1) {
			conrad_rsl_switch->binary[x/2]=1;
		} else {
			conrad_rsl_switch->binary[x/2]=0;
		}
	}

	int id = binToDecRev(conrad_rsl_switch->binary, 6, 31);
	int check = binToDecRev(conrad_rsl_switch->binary, 0, 3);
	int check1 = conrad_rsl_switch->binary[32];
	int state1 = conrad_rsl_switch->binary[4];
	int state2 = conrad_rsl_switch->binary[5];

	if(check == 4 && check1 == 1 && state1 != state2) {
		conradRSLSwCreateMessage(id, state2);
	}
}

void conradRSLSwCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		conrad_rsl_switch->raw[i]=((conrad_rsl_switch->pulse+1)*conrad_rsl_switch->plslen->length);
		conrad_rsl_switch->raw[i+1]=conrad_rsl_switch->plslen->length*2;
	}
}

void conradRSLSwCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		conrad_rsl_switch->raw[i]=conrad_rsl_switch->plslen->length*2;
		conrad_rsl_switch->raw[i+1]=((conrad_rsl_switch->pulse+1)*conrad_rsl_switch->plslen->length);
	}
}

void conradRSLSwClearCode(void) {
	conradRSLSwCreateLow(0,65);
}

void conradRSLSwCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			conradRSLSwCreateHigh(12+x, 12+x+1);
		}
	}
}

void conradRSLSwCreateStart(int start) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(start, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			conradRSLSwCreateHigh(2+x, 2+x+1);
		}
	}
}

void conradRSLSwCreateState(int state) {
	if(state == 1) {
		conradRSLSwCreateHigh(10, 11);
	} else {
		conradRSLSwCreateHigh(8, 9);
	}
}

void conradRSLSwCreateFooter(void) {
	conrad_rsl_switch->raw[64]=(conrad_rsl_switch->plslen->length);
	conrad_rsl_switch->raw[65]=(PULSE_DIV*conrad_rsl_switch->plslen->length);
}

int conradRSLSwCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	int tmp;

	json_find_number(code, "id", &id);
	if(json_find_number(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &tmp) == 0)
		state=1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "conrad_rsl_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 67108863 || id < 0) {
		logprintf(LOG_ERR, "conrad_rsl_switch: invalid programcode range");
		return EXIT_FAILURE;
	} else {
		conradRSLSwCreateMessage(id, state);
		conradRSLSwClearCode();
		conradRSLSwCreateStart(4);
		conradRSLSwCreateId(id);
		conradRSLSwCreateState(state);
		conradRSLSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

void conradRSLSwPrintHelp(void) {
	printf("\t -i --id=id\tcontrol a device with this id\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

void conradRSLSwInit(void) {

	protocol_register(&conrad_rsl_switch);
	protocol_set_id(conrad_rsl_switch, "conrad_rsl_switch");
	protocol_device_add(conrad_rsl_switch, "conrad_rsl_switch", "Conrad RSL Switches");
	protocol_plslen_add(conrad_rsl_switch, 204);
	conrad_rsl_switch->devtype = SWITCH;
	conrad_rsl_switch->hwtype = RF433;
	conrad_rsl_switch->pulse = 5;
	conrad_rsl_switch->rawlen = 66;
	conrad_rsl_switch->binlen = 33;

	options_add(&conrad_rsl_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(([0-9]|([1-9][0-9])|([1-9][0-9]{2})|([1-9][0-9]{3})|([1-9][0-9]{4})|([1-9][0-9]{5})|([1-9][0-9]{6})|((6710886[0-3])|(671088[0-5][0-9])|(67108[0-7][0-9]{2})|(6710[0-7][0-9]{3})|(671[0--1][0-9]{4})|(670[0-9]{5})|(6[0-6][0-9]{6})|(0[0-5][0-9]{7}))))$");
	options_add(&conrad_rsl_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&conrad_rsl_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&conrad_rsl_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	conrad_rsl_switch->parseCode=&conradRSLSwParseCode;
	conrad_rsl_switch->createCode=&conradRSLSwCreateCode;
	conrad_rsl_switch->printHelp=&conradRSLSwPrintHelp;
}
