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
#include "impuls.h"

void impulsCreateMessage(int systemcode, int programcode, int state) {
	impuls->message = json_mkobject();
	json_append_member(impuls->message, "systemcode", json_mknumber(systemcode));
	json_append_member(impuls->message, "programcode", json_mknumber(programcode));
	if(state == 1) {
		json_append_member(impuls->message, "state", json_mkstring("on"));
	} else {
		json_append_member(impuls->message, "state", json_mkstring("off"));
	}
}

void impulsParseCode(void) {
	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<impuls->rawlen; x+=4) {
		if(impuls->code[x+3] == 1 || impuls->code[x+0] == 1) {
			impuls->binary[x/4]=1;
		} else {
			impuls->binary[x/4]=0;
		}
	}

	int systemcode = binToDec(impuls->binary, 0, 4);
	int programcode = binToDec(impuls->binary, 5, 9);
	int check = impuls->binary[10];
	int state = impuls->binary[11];

	if(check != state) {
		impulsCreateMessage(systemcode, programcode, state);
	}
}

void impulsCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		impuls->raw[i]=impuls->plslen->length;
		impuls->raw[i+1]=(impuls->pulse*impuls->plslen->length);
		impuls->raw[i+2]=(impuls->pulse*impuls->plslen->length);
		impuls->raw[i+3]=impuls->plslen->length;
	}
}

void impulsCreateMed(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		impuls->raw[i]=(impuls->pulse*impuls->plslen->length);
		impuls->raw[i+1]=impuls->plslen->length;
		impuls->raw[i+2]=(impuls->pulse*impuls->plslen->length);
		impuls->raw[i+3]=impuls->plslen->length;
	}
}

void impulsCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		impuls->raw[i]=impuls->plslen->length;
		impuls->raw[i+1]=(impuls->pulse*impuls->plslen->length);
		impuls->raw[i+2]=impuls->plslen->length;
		impuls->raw[i+3]=(impuls->pulse*impuls->plslen->length);
	}
}

void impulsClearCode(void) {
	impulsCreateLow(0,47);
}

void impulsCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			impulsCreateMed(x, x+3);
		}
	}
}

void impulsCreateProgramCode(int programcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(programcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			impulsCreateHigh(20+x, 20+x+3);
		}
	}
}

void impulsCreateState(int state) {
	if(state == 0) {
		impulsCreateHigh(40, 43);
	} else {
		impulsCreateHigh(44, 47);
	}
}

void impulsCreateFooter(void) {
	impuls->raw[48]=(impuls->plslen->length);
	impuls->raw[49]=(PULSE_DIV*impuls->plslen->length);
}

int impulsCreateCode(JsonNode *code) {
	int systemcode = -1;
	int programcode = -1;
	int state = -1;
	int tmp;

	json_find_number(code, "systemcode", &systemcode);
	json_find_number(code, "programcode", &programcode);
	if(json_find_number(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &tmp) == 0)
		state=1;

	if(systemcode == -1 || programcode == -1 || state == -1) {
		logprintf(LOG_ERR, "impuls: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "impuls: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(programcode > 31 || programcode < 0) {
		logprintf(LOG_ERR, "impuls: invalid programcode range");
		return EXIT_FAILURE;
	} else {
		impulsCreateMessage(systemcode, programcode, state);
		impulsClearCode();
		impulsCreateSystemCode(systemcode);
		impulsCreateProgramCode(programcode);
		impulsCreateState(state);
		impulsCreateFooter();
	}
	return EXIT_SUCCESS;
}

void impulsPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --programcode=programcode\tcontrol a device with this programcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

void impulsInit(void) {

	protocol_register(&impuls);
	protocol_set_id(impuls, "impuls");
	protocol_device_add(impuls, "impuls", "Impuls Switches");
	protocol_device_add(impuls, "select-remote", "SelectRemote Switches");
	protocol_conflict_add(impuls, "rev_switch");
	protocol_plslen_add(impuls, 171);
	protocol_plslen_add(impuls, 141);
	protocol_plslen_add(impuls, 133);
	impuls->devtype = SWITCH;
	impuls->hwtype = RF433;
	impuls->pulse = 3;
	impuls->rawlen = 50;
	impuls->binlen = 12;

	options_add(&impuls->options, 's', "systemcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&impuls->options, 'u', "programcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&impuls->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL);
	options_add(&impuls->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL);

	protocol_setting_add_string(impuls, "states", "on,off");
	protocol_setting_add_number(impuls, "readonly", 0);

	impuls->parseCode=&impulsParseCode;
	impuls->createCode=&impulsCreateCode;
	impuls->printHelp=&impulsPrintHelp;
}
