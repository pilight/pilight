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
#include "silvercrest.h"

static void silvercrestCreateMessage(int systemcode, int unitcode, int state) {
	silvercrest->message = json_mkobject();
	json_append_member(silvercrest->message, "systemcode", json_mknumber(systemcode));
	json_append_member(silvercrest->message, "unitcode", json_mknumber(unitcode));
	if(state == 0) {
		json_append_member(silvercrest->message, "state", json_mkstring("on"));
	} else {
		json_append_member(silvercrest->message, "state", json_mkstring("off"));
	}
}

static void silvercrestParseBinary(void) {
	int systemcode = binToDec(silvercrest->binary, 0, 4);
	int unitcode = binToDec(silvercrest->binary, 5, 9);
	int check = silvercrest->binary[10];
	int state = silvercrest->binary[11];
	if(check != state) {
		silvercrestCreateMessage(systemcode, unitcode, state);
	}
}

static void silvercrestCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		silvercrest->raw[i]=(silvercrest->plslen->length);
		silvercrest->raw[i+1]=(silvercrest->pulse*silvercrest->plslen->length);
		silvercrest->raw[i+2]=(silvercrest->pulse*silvercrest->plslen->length);
		silvercrest->raw[i+3]=(silvercrest->plslen->length);
	}
}

static void silvercrestCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		silvercrest->raw[i]=(silvercrest->plslen->length);
		silvercrest->raw[i+1]=(silvercrest->pulse*silvercrest->plslen->length);
		silvercrest->raw[i+2]=(silvercrest->plslen->length);
		silvercrest->raw[i+3]=(silvercrest->pulse*silvercrest->plslen->length);
	}
}
static void silvercrestClearCode(void) {
	silvercrestCreateLow(0,47);
}

static void silvercrestCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			silvercrestCreateHigh(x, x+3);
		}
	}
}

static void silvercrestCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			silvercrestCreateHigh(20+x, 20+x+3);
		}
	}
}

static void silvercrestCreateState(int state) {
	if(state == 1) {
		silvercrestCreateHigh(44, 47);
	} else {
		silvercrestCreateHigh(40, 43);
	}
}

static void silvercrestCreateFooter(void) {
	silvercrest->raw[48]=(silvercrest->plslen->length);
	silvercrest->raw[49]=(PULSE_DIV*silvercrest->plslen->length);
}

static int silvercrestCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "silvercrest: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "silvercrest: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "silvercrest: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		silvercrestCreateMessage(systemcode, unitcode, state);
		silvercrestClearCode();
		silvercrestCreateSystemCode(systemcode);
		silvercrestCreateUnitCode(unitcode);
		silvercrestCreateState(state);
		silvercrestCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void silvercrestPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void silvercrestInit(void) {

	protocol_register(&silvercrest);
	protocol_set_id(silvercrest, "silvercrest");
	protocol_device_add(silvercrest, "silvercrest", "Silvercrest Switches");
	protocol_device_add(silvercrest, "unitech", "Unitech Switches");
	protocol_plslen_add(silvercrest, 312);
	silvercrest->devtype = SWITCH;
	silvercrest->hwtype = RF433;
	silvercrest->pulse = 3;
	silvercrest->rawlen = 50;
	silvercrest->binlen = 12;
	silvercrest->lsb = 3;

	options_add(&silvercrest->options, 's', "systemcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&silvercrest->options, 'u', "unitcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&silvercrest->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&silvercrest->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&silvercrest->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	silvercrest->parseBinary=&silvercrestParseBinary;
	silvercrest->createCode=&silvercrestCreateCode;
	silvercrest->printHelp=&silvercrestPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "silvercrest";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	silvercrestInit();
}
#endif
