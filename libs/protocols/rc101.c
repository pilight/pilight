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
#include "rc101.h"

static void rc101CreateMessage(int id, int state, int unit, int all) {
	rc101->message = json_mkobject();
	json_append_member(rc101->message, "id", json_mknumber(id, 0));
	if(all == 1) {
		json_append_member(rc101->message, "all", json_mknumber(1, 0));
	} else {
		json_append_member(rc101->message, "unit", json_mknumber(unit, 0));
	}
	if(state == 1) {
		json_append_member(rc101->message, "state", json_mkstring("on"));
	} else {
		json_append_member(rc101->message, "state", json_mkstring("off"));
	}
}

static void rc101ParseCode(void) {
	int i = 0, x = 0;
	for(i=0;i<rc101->rawlen; i+=2) {
		rc101->binary[x++] = rc101->code[i];
	}

	int id = binToDec(rc101->binary, 0, 19);
	int state = rc101->binary[20];
	int unit = 7-binToDec(rc101->binary, 21, 23);
	int all = 0;
	if(unit == 7 && state == 1) {
		all = 1;
		state = 0;
	}
	if(unit == 6 && state == 0) {
		all = 1;
		state = 1;
	}
	rc101CreateMessage(id, state, unit, all);
}

static void rc101CreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		rc101->raw[i]=rc101->plslen->length;
		rc101->raw[i+1]=(rc101->pulse*rc101->plslen->length);
	}
}

static void rc101CreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		rc101->raw[i]=(rc101->pulse*rc101->plslen->length);
		rc101->raw[i+1]=rc101->plslen->length;
	}
}

static void rc101ClearCode(void) {
	rc101CreateLow(0, 63);
}

static void rc101CreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			rc101CreateHigh(x, x+1);
		}
	}
}

static void rc101CreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(7-unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			rc101CreateHigh(42+x, 42+x+1);
		}
	}
}

static void rc101CreateState(int state) {
	if(state == 1) {
		rc101CreateHigh(40, 41);
	}
}

static void rc101CreateFooter(void) {
	rc101->raw[64]=(rc101->plslen->length);
	rc101->raw[65]=(PULSE_DIV*rc101->plslen->length);
}

static int rc101CreateCode(JsonNode *code) {
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
		rc101CreateMessage(id, state, unit, all);
		rc101ClearCode();
		rc101CreateId(id);
		rc101CreateState(state);
		if(unit > -1) {
			rc101CreateUnit(unit);
		}
		rc101CreateFooter();
	}
	return EXIT_SUCCESS;
}

static void rc101PrintHelp(void) {
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void rc101Init(void) {

	protocol_register(&rc101);
	protocol_set_id(rc101, "rc101");
	protocol_device_add(rc101, "rc101", "rc101 Switches");
	protocol_device_add(rc101, "rc102", "rc102 Switches");
	protocol_plslen_add(rc101, 241);
	rc101->devtype = SWITCH;
	rc101->hwtype = RF433;
	rc101->pulse = 3;
	rc101->rawlen = 66;
	rc101->binlen = 16;

	options_add(&rc101->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-4])$");
	options_add(&rc101->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-4])$");
	options_add(&rc101->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&rc101->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&rc101->options, 'a', "all", OPTION_OPT_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&rc101->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	rc101->parseCode=&rc101ParseCode;
	rc101->createCode=&rc101CreateCode;
	rc101->printHelp=&rc101PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "rc101";
	module->version = "0.1";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	rc101Init();
}
#endif
