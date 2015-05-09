/*
	Copyright (C) 2015 CurlyMo & meloen
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
#include "heitech.h"
static void heitechCreateMessage(int systemcode, int unitcode, int state) {
	heitech->message = json_mkobject();
	json_append_member(heitech->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(heitech->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(heitech->message, "state", json_mkstring("on"));
	} else {
		json_append_member(heitech->message, "state", json_mkstring("off"));
	}
}
static void heitechParseBinary(void) {
	int systemcode = binToDec(heitech->binary, 0, 4);
	int unitcode = binToDec(heitech->binary, 5, 9);
	int state = heitech->binary[11];
	heitechCreateMessage(systemcode, unitcode, state);
}

static void heitechCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		heitech->raw[i]=(heitech->plslen->length);
		heitech->raw[i+1]=(heitech->pulse*heitech->plslen->length);
		heitech->raw[i+2]=(heitech->pulse*heitech->plslen->length);
		heitech->raw[i+3]=(heitech->plslen->length);
	}
}

static void heitechCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		heitech->raw[i]=(heitech->plslen->length);
		heitech->raw[i+1]=(heitech->pulse*heitech->plslen->length);
		heitech->raw[i+2]=(heitech->plslen->length);
		heitech->raw[i+3]=(heitech->pulse*heitech->plslen->length);
	}
}
static void heitechClearCode(void) {
	heitechCreateLow(0,47);
}

static void heitechCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			heitechCreateHigh(x, x+3);
		}
	}
}

static void heitechCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			heitechCreateHigh(20+x, 20+x+3);
		}
	}
}

static void heitechCreateState(int state) {
	if(state == 1) {
		heitechCreateHigh(44, 47);
	} else {
		heitechCreateHigh(40, 43);
	}
}

static void heitechCreateFooter(void) {
	heitech->raw[48]=(heitech->plslen->length);
	heitech->raw[49]=(PULSE_DIV*heitech->plslen->length);
}

static int heitechCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "heitech: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 31 || systemcode < 0) {
		logprintf(LOG_ERR, "heitech: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(unitcode > 31 || unitcode < 0) {
		logprintf(LOG_ERR, "heitech: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		heitechCreateMessage(systemcode, unitcode, state);
		heitechClearCode();
		heitechCreateSystemCode(systemcode);
		heitechCreateUnitCode(unitcode);
		heitechCreateState(state);
		heitechCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void heitechPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void heitechInit(void) {

	protocol_register(&heitech);
	protocol_set_id(heitech, "heitech");
	protocol_device_add(heitech, "heitech", "Heitech series Switches");
	protocol_plslen_add(heitech, 273);
	protocol_plslen_add(heitech, 280);
	heitech->devtype = SWITCH;
	heitech->hwtype = RF433;
	heitech->pulse = 3;
	heitech->rawlen = 50;
	heitech->binlen = 12;
	heitech->lsb = 3;

	options_add(&heitech->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&heitech->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&heitech->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&heitech->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&heitech->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	heitech->parseBinary=&heitechParseBinary;
	heitech->createCode=&heitechCreateCode;
	heitech->printHelp=&heitechPrintHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "heitech";
	module->version = "0.3";
	module->reqversion = "6.0";
	module->reqcommit = "187";
}

void init(void) {
	heitechInit();
}
#endif
