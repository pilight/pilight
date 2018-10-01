/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "elro_800_switch.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	274
#define MAX_PULSE_LENGTH	320
#define AVG_PULSE_LENGTH	300
#define RAW_LENGTH				50

static int validate(void) {
	if(elro_800_switch->rawlen == RAW_LENGTH) {
		if(elro_800_switch->raw[elro_800_switch->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   elro_800_switch->raw[elro_800_switch->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(char **message, int systemcode, int unitcode, int state) {
	int x = snprintf((*message), 255, "{\"systemcode\":%d,", systemcode);
	x += snprintf(&(*message)[x], 255-x, "\"unitcode\":%d,", unitcode);

	if(state == 0) {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"on\"");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"off\"");
	}
	x += snprintf(&(*message)[x], 255-x, "}");
}

static void parseCode(char **message) {
	int binary[RAW_LENGTH/4], x = 0, i = 0;

	if(elro_800_switch->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "elro_800_switch: parsecode - invalid parameter passed %d", elro_800_switch->rawlen);
		return;
	}

	for(x=0;x<elro_800_switch->rawlen-2;x+=4) {
		if(elro_800_switch->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	int systemcode = binToDec(binary, 0, 4);
	int unitcode = binToDec(binary, 5, 9);
	int state = binary[11];

	createMessage(message, systemcode, unitcode, state);
}

static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_800_switch->raw[i]=(AVG_PULSE_LENGTH);
		elro_800_switch->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		elro_800_switch->raw[i+2]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		elro_800_switch->raw[i+3]=(AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_800_switch->raw[i]=(AVG_PULSE_LENGTH);
		elro_800_switch->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		elro_800_switch->raw[i+2]=(AVG_PULSE_LENGTH);
		elro_800_switch->raw[i+3]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}
static void clearCode(void) {
	createLow(0,47);
}

static void createSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			createHigh(x, x+3);
		}
	}
}

static void createUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			createHigh(20+x, 20+x+3);
		}
	}
}

static void createState(int state) {
	if(state == 1) {
		createHigh(44, 47);
	} else {
		createHigh(40, 43);
	}
}

static void createFooter(void) {
	elro_800_switch->raw[48]=(AVG_PULSE_LENGTH);
	elro_800_switch->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code, char **message) {
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

	if(systemcode == -1 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "elro_800_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "elro_800_switch: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_800_switch: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		createMessage(message, systemcode, unitcode, state);
		clearCode();
		createSystemCode(systemcode);
		createUnitCode(unitcode);
		createState(state);
		createFooter();
		elro_800_switch->rawlen = RAW_LENGTH;
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
void elro800SwitchInit(void) {

	protocol_register(&elro_800_switch);
	protocol_set_id(elro_800_switch, "elro_800_switch");
	protocol_device_add(elro_800_switch, "elro_800_switch", "Elro 800 series Switches");
	protocol_device_add(elro_800_switch, "brennenstuhl", "Brennenstuhl Comfort");
	elro_800_switch->devtype = SWITCH;
	elro_800_switch->hwtype = RF433;
	elro_800_switch->minrawlen = RAW_LENGTH;
	elro_800_switch->maxrawlen = RAW_LENGTH;
	elro_800_switch->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	elro_800_switch->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&elro_800_switch->options, "s", "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_800_switch->options, "u", "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_800_switch->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_800_switch->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_800_switch->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&elro_800_switch->options, "0", "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_800_switch->parseCode=&parseCode;
	elro_800_switch->createCode=&createCode;
	elro_800_switch->printHelp=&printHelp;
	elro_800_switch->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "elro_800_switch";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	elro800SwitchInit();
}
#endif
