/*
	Copyright (C) 2016 Puuu

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
#include "action.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	160
#define MAX_PULSE_LENGTH	200
#define AVG_PULSE_LENGTH	180
#define RAW_LENGTH		50

/*
	Action switch

	logical representations of pulses (50 pulses):
	Low	180, 540, 540, 180
	Med	540, 180, 540, 180
	High	180, 540, 180, 540
	Footer	180, 6120

	message format (12 bit):
	01234 56789 01
	iiiii uuuuu cs

	i: id     binary representation with Med and Low
	u: unit   binary representation with High and Low
	s: state  On: High, Off: Low
	c: check  inverse of state
*/

static int validate(void) {
	if(action_switch->rawlen == RAW_LENGTH) {
		if(action_switch->raw[action_switch->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   action_switch->raw[action_switch->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int id, int unit, int state) {
	action_switch->message = json_mkobject();
	json_append_member(action_switch->message, "id", json_mknumber(id, 0));
	json_append_member(action_switch->message, "unit", json_mknumber(unit, 0));
	if(state == 1)
		json_append_member(action_switch->message, "state", json_mkstring("on"));
	else
		json_append_member(action_switch->message, "state", json_mkstring("off"));
}

static void parseCode(void) {
	int x = 0, binary[RAW_LENGTH/4];

	if(action_switch->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "action_switch: parsecode - invalid parameter passed %d", action_switch->rawlen);
		return;
	}


	/* Convert the one's and zero's into binary */
	for(x=0;x<action_switch->rawlen-2;x+=4) {
		if(action_switch->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2)) ||
		   action_switch->raw[x+0] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[x/4] = 1;
		} else {
			binary[x/4] = 0;
		}
	}

	int id = binToDec(binary, 0, 4);
	int unit = binToDec(binary, 5, 9);
	int check = binary[10];
	int state = binary[11];

	if(check != state) {
		createMessage(id, unit, state);
	}
}

static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		action_switch->raw[i]=(AVG_PULSE_LENGTH);
		action_switch->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		action_switch->raw[i+2]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		action_switch->raw[i+3]=(AVG_PULSE_LENGTH);
	}
}

static void createMed(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		action_switch->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		action_switch->raw[i+1]=(AVG_PULSE_LENGTH);
		action_switch->raw[i+2]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		action_switch->raw[i+3]=(AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		action_switch->raw[i]=(AVG_PULSE_LENGTH);
		action_switch->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		action_switch->raw[i+2]=(AVG_PULSE_LENGTH);
		action_switch->raw[i+3]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createLow(0,47);
}

static void createId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			createMed(x, x+3);
		}
	}
}

static void createUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			createHigh(20+x, 20+x+3);
		}
	}
}

static void createState(int state) {
	if(state == 0) {
		createHigh(40,43);
	} else {
		createHigh(44,47);
	}
}

static void createFooter(void) {
	action_switch->raw[48]=(AVG_PULSE_LENGTH);
	action_switch->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	double itmp;

	if(json_find_number(code, "id", &itmp) == 0)
		id=(int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);

	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "action_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id < 0 || id > 31) {
		logprintf(LOG_ERR, "action_switch: invalid id range");
		return EXIT_FAILURE;
	} else if(unit < 0 || unit > 31) {
		logprintf(LOG_ERR, "action_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {
		createMessage(id, unit, state);
		clearCode();
		createId(id);
		createUnit(unit);
		createState(state);
		createFooter();
		action_switch->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionSwitchInit(void) {

	protocol_register(&action_switch);
	protocol_set_id(action_switch, "action_switch");
	protocol_device_add(action_switch, "action_switch", "Action Switches");
	action_switch->devtype = SWITCH;
	action_switch->hwtype = RF433;
	action_switch->minrawlen = RAW_LENGTH;
	action_switch->maxrawlen = RAW_LENGTH;
	action_switch->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	action_switch->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&action_switch->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&action_switch->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&action_switch->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&action_switch->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");

	options_add(&action_switch->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&action_switch->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	action_switch->parseCode=&parseCode;
	action_switch->createCode=&createCode;
	action_switch->printHelp=&printHelp;
	action_switch->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "action_switch";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	actionSwitchInit();
}
#endif
