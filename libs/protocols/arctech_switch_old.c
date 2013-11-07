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
#include "arctech_switch_old.h"

void arctechSwOldCreateMessage(int id, int unit, int state) {
	arctech_switch_old->message = json_mkobject();
	json_append_member(arctech_switch_old->message, "id", json_mknumber(id));
	json_append_member(arctech_switch_old->message, "unit", json_mknumber(unit));
	if(state == 1)
		json_append_member(arctech_switch_old->message, "state", json_mkstring("on"));
	else
		json_append_member(arctech_switch_old->message, "state", json_mkstring("off"));
}

void arctechSwOldParseBinary(void) {
	int unit = binToDec(arctech_switch_old->binary, 0, 3);
	int state = arctech_switch_old->binary[11];
	int id = binToDec(arctech_switch_old->binary, 4, 8);
	arctechSwOldCreateMessage(id, unit, state);
}

void arctechSwOldCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_switch_old->raw[i]=(arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+1]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+2]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+3]=(arctech_switch_old->plslen->length);
	}
}

void arctechSwOldCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_switch_old->raw[i]=(arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+1]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+2]=(arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+3]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
	}
}

void arctechSwOldClearCode(void) {
	arctechSwOldCreateHigh(0,35);
	arctechSwOldCreateLow(36,47);
}

void arctechSwOldCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			arctechSwOldCreateLow(x, x+3);
		}
	}
}

void arctechSwOldCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			arctechSwOldCreateLow(16+x, 16+x+3);
		}
	}
}

void arctechSwOldCreateState(int state) {
	if(state == 0) {
		arctechSwOldCreateHigh(44,47);
	}
}

void arctechSwOldCreateFooter(void) {
	arctech_switch_old->raw[48]=(arctech_switch_old->plslen->length);
	arctech_switch_old->raw[49]=(PULSE_DIV*arctech_switch_old->plslen->length);
}

int arctechSwOldCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "arctech_switch_old: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 31 || id < 0) {
		logprintf(LOG_ERR, "arctech_switch_old: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 15 || unit < 0) {
		logprintf(LOG_ERR, "arctech_switch_old: invalid unit range");
		return EXIT_FAILURE;
	} else {
		arctechSwOldCreateMessage(id, unit, state);
		arctechSwOldClearCode();
		arctechSwOldCreateUnit(unit);
		arctechSwOldCreateId(id);
		arctechSwOldCreateState(state);
		arctechSwOldCreateFooter();
	}
	return EXIT_SUCCESS;
}

void arctechSwOldPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void arctechSwOldInit(void) {

	protocol_register(&arctech_switch_old);
	protocol_set_id(arctech_switch_old, "arctech_switches_old");	
	protocol_device_add(arctech_switch_old, "kaku_switch_old", "Old KlikAanKlikUit Switches");
	protocol_device_add(arctech_switch_old, "cogex", "Cogex Switches");
	protocol_device_add(arctech_switch_old, "intertechno_old", "Old Intertechno Switches");
	protocol_conflict_add(arctech_switch_old, "archtech_screens_old");	
	protocol_plslen_add(arctech_switch_old, 336);
	arctech_switch_old->devtype = SWITCH;
	arctech_switch_old->hwtype = RX433;
	arctech_switch_old->pulse = 3;
	arctech_switch_old->rawlen = 50;
	arctech_switch_old->binlen = 12;
	arctech_switch_old->lsb = 2;

	options_add(&arctech_switch_old->options, 't', "on", no_value, config_state, NULL);
	options_add(&arctech_switch_old->options, 'f', "off", no_value, config_state, NULL);
	options_add(&arctech_switch_old->options, 'u', "unit", has_value, config_id, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_switch_old->options, 'i', "id", has_value, config_id, "^(3[012]?|[012][0-9]|[0-9]{1})$");

	protocol_setting_add_string(arctech_switch_old, "states", "on,off");
	protocol_setting_add_number(arctech_switch_old, "readonly", 0);
	
	arctech_switch_old->parseBinary=arctechSwOldParseBinary;
	arctech_switch_old->createCode=&arctechSwOldCreateCode;
	arctech_switch_old->printHelp=&arctechSwOldPrintHelp;
}
