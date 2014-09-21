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
#include "pollin.h"

static void pollinCreateMessage(int systemcode, int unitcode, int state) {
	pollin->message = json_mkobject();
	json_append_member(pollin->message, "systemcode", json_mknumber(systemcode));
	json_append_member(pollin->message, "unitcode", json_mknumber(unitcode));
	if(state == 0) {
		json_append_member(pollin->message, "state", json_mkstring("on"));
	} else {
		json_append_member(pollin->message, "state", json_mkstring("off"));
	}
}

static void pollinParseBinary(void) {
	int systemcode = binToDec(pollin->binary, 0, 4);
	int unitcode = binToDec(pollin->binary, 5, 9);
	int state = pollin->binary[11];
	pollinCreateMessage(systemcode, unitcode, state);
}

static void pollinCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		pollin->raw[i]=(pollin->plslen->length);
		pollin->raw[i+1]=(pollin->pulse*pollin->plslen->length);
		pollin->raw[i+2]=(pollin->pulse*pollin->plslen->length);
		pollin->raw[i+3]=(pollin->plslen->length);
	}
}

static void pollinCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		pollin->raw[i]=(pollin->plslen->length);
		pollin->raw[i+1]=(pollin->pulse*pollin->plslen->length);
		pollin->raw[i+2]=(pollin->plslen->length);
		pollin->raw[i+3]=(pollin->pulse*pollin->plslen->length);
	}
}
static void pollinClearCode(void) {
	pollinCreateLow(0,47);
}

static void pollinCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			pollinCreateHigh(x, x+3);
		}
	}
}

static void pollinCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			pollinCreateHigh(20+x, 20+x+3);
		}
	}
}

static void pollinCreateState(int state) {
	if(state == 1) {
		pollinCreateHigh(44, 47);
	}
}

static void pollinCreateFooter(void) {
	pollin->raw[48]=(pollin->plslen->length);
	pollin->raw[49]=(PULSE_DIV*pollin->plslen->length);
}

static int pollinCreateCode(JsonNode *code) {
	int systemcode = -1;
	int unitcode = -1;
	int state = -1;
	double itmp = -1;

	if(json_find_number(code, "systemcode", &itmp) == 0)
		systemcode = (int)round(itmp);
	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=0;

	if(systemcode == -1 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "pollin: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "pollin: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "pollin: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		pollinCreateMessage(systemcode, unitcode, state);
		pollinClearCode();
		pollinCreateSystemCode(systemcode);
		pollinCreateUnitCode(unitcode);
		pollinCreateState(state);
		pollinCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void pollinPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void pollinInit(void) {

	protocol_register(&pollin);
	protocol_set_id(pollin, "pollin");
	protocol_device_add(pollin, "pollin", "Pollin Switches");
	protocol_plslen_add(pollin, 301);
	pollin->devtype = SWITCH;
	pollin->hwtype = RF433;
	pollin->pulse = 3;
	pollin->rawlen = 50;
	pollin->binlen = 12;
	pollin->lsb = 3;

	options_add(&pollin->options, 's', "systemcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&pollin->options, 'u', "unitcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&pollin->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&pollin->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&pollin->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	pollin->parseBinary=&pollinParseBinary;
	pollin->createCode=&pollinCreateCode;
	pollin->printHelp=&pollinPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "pollin";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	pollinInit();
}
#endif
