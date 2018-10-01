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
#include "rc101.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	236
#define MAX_PULSE_LENGTH	246
#define AVG_PULSE_LENGTH	241
#define RAW_LENGTH				66

static int validate(void) {
	if(rc101->rawlen == RAW_LENGTH) {
		if(rc101->raw[rc101->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   rc101->raw[rc101->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(char **message, int id, int state, int unit, int all) {
	int x = snprintf((*message), 255, "{\"id\":%d,", id);
	if(all == 1) {
		x += snprintf(&(*message)[x], 255-x, "\"all\":1,");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"unit\":%d,", unit);
	}

	if(state == 1) {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"on\"");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"off\"");
	}
	x += snprintf(&(*message)[x], 255-x, "}");
}

static void parseCode(char **message) {
	int i = 0, x = 0, binary[RAW_LENGTH/2];

	if(rc101->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "rc101: parsecode - invalid parameter passed %d", rc101->rawlen);
		return;
	}

	for(i=0;i<rc101->rawlen; i+=2) {
		if(rc101->raw[i] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[x++] = 1;
		} else {
			binary[x++] = 0;
		}
	}

	int id = binToDec(binary, 0, 19);
	int state = binary[20];
	int unit = 7-binToDec(binary, 21, 23);
	int all = 0;
	if(unit == 7 && state == 1) {
		all = 1;
		state = 0;
	}
	if(unit == 6 && state == 0) {
		all = 1;
		state = 1;
	}
	createMessage(message, id, state, unit, all);
}

static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		rc101->raw[i]=AVG_PULSE_LENGTH;
		rc101->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		rc101->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		rc101->raw[i+1]=AVG_PULSE_LENGTH;
	}
}

static void clearCode(void) {
	createLow(0, 63);
}

static void createId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			createHigh(x, x+1);
		}
	}
}

static void createUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(7-unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			createHigh(42+x, 42+x+1);
		}
	}
}

static void createState(int state) {
	if(state == 1) {
		createHigh(40, 41);
	}
}

static void createFooter(void) {
	rc101->raw[64]=(AVG_PULSE_LENGTH);
	rc101->raw[65]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code, char **message) {
	int id = -1;
	int state = -1;
	int unit = -1;
	int all = -1;
	double itmp = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "all", &itmp) == 0)
		all = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(all == 1 && state == 1) {
		unit = 6;
		state = 0;
	}
	if(all == 1 && state == 0) {
		unit = 7;
		state = 1;
	}

	if(id == -1 || state == -1 || unit == -1) {
		logprintf(LOG_ERR, "rc101: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 1048575 || id < 0) {
		logprintf(LOG_ERR, "rc101: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 4 || unit < 0) {
		createMessage(message, id, state, unit, all);
		clearCode();
		createId(id);
		createState(state);
		if(unit > -1) {
			createUnit(unit);
		}
		createFooter();
		rc101->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void rc101Init(void) {

	protocol_register(&rc101);
	protocol_set_id(rc101, "rc101");
	protocol_device_add(rc101, "rc101", "rc101 Switches");
	protocol_device_add(rc101, "rc102", "rc102 Switches");
	rc101->devtype = SWITCH;
	rc101->hwtype = RF433;
	rc101->minrawlen = RAW_LENGTH;
	rc101->maxrawlen = RAW_LENGTH;
	rc101->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	rc101->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&rc101->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-4])$");
	options_add(&rc101->options, "u", "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-4])$");
	options_add(&rc101->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&rc101->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&rc101->options, "a", "all", OPTION_OPT_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&rc101->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&rc101->options, "0", "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	rc101->parseCode=&parseCode;
	rc101->createCode=&createCode;
	rc101->printHelp=&printHelp;
	rc101->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "rc101";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	rc101Init();
}
#endif
