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
#include "chacon.h"

void chaconSwCreateMessage(int id, int unit, int state) {
	chacon_switch->message = json_mkobject();
	json_append_member(chacon_switch->message, "id", json_mknumber(id));
	json_append_member(chacon_switch->message, "unit", json_mknumber(unit));
	if(state == 1)
		json_append_member(chacon_switch->message, "state", json_mkstring("on"));
	else
		json_append_member(chacon_switch->message, "state", json_mkstring("off"));
}

void chaconSwParseBinary(void) {
	int unit = binToDec(chacon_switch->binary, 0, 3);
	int state = chacon_switch->binary[11];
	int id = binToDec(chacon_switch->binary, 4, 8);
	chaconSwCreateMessage(id, unit, state);
}

void chaconSwCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		chacon_switch->raw[i]=(chacon_switch->plslen->length);
		chacon_switch->raw[i+1]=(chacon_switch->pulse*chacon_switch->plslen->length);
		chacon_switch->raw[i+2]=(chacon_switch->pulse*chacon_switch->plslen->length);
		chacon_switch->raw[i+3]=(chacon_switch->plslen->length);
	}
}

void chaconSwCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		chacon_switch->raw[i]=(chacon_switch->plslen->length);
		chacon_switch->raw[i+1]=(chacon_switch->pulse*chacon_switch->plslen->length);
		chacon_switch->raw[i+2]=(chacon_switch->plslen->length);
		chacon_switch->raw[i+3]=(chacon_switch->pulse*chacon_switch->plslen->length);
	}
}

void chaconSwClearCode(void) {
	chaconSwCreateHigh(0,35);
	chaconSwCreateLow(36,47);
}

void chaconSwCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			chaconSwCreateLow(x, x+3);
		}
	}
}

void chaconSwCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			chaconSwCreateLow(16+x, 16+x+3);
		}
	}
}

void chaconSwCreateState(int state) {
	if(state == 0) {
		chaconSwCreateHigh(44,47);
	}
}

void chaconSwCreateFooter(void) {
	chacon_switch->raw[48]=(chacon_switch->plslen->length);
	chacon_switch->raw[49]=(PULSE_DIV*chacon_switch->plslen->length);
}

int chaconSwCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "chacon_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 31 || id < 0) {
		logprintf(LOG_ERR, "chacon_switch: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 15 || unit < 0) {
		logprintf(LOG_ERR, "chacon_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {
		chaconSwCreateMessage(id, unit, state);
		chaconSwClearCode();
		chaconSwCreateUnit(unit);
		chaconSwCreateId(id);
		chaconSwCreateState(state);
		chaconSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

void chaconSwPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void chaconSwInit(void) {
	protocol_register(&chacon_switch);
	protocol_set_id(chacon_switch, "chacon_switch");	
	protocol_device_add(chacon_switch, "chacon_switch", "Charon Switches");
	protocol_plslen_add(chacon_switch, 454);
	chacon_switch->devtype = SWITCH;
	chacon_switch->hwtype = RX433;
	chacon_switch->pulse = 3;
	chacon_switch->rawlen = 50;
	chacon_switch->binlen = 12;
	chacon_switch->lsb = 2;

	options_add(&chacon_switch->options, 't', "on", no_value, config_state, NULL);
	options_add(&chacon_switch->options, 'f', "off", no_value, config_state, NULL);
	options_add(&chacon_switch->options, 'u', "unit", has_value, config_id, "^([0-9]{1}|[1][0-5])$");
	options_add(&chacon_switch->options, 'i', "id", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");

	protocol_setting_add_string(chacon_switch, "states", "on,off");
	protocol_setting_add_number(chacon_switch, "readonly", 0);
	
	chacon_switch->parseBinary=&chaconSwParseBinary;
	chacon_switch->createCode=&chaconSwCreateCode;
	chacon_switch->printHelp=&chaconSwPrintHelp;
}
