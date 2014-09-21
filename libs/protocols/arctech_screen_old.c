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
#include "arctech_screen_old.h"

static void arctechSrOldCreateMessage(int id, int unit, int state) {
	arctech_screen_old->message = json_mkobject();
	json_append_member(arctech_screen_old->message, "id", json_mknumber(id));
	json_append_member(arctech_screen_old->message, "unit", json_mknumber(unit));
	if(state == 1)
		json_append_member(arctech_screen_old->message, "state", json_mkstring("up"));
	else
		json_append_member(arctech_screen_old->message, "state", json_mkstring("down"));
}

static void arctechSrOldParseBinary(void) {
	int unit = binToDec(arctech_screen_old->binary, 0, 3);
	int state = arctech_screen_old->binary[11];
	int id = binToDec(arctech_screen_old->binary, 4, 8);
	arctechSrOldCreateMessage(id, unit, state);
}

static void arctechSrOldCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_screen_old->raw[i]=(arctech_screen_old->plslen->length);
		arctech_screen_old->raw[i+1]=(arctech_screen_old->pulse*arctech_screen_old->plslen->length);
		arctech_screen_old->raw[i+2]=(arctech_screen_old->pulse*arctech_screen_old->plslen->length);
		arctech_screen_old->raw[i+3]=(arctech_screen_old->plslen->length);
	}
}

static void arctechSrOldCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_screen_old->raw[i]=(arctech_screen_old->plslen->length);
		arctech_screen_old->raw[i+1]=(arctech_screen_old->pulse*arctech_screen_old->plslen->length);
		arctech_screen_old->raw[i+2]=(arctech_screen_old->plslen->length);
		arctech_screen_old->raw[i+3]=(arctech_screen_old->pulse*arctech_screen_old->plslen->length);
	}
}

static void arctechSrOldClearCode(void) {
	arctechSrOldCreateHigh(0,35);
	arctechSrOldCreateLow(36,47);
}

static void arctechSrOldCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			arctechSrOldCreateLow(x, x+3);
		}
	}
}

static void arctechSrOldCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			arctechSrOldCreateLow(16+x, 16+x+3);
		}
	}
}

static void arctechSrOldCreateState(int state) {
	if(state == 0) {
		arctechSrOldCreateHigh(44,47);
	}
}

static void arctechSrOldCreateFooter(void) {
	arctech_screen_old->raw[48]=(arctech_screen_old->plslen->length);
	arctech_screen_old->raw[49]=(PULSE_DIV*arctech_screen_old->plslen->length);
}

static int arctechSrOldCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "down", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "up", &itmp) == 0)
		state=1;

	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "arctech_screen_old: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 31 || id < 0) {
		logprintf(LOG_ERR, "arctech_screen_old: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 15 || unit < 0) {
		logprintf(LOG_ERR, "arctech_screen_old: invalid unit range");
		return EXIT_FAILURE;
	} else {
		arctechSrOldCreateMessage(id, unit, state);
		arctechSrOldClearCode();
		arctechSrOldCreateUnit(unit);
		arctechSrOldCreateId(id);
		arctechSrOldCreateState(state);
		arctechSrOldCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void arctechSrOldPrintHelp(void) {
	printf("\t -t --up\t\t\tsend an up signal\n");
	printf("\t -f --down\t\t\tsend an down signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void arctechSrOldInit(void) {

	protocol_register(&arctech_screen_old);
	protocol_set_id(arctech_screen_old, "arctech_screen_old");
	protocol_device_add(arctech_screen_old, "kaku_screen_old", "Old KlikAanKlikUit Screens");
	protocol_plslen_add(arctech_screen_old, 336);
	arctech_screen_old->devtype = SCREEN;
	arctech_screen_old->hwtype = RF433;
	arctech_screen_old->pulse = 3;
	arctech_screen_old->rawlen = 50;
	arctech_screen_old->binlen = 12;
	arctech_screen_old->lsb = 2;

	options_add(&arctech_screen_old->options, 't', "up", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_screen_old->options, 'f', "down", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_screen_old->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_screen_old->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");

	options_add(&arctech_screen_old->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	arctech_screen_old->parseBinary=&arctechSrOldParseBinary;
	arctech_screen_old->createCode=&arctechSrOldCreateCode;
	arctech_screen_old->printHelp=&arctechSrOldPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "arctech_screen_old";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	arctechSrOldInit();
}
#endif
