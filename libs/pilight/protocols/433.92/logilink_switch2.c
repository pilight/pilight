/*
	Copyright (C) 2015 CurlyMo, Meloen and Scarala
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
#include "logilink_switch2.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	279
#define MAX_PULSE_LENGTH	289
#define AVG_PULSE_LENGTH	284
#define RAW_LENGTH				50

static int validate(void) {
	if(logilink_switch2->rawlen == RAW_LENGTH) {
		if(logilink_switch2->raw[logilink_switch2->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   logilink_switch2->raw[logilink_switch2->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int systemcode, int unitcode, int state) {
	logilink_switch2->message = json_mkobject();
	json_append_member(logilink_switch2->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(logilink_switch2->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(logilink_switch2->message, "state", json_mkstring("on"));
	} else {
		json_append_member(logilink_switch2->message, "state", json_mkstring("off"));
	}
} 
static void parseCode(void) {
	int i = 0, x = 0, binary[RAW_LENGTH/2];
	int systemcode = 0, state = 0, unitcode = 0;

	for(x=0;x<logilink_switch2->rawlen-1;x+=2) {
		if(logilink_switch2->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}
	systemcode = binToDecRev(binary, 0, 19);
	state = binary[23];
	unitcode = binToDecRev(binary, 20, 22);

	createMessage(systemcode, unitcode, state);
}

static void createLow(int s, int e) {
	int i = 0;

	for(i=s;i<=e;i+=2) {
		logilink_switch2->raw[i]=(AVG_PULSE_LENGTH);
		logilink_switch2->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i = 0;

	for(i=s;i<=e;i+=2) {
		logilink_switch2->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		logilink_switch2->raw[i+1]=(AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createLow(0, logilink_switch2->rawlen-2);
}

static void createSystemCode(int systemcode) {
	int binary[255];
	int length=0;
	int i = 0, x = 38;

	length = decToBin(systemcode, binary);
	for(i=length;i>=0;i--) {
		if(binary[i] == 1) {
			createHigh(x, x+1);
		}

		x -= 2;
	}
}

static void createUnitCode(int unitcode) {
	switch(unitcode) {
		case 7:
			createHigh(40, 45);	// Button 1
		break;
		case 3:
			createLow(40, 41); // Button 2
			createHigh(42, 45);
		break;
		case 5:
			createHigh(40, 41); // Button 3
			createLow(42, 43);
			createHigh(44, 45);
		break;
		case 6:
			createHigh(40, 43); // Button 4
			createLow(44, 45);
		break;
		case 0:
			createLow(40, 45);	// Button ALL OFF
		break;
		default:
		break;
	}
}

static void createState(int state) {
	if(state == 1) {
		createLow(46, 47);
	} else {
		createHigh(46, 47);
	}
}

static void createFooter(void) {
	logilink_switch2->raw[48]=(AVG_PULSE_LENGTH);
	logilink_switch2->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code) {
	int systemcode = -1;
	int unitcode = -1;
	int state = -1; 
	double itmp = 0;

	if(json_find_number(code, "systemcode", &itmp) == 0)
		systemcode = (int)round(itmp);
	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=0;
	if(json_find_number(code, "version", &itmp) == 0)
		version = (int)round(itmp);
	if(systemcode == -1 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "logilink_switch2: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 2097151 || systemcode < 0) {
		logprintf(LOG_ERR, "logilink_switch2: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 7 || unitcode < 0) {
		logprintf(LOG_ERR, "logilink_switch2: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		createMessage(systemcode, unitcode, state); 
		clearCode();
		createSystemCode(systemcode);
		createUnitCode(unitcode);
		createState(state);
		createFooter();
		logilink_switch2->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n"); 
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif

void logilinkSwitch2Init(void) {
	protocol_register(&logilink_switch2);
	protocol_set_id(logilink_switch2, "logilink_switch2");
	protocol_device_add(logilink_switch2, "logilink_switch2", "Logilink Switches");
	logilink_switch2->devtype = SWITCH;
	logilink_switch2->hwtype = RF433;
	logilink_switch2->minrawlen = RAW_LENGTH;
	logilink_switch2->maxrawlen = RAW_LENGTH;
	logilink_switch2->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	logilink_switch2->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&logilink_switch2->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&logilink_switch2->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-7]$");
	options_add(&logilink_switch2->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&logilink_switch2->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
 
	options_add(&logilink_switch2->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&logilink_switch2->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	logilink_switch2->parseCode=&parseCode;
	logilink_switch2->createCode=&createCode;
	logilink_switch2->printHelp=&printHelp;
	logilink_switch2->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "logilink_sitch";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	logilinkSwitch2Init();
}
#endif