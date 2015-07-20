/*
	Copyright (C) 2014 CurlyMo

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

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "kambrook.h"

#define PULSE_MULTIPLIER 2
#define MIN_PULSE_LENGTH 288
#define MAX_PULSE_LENGTH 280
#define AVG_PULSE_LENGTH 284
#define RAW_LENGTH 96

static int validate(void) {
	if(kambrook->rawlen == RAW_LENGTH) {
		if(kambrook->raw[kambrook->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
			kambrook->raw[kambrook->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}
	return -1;
}

static void createMessage(int id, int unit, int state) {
	kambrook->message = json_mkobject();
	json_append_member(kambrook->message, "id", json_mknumber(id, 0));
	json_append_member(kambrook->message, "unit", json_mknumber(unit, 0));
	if(state == 0) {
		json_append_member(kambrook->message, "state", json_mkstring("on"));
	} else {
		json_append_member(kambrook->message, "state", json_mkstring("off"));
	}
}

static void createLow(int s, int e) {
	int i;
	for(i = s;i <= e;i += 2) {
		kambrook->raw[i]=(AVG_PULSE_LENGTH);
		kambrook->raw[i+1]=(AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i;
	for(i = s;i <= e;i += 2) {
		kambrook->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		kambrook->raw[i+1]=(AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createLow(0,94);
}

static void createHeader(void){
	int i;
	for(i = 2;i < 16;i += 4) {
		createHigh(i,i+1);
	}
}

static void createId(int id) {
	int binary[255], length = 0, i = 0, x = 0;
	length = decToBinRev(id, binary);
	for(i = 0;i <= length;i++) {
		if(binary[i] == 1) {
			x = i*2;
			createHigh(62-x,62-x+1);
		}
	}
}

static void createFooter(void) {
	createHigh(80,94);
	kambrook->raw[95]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static void createOverallCode(int unit, int state) {
	int binary[255], length = 0, i = 0, x = 0, overallcode = 0;
	while(unit > 5) {
		overallcode += 16;
		unit -= 5;
	}
	overallcode+=(unit*2)-1+state;
	length = decToBinRev(overallcode, binary);
	for(i = 0;i <= length;i++) {
		if(binary[i] == 1) {
			x = i*2;
			createHigh(78-x, 78-x+1);
		}
	}
}

static int createCode(struct JsonNode *code) {
	double itmp = 0.0;
	int id = -1, unit = -1, state = -1;
	if(json_find_number(code, "id", &itmp) == 0) id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0) unit = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0) state=1;
	else if(json_find_number(code, "on", &itmp) == 0) state=0;
	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "kambrook: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 29999 || id < 1) {
		logprintf(LOG_ERR, "kambrook: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 19 || unit < 1) {
		logprintf(LOG_ERR, "kambrook: invalid unit range");
		return EXIT_FAILURE;
	} else {
		createMessage(id, unit, state);
		clearCode();
		createHeader();
		createId(id);
		createOverallCode(unit,state);
		createFooter();
		kambrook->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
		printf("\t -i --id=id\tcontrol a device with this id (id) (1-29999)\n");
		printf("\t -u --unit=unit\t\tcontrol a device with this unit (1-19)\n");
		printf("\t -t --on\t\t\tsend an on signal\n");
		printf("\t -f --off\t\t\tsend an off signal\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif

void kambrookInit(void) {
	protocol_register(&kambrook);
	protocol_set_id(kambrook, "kambrook");
	protocol_device_add(kambrook, "kambrook", "kambrook Switches");
	kambrook->devtype = SWITCH;
	kambrook->hwtype = RF433;
	options_add(&kambrook->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(([0-2][0-9][0-9][0-9][0-9])|\\d{4}|\\d{3}|\\d{2}|[1-9])$");
	options_add(&kambrook->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-1]?[0-9])$");
	options_add(&kambrook->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&kambrook->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&kambrook->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&kambrook->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	kambrook->createCode=&createCode;
	kambrook->printHelp=&printHelp;
	kambrook->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "kambrook";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	kambrookInit();
}

#endif
