/*
	Copyright (C) 2014 CurlyMo & wo-rasp

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
#include "techlico_switch.h"

#define SWMAP 5
static int techlicoSwMap[SWMAP]={0, 3, 192, 15, 12};

static void techlicoSwCreateMessage(int id, int unit, int state) {
	techlico_switch->message = json_mkobject();
	json_append_member(techlico_switch->message, "id", json_mknumber(id));
	json_append_member(techlico_switch->message, "unit", json_mknumber(unit));
	if(state == 0) {
		json_append_member(techlico_switch->message, "state", json_mkstring("off"));
	}
	if(state == 1) {
		json_append_member(techlico_switch->message, "state", json_mkstring("on"));
	}
}

static void techlicoSwParseCode(void) {
	int i = 0, x = 0, y = 0;
	int id = -1, state = -1, unit = -1, code = 0;

	for(i=0;i<techlico_switch->rawlen;i+=2) {
		techlico_switch->binary[x++] = techlico_switch->code[i];
	}
	id = binToDecRev(techlico_switch->binary, 0, 15);

	code = binToDecRev(techlico_switch->binary, 16, 23);

	for(y=0;y<SWMAP;y++) {
		if(techlicoSwMap[y] == code) {
			unit = y;
			break;
		}
	}

	techlicoSwCreateMessage(id, unit, state);
}

static void techlicoSwCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		techlico_switch->raw[i]=(techlico_switch->plslen->length);
		techlico_switch->raw[i+1]=(techlico_switch->pulse*techlico_switch->plslen->length);
	}
}

static void techlicoSwCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		techlico_switch->raw[i]=(techlico_switch->pulse*techlico_switch->plslen->length);
		techlico_switch->raw[i+1]=(techlico_switch->plslen->length);
	}
}

static void techlicoSwClearCode(void) {
	techlicoSwCreateHigh(0,47);
}

static void techlicoSwCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			techlicoSwCreateLow(31-(x+1), 31-x);
		}
	}
}

static void techlicoSwCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			techlicoSwCreateLow(47-(x+1), 47-x);
		}
	}
}

static void techlicoSwCreateFooter(void) {
	techlico_switch->raw[48]=(techlico_switch->plslen->length);
	techlico_switch->raw[49]=(PULSE_DIV*techlico_switch->plslen->length);
}

static int techlicoSwCreateCode(JsonNode *code) {
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
	if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if((id == -1) || (unit == -1) || (state == -1)) {
		logprintf(LOG_ERR, "techlico_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 65535 || id < 1) {
		logprintf(LOG_ERR, "techlico_switch: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 4 || unit < 1)) {
		logprintf(LOG_ERR, "techlico_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {

		techlicoSwCreateMessage(id, unit, state);
		techlicoSwClearCode();
		techlicoSwCreateId(id);
		unit = techlicoSwMap[unit];
		techlicoSwCreateUnit(unit);
		techlicoSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void techlicoSwPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void techlicoSwInit(void) {

	protocol_register(&techlico_switch);
	protocol_set_id(techlico_switch, "techlico_switch");
	protocol_device_add(techlico_switch, "techlico_switch", "TechLiCo Lamp");
	protocol_plslen_add(techlico_switch, 208);
	techlico_switch->devtype = SWITCH;
	techlico_switch->hwtype = RF433;
	techlico_switch->pulse = 3;
	techlico_switch->rawlen = 50;
	techlico_switch->binlen = 12;

	options_add(&techlico_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&techlico_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&techlico_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([1-4])$");
	options_add(&techlico_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");

	options_add(&techlico_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	techlico_switch->parseCode=&techlicoSwParseCode;
	techlico_switch->createCode=&techlicoSwCreateCode;
	techlico_switch->printHelp=&techlicoSwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "techlico_switch";
	module->version = "0.9";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	techlicoSwInit();
}
#endif


