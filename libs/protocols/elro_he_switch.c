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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "elro_he_switch.h"

static void elroHESwitchCreateMessage(int systemcode, int unitcode, int state) {
	elro_he_switch->message = json_mkobject();
	json_append_member(elro_he_switch->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(elro_he_switch->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(elro_he_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(elro_he_switch->message, "state", json_mkstring("off"));
	}
}

static void elroHESwitchParseBinary(void) {
	int systemcode = binToDec(elro_he_switch->binary, 0, 4);
	int unitcode = binToDec(elro_he_switch->binary, 5, 9);
	int state = elro_he_switch->binary[11];
	elroHESwitchCreateMessage(systemcode, unitcode, state);
}

static void elroHESwitchCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_he_switch->raw[i]=(elro_he_switch->plslen->length);
		elro_he_switch->raw[i+1]=(elro_he_switch->pulse*elro_he_switch->plslen->length);
		elro_he_switch->raw[i+2]=(elro_he_switch->pulse*elro_he_switch->plslen->length);
		elro_he_switch->raw[i+3]=(elro_he_switch->plslen->length);
	}
}

static void elroHESwitchCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_he_switch->raw[i]=(elro_he_switch->plslen->length);
		elro_he_switch->raw[i+1]=(elro_he_switch->pulse*elro_he_switch->plslen->length);
		elro_he_switch->raw[i+2]=(elro_he_switch->plslen->length);
		elro_he_switch->raw[i+3]=(elro_he_switch->pulse*elro_he_switch->plslen->length);
	}
}
static void elroHESwitchClearCode(void) {
	elroHESwitchCreateLow(0,47);
}

static void elroHESwitchCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHESwitchCreateHigh(x, x+3);
		}
	}
}

static void elroHESwitchCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHESwitchCreateHigh(20+x, 20+x+3);
		}
	}
}

static void elroHESwitchCreateState(int state) {
	if(state == 1) {
		elroHESwitchCreateHigh(44, 47);
	} else {
		elroHESwitchCreateHigh(40, 43);
	}
}

static void elroHESwitchCreateFooter(void) {
	elro_he_switch->raw[48]=(elro_he_switch->plslen->length);
	elro_he_switch->raw[49]=(PULSE_DIV*elro_he_switch->plslen->length);
}

static int elroHESwitchCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "elro_he_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "elro_he_switch: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_he_switch: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		elroHESwitchCreateMessage(systemcode, unitcode, state);
		elroHESwitchClearCode();
		elroHESwitchCreateSystemCode(systemcode);
		elroHESwitchCreateUnitCode(unitcode);
		elroHESwitchCreateState(state);
		elroHESwitchCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void elroHESwitchPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void elroHESwitchInit(void) {

	protocol_register(&elro_he_switch);
	protocol_set_id(elro_he_switch, "elro_he_switch");
	protocol_device_add(elro_he_switch, "elro_he_switch", "Elro Home Easy Switches");
	protocol_device_add(elro_he_switch, "brennenstuhl", "Brennenstuhl Comfort");
	protocol_plslen_add(elro_he_switch, 288);
	protocol_plslen_add(elro_he_switch, 300);
	elro_he_switch->devtype = SWITCH;
	elro_he_switch->hwtype = RF433;
	elro_he_switch->pulse = 3;
	elro_he_switch->rawlen = 50;
	elro_he_switch->binlen = 12;
	elro_he_switch->lsb = 3;

	options_add(&elro_he_switch->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_he_switch->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_he_switch->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_he_switch->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_he_switch->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_he_switch->parseBinary=&elroHESwitchParseBinary;
	elro_he_switch->createCode=&elroHESwitchCreateCode;
	elro_he_switch->printHelp=&elroHESwitchPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "elro_he_switch";
	module->version = "1.3";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	elroHESwitchInit();
}
#endif
