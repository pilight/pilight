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
#include "ehome.h"

static void ehomeCreateMessage(int id, int state) {
	ehome->message = json_mkobject();
	json_append_member(ehome->message, "id", json_mknumber(id));
	if(state == 1) {
		json_append_member(ehome->message, "state", json_mkstring("on"));
	} else {
		json_append_member(ehome->message, "state", json_mkstring("off"));
	}
}

static void ehomeParseCode(void) {
	int i = 0;
	for(i=0; i<ehome->rawlen; i+=4) {
		if(ehome->code[i+3] == 1) {
			ehome->binary[i/4]=1;
		} else {
			ehome->binary[i/4]=0;
		}
	}

	int id = binToDec(ehome->binary, 1, 3);
	int state = ehome->binary[0];

	ehomeCreateMessage(id, state);
}

static void ehomeCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		ehome->raw[i]=ehome->plslen->length;
		ehome->raw[i+1]=(ehome->pulse*ehome->plslen->length);
		ehome->raw[i+2]=(ehome->pulse*ehome->plslen->length);
		ehome->raw[i+3]=ehome->plslen->length;
	}
}

static void ehomeCreateMed(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		ehome->raw[i]=(ehome->pulse*ehome->plslen->length);
		ehome->raw[i+1]=ehome->plslen->length;
		ehome->raw[i+2]=(ehome->pulse*ehome->plslen->length);
		ehome->raw[i+3]=ehome->plslen->length;
	}
}

static void ehomeCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		ehome->raw[i]=ehome->plslen->length;
		ehome->raw[i+1]=(ehome->pulse*ehome->plslen->length);
		ehome->raw[i+2]=ehome->plslen->length;
		ehome->raw[i+3]=(ehome->pulse*ehome->plslen->length);
	}
}

static void ehomeClearCode(void) {
	ehomeCreateLow(0,47);
}

static void ehomeCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			ehomeCreateHigh(4+x, 4+(x+3));
		}
	}
}

static void ehomeCreateState(int state) {
	if(state == 0) {
		ehomeCreateMed(0, 3);
	} else {
		ehomeCreateHigh(0, 3);
	}
}

static void ehomeCreateFooter(void) {
	ehome->raw[48]=(ehome->plslen->length);
	ehome->raw[49]=(PULSE_DIV*ehome->plslen->length);
}

static int ehomeCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "ehome: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 7 || id < 0) {
		logprintf(LOG_ERR, "ehome: invalid id range");
		return EXIT_FAILURE;
	} else {
		ehomeCreateMessage(id, state);
		ehomeClearCode();
		ehomeCreateId(id);
		ehomeCreateState(state);
		ehomeCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void ehomePrintHelp(void) {
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void ehomeInit(void) {

	protocol_register(&ehome);
	protocol_set_id(ehome, "ehome");
	protocol_device_add(ehome, "ehome", "eHome Switches");
	protocol_plslen_add(ehome, 282);
	ehome->devtype = SWITCH;
	ehome->hwtype = RF433;
	ehome->pulse = 3;
	ehome->rawlen = 50;
	ehome->binlen = 12;

	options_add(&ehome->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-4])$");
	options_add(&ehome->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&ehome->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&ehome->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	ehome->parseCode=&ehomeParseCode;
	ehome->createCode=&ehomeCreateCode;
	ehome->printHelp=&ehomePrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "ehome";
	module->version = "0.3";
	module->reqversion = "4.0";
	module->reqcommit = "45";
}

void init(void) {
	ehomeInit();
}
#endif
