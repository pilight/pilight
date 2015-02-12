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
#include "elro_400.h"

static void elro400CreateMessage(int systemcode, int unitcode, int state) {
	elro_400->message = json_mkobject();
	json_append_member(elro_400->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(elro_400->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 1) {
		json_append_member(elro_400->message, "state", json_mkstring("on"));
	} else {
		json_append_member(elro_400->message, "state", json_mkstring("off"));
	}
}

static void elro400ParseBinary(void) {
	int x = 0;
	for(x=0;x<elro_400->binlen;x++) {
		elro_400->binary[x] ^= 1;
	}
	int systemcode = binToDecRev(elro_400->binary, 0, 4);
	int unitcode = binToDecRev(elro_400->binary, 5, 9);
	int state = elro_400->binary[11];
	elro400CreateMessage(systemcode, unitcode, state);
}

static void elro400CreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_400->raw[i]=(elro_400->plslen->length);
		elro_400->raw[i+1]=(elro_400->pulse*elro_400->plslen->length);
		elro_400->raw[i+2]=(elro_400->pulse*elro_400->plslen->length);
		elro_400->raw[i+3]=(elro_400->plslen->length);
	}
}

static void elro400CreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_400->raw[i]=(elro_400->plslen->length);
		elro_400->raw[i+1]=(elro_400->pulse*elro_400->plslen->length);
		elro_400->raw[i+2]=(elro_400->plslen->length);
		elro_400->raw[i+3]=(elro_400->pulse*elro_400->plslen->length);
	}
}
static void elro400ClearCode(void) {
	elro400CreateHigh(0,47);
}

static void elro400CreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elro400CreateLow(19-(x+3), 19-x);
		}
	}
}

static void elro400CreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elro400CreateLow(39-(x+3), 39-x);
		}
	}
}

static void elro400CreateState(int state) {
	if(state == 1) {
		elro400CreateLow(44, 47);
		elro400CreateLow(40, 43);
	} else {
		elro400CreateLow(40, 43);
	}
}

static void elro400CreateFooter(void) {
	elro_400->raw[48]=(elro_400->plslen->length);
	elro_400->raw[49]=(PULSE_DIV*elro_400->plslen->length);
}

static int elro400CreateCode(JsonNode *code) {
	int systemcode = -1;
	int unitcode = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "systemcode", &itmp) == 0)
		systemcode = (int)round(itmp);
	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(systemcode == -1 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "elro_400: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "elro_400: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_400: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		elro400CreateMessage(systemcode, unitcode, state);
		elro400ClearCode();
		elro400CreateSystemCode(systemcode);
		elro400CreateUnitCode(unitcode);
		elro400CreateState(state);
		elro400CreateFooter();
	}
	return EXIT_SUCCESS;
}

static void elro400PrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void elro400Init(void) {

	protocol_register(&elro_400);
	protocol_set_id(elro_400, "elro_400");
	protocol_device_add(elro_400, "elro_400", "Elro 400 Series Switches");
	protocol_plslen_add(elro_400, 296);
	elro_400->devtype = SWITCH;
	elro_400->hwtype = RF433;
	elro_400->pulse = 3;
	elro_400->rawlen = 50;
	elro_400->binlen = 12;
	elro_400->lsb = 3;

	options_add(&elro_400->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_400->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_400->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_400->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_400->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_400->parseBinary=&elro400ParseBinary;
	elro_400->createCode=&elro400CreateCode;
	elro_400->printHelp=&elro400PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "elro_400";
	module->version = "1.3";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	elro400Init();
}
#endif
