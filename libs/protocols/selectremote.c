/*
	Copyright (C) 2013 CurlyMo & Bram1337

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
#include "selectremote.h"

static void selectremoteCreateMessage(int id, int state) {
	selectremote->message = json_mkobject();
	json_append_member(selectremote->message, "id", json_mknumber(id));
	if(state == 1) {
		json_append_member(selectremote->message, "state", json_mkstring("on"));
	} else {
		json_append_member(selectremote->message, "state", json_mkstring("off"));
	}
}

static void selectremoteParseBinary(void) {
	int id = 7-binToDec(selectremote->binary, 1, 3);
	int state = selectremote->binary[8];

	selectremoteCreateMessage(id, state);
}

static void selectremoteCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		selectremote->raw[i]=(selectremote->pulse*selectremote->plslen->length);
		selectremote->raw[i+1]=selectremote->plslen->length;
		selectremote->raw[i+2]=(selectremote->pulse*selectremote->plslen->length);
		selectremote->raw[i+3]=selectremote->plslen->length;
	}
}

static void selectremoteCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		selectremote->raw[i]=selectremote->plslen->length;
		selectremote->raw[i+1]=(selectremote->pulse*selectremote->plslen->length);
		selectremote->raw[i+2]=selectremote->plslen->length;
		selectremote->raw[i+3]=(selectremote->pulse*selectremote->plslen->length);
	}
}

static void selectremoteClearCode(void) {
	selectremoteCreateHigh(0,47);
}

static void selectremoteCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	id = 7-id;
	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			selectremoteCreateLow(4+x, 4+x+3);
		}
	}
}

static void selectremoteCreateState(int state) {
	if(state == 1) {
		selectremoteCreateLow(32, 35);
	}
}

static void selectremoteCreateFooter(void) {
	selectremote->raw[48]=(selectremote->plslen->length);
	selectremote->raw[49]=(PULSE_DIV*selectremote->plslen->length);
}

static int selectremoteCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "selectremote: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 7 || id < 0) {
		logprintf(LOG_ERR, "selectremote: invalid id range");
		return EXIT_FAILURE;
	} else {
		selectremoteCreateMessage(id, state);
		selectremoteClearCode();
		selectremoteCreateId(id);
		selectremoteCreateState(state);
		selectremoteCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void selectremotePrintHelp(void) {
	printf("\t -i --id=systemcode\tcontrol a device with this id\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void selectremoteInit(void) {

	protocol_register(&selectremote);
	protocol_set_id(selectremote, "selectremote");
	protocol_device_add(selectremote, "selectremote", "SelectRemote Switches");
	protocol_plslen_add(selectremote, 396);
	selectremote->devtype = SWITCH;
	selectremote->hwtype = RF433;
	selectremote->pulse = 3;
	selectremote->rawlen = 50;
	selectremote->binlen = 12;

	options_add(&selectremote->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[0-7]$");
	options_add(&selectremote->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&selectremote->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&selectremote->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	selectremote->parseBinary=&selectremoteParseBinary;
	selectremote->createCode=&selectremoteCreateCode;
	selectremote->printHelp=&selectremotePrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "selectremote";
	module->version = "0.8";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	selectremoteInit();
}
#endif
