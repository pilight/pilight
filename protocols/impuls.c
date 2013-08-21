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

#include "settings.h"
#include "log.h"
#include "protocol.h"
#include "binary.h"
#include "gc.h"
#include "impuls.h"

void impulsCreateMessage(int id, int unit, int state) {
	impuls->message = json_mkobject();
	json_append_member(impuls->message, "id", json_mknumber(id));
	json_append_member(impuls->message, "unit", json_mknumber(unit));
	if(state == 1)
		json_append_member(impuls->message, "state", json_mkstring("on"));
	else
		json_append_member(impuls->message, "state", json_mkstring("off"));
}

void impulsParseBinary(void) {
	impuls->message = NULL;
	int unit = binToDec(impuls->binary, 0, 4);
	int check = impuls->binary[10];
	int state = impuls->binary[11];
	int id = binToDec(impuls->binary, 5, 9);
	if(check != state)
		impulsCreateMessage(id, unit, state);
}

void impulsCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		impuls->raw[i]=(PULSE_LENGTH);
		impuls->raw[i+1]=(impuls->pulse*PULSE_LENGTH);
		impuls->raw[i+2]=(impuls->pulse*PULSE_LENGTH);
		impuls->raw[i+3]=(PULSE_LENGTH);
	}
}

void impulsCreateMed(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		impuls->raw[i]=(impuls->pulse*PULSE_LENGTH);
		impuls->raw[i+1]=(PULSE_LENGTH);
		impuls->raw[i+2]=(impuls->pulse*PULSE_LENGTH);
		impuls->raw[i+3]=(PULSE_LENGTH);
	}
}

void impulsCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		impuls->raw[i]=(PULSE_LENGTH);
		impuls->raw[i+1]=(impuls->pulse*PULSE_LENGTH);
		impuls->raw[i+2]=(PULSE_LENGTH);
		impuls->raw[i+3]=(impuls->pulse*PULSE_LENGTH);
	}
}
void impulsClearCode(void) {
	impulsCreateLow(0,47);
}

void impulsCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			impulsCreateMed(x, x+3);
		}
	}
}

void impulsCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
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
	impuls->raw[48]=(PULSE_LENGTH);
	impuls->raw[49]=(impuls->footer*PULSE_LENGTH);
}

int impulsCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_string(code, "on", &tmp) == 0)
		state=1;
	if(json_find_string(code, "unit", &tmp) == 0)
		unit = atoi(tmp);

	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "impuls: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 31 || id < 0) {
		logprintf(LOG_ERR, "impuls: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 31 || unit < 0) {
		logprintf(LOG_ERR, "impuls: invalid unit range");
		return EXIT_FAILURE;
	} else {
		impulsCreateMessage(id, unit, state);
		impulsClearCode();
		impulsCreateUnit(unit);
		impulsCreateId(id);
		impulsCreateState(state);
		impulsCreateFooter();
	}
	return EXIT_SUCCESS;
}

void impulsPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void impulsInit(void) {

	protocol_register(&impuls);
	impuls->id = strdup("impuls");
	protocol_add_device(impuls, "impuls", "Impuls Switches");
	protocol_add_device(impuls, "select-remote", "SelectRemote Switches");
	protocol_add_conflict(impuls, "arctech_old");
	protocol_add_conflict(impuls, "sartano");
	impuls->type = SWITCH;
	impuls->pulse = 3;
	impuls->footer = 31;
	impuls->rawLength = 50;
	impuls->binLength = 12;
	impuls->message = malloc(sizeof(JsonNode));
	impuls->lsb = 1;

	impuls->bit = 0;
	impuls->recording = 0;

	options_add(&impuls->options, 't', "on", no_value, config_state, NULL);
	options_add(&impuls->options, 'f', "off", no_value, config_state, NULL);
	options_add(&impuls->options, 'u', "unit", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&impuls->options, 'i', "id", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");

	impuls->parseBinary=&impulsParseBinary;
	impuls->createCode=&impulsCreateCode;
	impuls->printHelp=&impulsPrintHelp;
}
