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
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "arctech_switch_old.h"

static void arctechSwOldCreateMessage(int id, int unit, int state) {
	arctech_switch_old->message = json_mkobject();
	json_append_member(arctech_switch_old->message, "id", json_mknumber(id));
	json_append_member(arctech_switch_old->message, "unit", json_mknumber(unit));
	if(state == 1)
		json_append_member(arctech_switch_old->message, "state", json_mkstring("on"));
	else
		json_append_member(arctech_switch_old->message, "state", json_mkstring("off"));
}

static void arctechSwOldParseBinary(void) {
	int unit = binToDec(arctech_switch_old->binary, 0, 3);
	int state = arctech_switch_old->binary[11];
	int id = binToDec(arctech_switch_old->binary, 4, 8);
	arctechSwOldCreateMessage(id, unit, state);
}

static void arctechSwOldCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_switch_old->raw[i]=(arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+1]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+2]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+3]=(arctech_switch_old->plslen->length);
	}
}

static void arctechSwOldCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_switch_old->raw[i]=(arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+1]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+2]=(arctech_switch_old->plslen->length);
		arctech_switch_old->raw[i+3]=(arctech_switch_old->pulse*arctech_switch_old->plslen->length);
	}
}

static void arctechSwOldClearCode(void) {
	arctechSwOldCreateHigh(0,35);
	arctechSwOldCreateLow(36,47);
}

static void arctechSwOldCreateUnit(int unit) {
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

static void arctechSwOldCreateId(int id) {
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

static void arctechSwOldCreateState(int state) {
	if(state == 0) {
		arctechSwOldCreateHigh(44,47);
	}
}

static void arctechSwOldCreateFooter(void) {
	arctech_switch_old->raw[48]=(arctech_switch_old->plslen->length);
	arctech_switch_old->raw[49]=(PULSE_DIV*arctech_switch_old->plslen->length);
}

static int arctechSwOldCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

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

static void arctechSwOldPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void arctechSwOldInit(void) {

	protocol_register(&arctech_switch_old);
	protocol_set_id(arctech_switch_old, "arctech_switch_old");
	protocol_device_add(arctech_switch_old, "kaku_switch_old", "Old KlikAanKlikUit Switches");
	protocol_device_add(arctech_switch_old, "cogex", "Cogex Switches");
	protocol_device_add(arctech_switch_old, "intertechno_old", "Old Intertechno Switches");
	protocol_device_add(arctech_switch_old, "byebyestandbye", "Bye Bye Standbye Switches");
	protocol_device_add(arctech_switch_old, "duwi", "Düwi Terminal Switches");
	protocol_device_add(arctech_switch_old, "promax", "PRO max Switches");
	protocol_plslen_add(arctech_switch_old, 336);
	protocol_plslen_add(arctech_switch_old, 326);
	protocol_plslen_add(arctech_switch_old, 390);
	protocol_plslen_add(arctech_switch_old, 400);
	protocol_plslen_add(arctech_switch_old, 330);

	arctech_switch_old->devtype = SWITCH;
	arctech_switch_old->hwtype = RF433;
	arctech_switch_old->pulse = 3;
	arctech_switch_old->rawlen = 50;
	arctech_switch_old->binlen = 12;
	arctech_switch_old->lsb = 2;

	options_add(&arctech_switch_old->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_switch_old->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_switch_old->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_switch_old->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");

	options_add(&arctech_switch_old->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	arctech_switch_old->parseBinary=&arctechSwOldParseBinary;
	arctech_switch_old->createCode=&arctechSwOldCreateCode;
	arctech_switch_old->printHelp=&arctechSwOldPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "arctech_switch_old";
	module->version = "1.2";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	arctechSwOldInit();
}
#endif
