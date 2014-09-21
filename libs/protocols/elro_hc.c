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
#include "elro_hc.h"

static void elroHCCreateMessage(int systemcode, int unitcode, int state) {
	elro_hc->message = json_mkobject();
	json_append_member(elro_hc->message, "systemcode", json_mknumber(systemcode));
	json_append_member(elro_hc->message, "unitcode", json_mknumber(unitcode));
	if(state == 1) {
		json_append_member(elro_hc->message, "state", json_mkstring("on"));
	} else {
		json_append_member(elro_hc->message, "state", json_mkstring("off"));
	}
}

static void elroHCParseBinary(void) {
	int x = 0;
	for(x=0;x<elro_hc->binlen;x++) {
		elro_hc->binary[x] ^= 1;
	}
	int systemcode = binToDecRev(elro_hc->binary, 0, 4);
	int unitcode = binToDecRev(elro_hc->binary, 5, 9);
	int state = elro_hc->binary[11];
	elroHCCreateMessage(systemcode, unitcode, state);
}

static void elroHCCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_hc->raw[i]=(elro_hc->plslen->length);
		elro_hc->raw[i+1]=(elro_hc->pulse*elro_hc->plslen->length);
		elro_hc->raw[i+2]=(elro_hc->pulse*elro_hc->plslen->length);
		elro_hc->raw[i+3]=(elro_hc->plslen->length);
	}
}

static void elroHCCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		elro_hc->raw[i]=(elro_hc->plslen->length);
		elro_hc->raw[i+1]=(elro_hc->pulse*elro_hc->plslen->length);
		elro_hc->raw[i+2]=(elro_hc->plslen->length);
		elro_hc->raw[i+3]=(elro_hc->pulse*elro_hc->plslen->length);
	}
}
static void elroHCClearCode(void) {
	elroHCCreateHigh(0,47);
}

static void elroHCCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHCCreateLow(19-(x+3), 19-x);
		}
	}
}

static void elroHCCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			elroHCCreateLow(39-(x+3), 39-x);
		}
	}
}

static void elroHCCreateState(int state) {
	if(state == 1) {
		elroHCCreateLow(44, 47);
		elroHCCreateLow(40, 43);
	} else {
		elroHCCreateLow(40, 43);
	}
}

static void elroHCCreateFooter(void) {
	elro_hc->raw[48]=(elro_hc->plslen->length);
	elro_hc->raw[49]=(PULSE_DIV*elro_hc->plslen->length);
}

static int elroHCCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "elro_hc: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "elro_hc: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_hc: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		elroHCCreateMessage(systemcode, unitcode, state);
		elroHCClearCode();
		elroHCCreateSystemCode(systemcode);
		elroHCCreateUnitCode(unitcode);
		elroHCCreateState(state);
		elroHCCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void elroHCPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void elroHCInit(void) {

	protocol_register(&elro_hc);
	protocol_set_id(elro_hc, "elro_hc");
	protocol_device_add(elro_hc, "elro_hc", "Elro Home Control Switches");
	protocol_device_add(elro_hc, "brennenstuhl", "Brennenstuhl Comfort");
	protocol_plslen_add(elro_hc, 296);
	elro_hc->devtype = SWITCH;
	elro_hc->hwtype = RF433;
	elro_hc->pulse = 3;
	elro_hc->rawlen = 50;
	elro_hc->binlen = 12;
	elro_hc->lsb = 3;

	options_add(&elro_hc->options, 's', "systemcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_hc->options, 'u', "unitcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_hc->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_hc->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&elro_hc->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	elro_hc->parseBinary=&elroHCParseBinary;
	elro_hc->createCode=&elroHCCreateCode;
	elro_hc->printHelp=&elroHCPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "elro_hc";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	elroHCInit();
}
#endif
