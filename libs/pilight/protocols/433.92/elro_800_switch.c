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
#include "elro_800_switch.h"

static void elro800SwitchCreateMessage(int systemcode, int unitcode, int state) {
	elro_800_switch->message = json_mkobject();
	json_append_member(elro_800_switch->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(elro_800_switch->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(elro_800_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(elro_800_switch->message, "state", json_mkstring("off"));
	}
}

static void elro800SwitchParseBinary(void) {
	int systemcode = binToDec(elro_800_switch->binary, 0, 4);
	int unitcode = binToDec(elro_800_switch->binary, 5, 9);
	int state = elro_800_switch->binary[11];
	elro800SwitchCreateMessage(systemcode, unitcode, state);
}

static void elro800SwitchCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_800_switch->raw[i]=(elro_800_switch->plslen->length);
		elro_800_switch->raw[i+1]=(elro_800_switch->pulse*elro_800_switch->plslen->length);
		elro_800_switch->raw[i+2]=(elro_800_switch->pulse*elro_800_switch->plslen->length);
		elro_800_switch->raw[i+3]=(elro_800_switch->plslen->length);
	}
}

static void elro800SwitchCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_800_switch->raw[i]=(elro_800_switch->plslen->length);
		elro_800_switch->raw[i+1]=(elro_800_switch->pulse*elro_800_switch->plslen->length);
		elro_800_switch->raw[i+2]=(elro_800_switch->plslen->length);
		elro_800_switch->raw[i+3]=(elro_800_switch->pulse*elro_800_switch->plslen->length);
	}
}
static void elro800SwitchClearCode(void) {
	elro800SwitchCreateLow(0,47);
}

static void elro800SwitchCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elro800SwitchCreateHigh(x, x+3);
		}
	}
}

static void elro800SwitchCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elro800SwitchCreateHigh(20+x, 20+x+3);
		}
	}
}

static void elro800SwitchCreateState(int state) {
	if(state == 1) {
		elro800SwitchCreateHigh(44, 47);
	} else {
		elro800SwitchCreateHigh(40, 43);
	}
}

static void elro800SwitchCreateFooter(void) {
	elro_800_switch->raw[48]=(elro_800_switch->plslen->length);
	elro_800_switch->raw[49]=(PULSE_DIV*elro_800_switch->plslen->length);
}

static int elro800SwitchCreateCode(JsonNode *code) {
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
		elro800SwitchCreateMessage(systemcode, unitcode, state);
		elro800SwitchClearCode();
		elro800SwitchCreateSystemCode(systemcode);
		elro800SwitchCreateUnitCode(unitcode);
		elro800SwitchCreateState(state);
		elro800SwitchCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void elro800SwitchPrintHelp(void) {
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
	protocol_plslen_add(elro_800_switch, 288);
	protocol_plslen_add(elro_800_switch, 300);
	elro_800_switch->devtype = SWITCH;
	elro_800_switch->hwtype = RF433;
	elro_800_switch->pulse = 3;
	elro_800_switch->rawlen = 50;
	elro_800_switch->binlen = 12;
	elro_800_switch->lsb = 3;

	options_add(&elro_800_switch->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_800_switch->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_800_switch->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_800_switch->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_800_switch->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_800_switch->parseBinary=&elro800SwitchParseBinary;
	elro_800_switch->createCode=&elro800SwitchCreateCode;
	elro_800_switch->printHelp=&elro800SwitchPrintHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "elro_800_switch";
	module->version = "1.4";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	elro800SwitchInit();
}
#endif
