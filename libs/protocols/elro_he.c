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
#include "elro_he.h"

static void elroHECreateMessage(int systemcode, int unitcode, int state) {
	elro_he->message = json_mkobject();
	json_append_member(elro_he->message, "systemcode", json_mknumber(systemcode));
	json_append_member(elro_he->message, "unitcode", json_mknumber(unitcode));
	if(state == 0) {
		json_append_member(elro_he->message, "state", json_mkstring("on"));
	} else {
		json_append_member(elro_he->message, "state", json_mkstring("off"));
	}
}

static void elroHEParseBinary(void) {
	int systemcode = binToDec(elro_he->binary, 0, 4);
	int unitcode = binToDec(elro_he->binary, 5, 9);
	int state = elro_he->binary[11];
	elroHECreateMessage(systemcode, unitcode, state);
}

static void elroHECreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_he->raw[i]=(elro_he->plslen->length);
		elro_he->raw[i+1]=(elro_he->pulse*elro_he->plslen->length);
		elro_he->raw[i+2]=(elro_he->pulse*elro_he->plslen->length);
		elro_he->raw[i+3]=(elro_he->plslen->length);
	}
}

static void elroHECreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_he->raw[i]=(elro_he->plslen->length);
		elro_he->raw[i+1]=(elro_he->pulse*elro_he->plslen->length);
		elro_he->raw[i+2]=(elro_he->plslen->length);
		elro_he->raw[i+3]=(elro_he->pulse*elro_he->plslen->length);
	}
}
static void elroHEClearCode(void) {
	elroHECreateLow(0,47);
}

static void elroHECreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHECreateHigh(x, x+3);
		}
	}
}

static void elroHECreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHECreateHigh(20+x, 20+x+3);
		}
	}
}

static void elroHECreateState(int state) {
	if(state == 1) {
		elroHECreateHigh(44, 47);
	}
}

static void elroHECreateFooter(void) {
	elro_he->raw[48]=(elro_he->plslen->length);
	elro_he->raw[49]=(PULSE_DIV*elro_he->plslen->length);
}

static int elroHECreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "elro_he: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "elro_he: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_he: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		elroHECreateMessage(systemcode, unitcode, state);
		elroHEClearCode();
		elroHECreateSystemCode(systemcode);
		elroHECreateUnitCode(unitcode);
		elroHECreateState(state);
		elroHECreateFooter();
	}
	return EXIT_SUCCESS;
}

static void elroHEPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void elroHEInit(void) {

	protocol_register(&elro_he);
	protocol_set_id(elro_he, "elro_he");
	protocol_device_add(elro_he, "elro_he", "Elro Home Easy Switches");
	protocol_plslen_add(elro_he, 288);
	protocol_plslen_add(elro_he, 300);
	elro_he->devtype = SWITCH;
	elro_he->hwtype = RF433;
	elro_he->pulse = 3;
	elro_he->rawlen = 50;
	elro_he->binlen = 12;
	elro_he->lsb = 3;

	options_add(&elro_he->options, 's', "systemcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_he->options, 'u', "unitcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_he->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_he->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_he->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_he->parseBinary=&elroHEParseBinary;
	elro_he->createCode=&elroHECreateCode;
	elro_he->printHelp=&elroHEPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "elro_he";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	elroHEInit();
}
#endif
