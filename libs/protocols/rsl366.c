/*
	Copyright (C) 2014 CurlyMo & Bram1337 & kirichkov

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
#include "rsl366.h"

static void rsl366CreateMessage(int systemcode, int programcode, int state) {
	rsl366->message = json_mkobject();
	json_append_member(rsl366->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(rsl366->message, "programcode", json_mknumber(programcode, 0));
	if(state == 1) {
		json_append_member(rsl366->message, "state", json_mkstring("on"));
	} else {
		json_append_member(rsl366->message, "state", json_mkstring("off"));
	}
}

static void rsl366ParseCode(void) {
	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<rsl366->rawlen; x+=4) {
		if(rsl366->code[x+3] == 1 || rsl366->code[x+0] == 1) {
			rsl366->binary[x/4]=1;
		} else {
			rsl366->binary[x/4]=0;
		}
	}

	int systemcode = binToDec(rsl366->binary, 0, 4);
	int programcode = binToDec(rsl366->binary, 5, 9);
	// There seems to be no check and binary[10] is always a low
	int state = rsl366->binary[11];

	rsl366CreateMessage(systemcode, programcode, state);
}

static void rsl366CreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		rsl366->raw[i]=rsl366->plslen->length;
		rsl366->raw[i+1]=(rsl366->pulse*rsl366->plslen->length);
		rsl366->raw[i+2]=(rsl366->pulse*rsl366->plslen->length);
		rsl366->raw[i+3]=rsl366->plslen->length;
	}
}

static void rsl366CreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		rsl366->raw[i]=rsl366->plslen->length;
		rsl366->raw[i+1]=(rsl366->pulse*rsl366->plslen->length);
		rsl366->raw[i+2]=rsl366->plslen->length;
		rsl366->raw[i+3]=(rsl366->pulse*rsl366->plslen->length);
	}
}

static void rsl366ClearCode(void) {
	rsl366CreateLow(0,47);
}

static void rsl366CreateSystemCode(int systemcode) {
	rsl366CreateHigh((systemcode-1)*4, (systemcode-1)*4+3);
}

static void rsl366CreateProgramCode(int programcode) {
	rsl366CreateHigh(16+(programcode-1)*4, 16+(programcode-1)*4+3);
}

static void rsl366CreateState(int state) {
	if(state == 0) {
		rsl366CreateHigh(44, 47);
	}
}

static void rsl366CreateFooter(void) {
	rsl366->raw[48]=(rsl366->plslen->length);
	rsl366->raw[49]=(PULSE_DIV*rsl366->plslen->length);
}

static int rsl366CreateCode(JsonNode *code) {
	int systemcode = -1;
	int programcode = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "systemcode", &itmp) == 0)
		systemcode = (int)round(itmp);
	if(json_find_number(code, "programcode", &itmp) == 0)
		programcode = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(systemcode == -1 || programcode == -1 || state == -1) {
		logprintf(LOG_ERR, "rsl366: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 4 || systemcode < 0) {
		logprintf(LOG_ERR, "rsl366: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(programcode > 4 || programcode < 0) {
		logprintf(LOG_ERR, "rsl366: invalid programcode range");
		return EXIT_FAILURE;
	} else {
		rsl366CreateMessage(systemcode, programcode, state);
		rsl366ClearCode();
		rsl366CreateSystemCode(systemcode);
		rsl366CreateProgramCode(programcode);
		rsl366CreateState(state);
		rsl366CreateFooter();
	}
	return EXIT_SUCCESS;
}

static void rsl366PrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --programcode=programcode\tcontrol a device with this programcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void rsl366Init(void) {

	protocol_register(&rsl366);
	protocol_set_id(rsl366, "rsl366");
	protocol_device_add(rsl366, "rsl366", "RSL366 Switches");
	protocol_plslen_add(rsl366, 390);
	rsl366->devtype = SWITCH;
	rsl366->hwtype = RF433;
	rsl366->pulse = 3;
	rsl366->rawlen = 50;
	rsl366->binlen = 12;

	options_add(&rsl366->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1234]{1})$");
	options_add(&rsl366->options, 'u', "programcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1234]{1})$");
	options_add(&rsl366->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&rsl366->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&rsl366->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	rsl366->parseCode=&rsl366ParseCode;
	rsl366->createCode=&rsl366CreateCode;
	rsl366->printHelp=&rsl366PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "rsl366";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	rsl366Init();
}
#endif
