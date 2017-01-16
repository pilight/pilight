/*
	Copyright (C) 2015 CurlyMo, Meloen and Scarala

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
#include "ev1527_ex.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	220
#define MAX_PULSE_LENGTH	500
#define AVG_PULSE_LENGTH	400
#define RAW_LENGTH			50

static int validate(void) {
	if(ev1527_ex->rawlen == RAW_LENGTH) {
		if(ev1527_ex->raw[ev1527_ex->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   ev1527_ex->raw[ev1527_ex->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int systemcode) {
	ev1527_ex->message = json_mkobject();
	json_append_member(ev1527_ex->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(ev1527_ex->message, "state", json_mkstring("on"));
}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], x = 0, i = 0;

	if(ev1527_ex->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "ev1527_ex: parsecode - invalid parameter passed %d", ev1527_ex->rawlen);
		return;
	}

	for(x=0;x<ev1527_ex->rawlen-2;x+=2) {
		if(ev1527_ex->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	int systemcode = binToDec(binary, 0, 23);
	createMessage(systemcode);
}

static void createLow(int s, int e) {
	int i = 0;

	for(i=s;i<=e;i+=2) {
		ev1527_ex->raw[i]=(AVG_PULSE_LENGTH);
		ev1527_ex->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i = 0;

	for(i=s;i<=e;i+=2) {
		ev1527_ex->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		ev1527_ex->raw[i+1]=(AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createLow(0, ev1527_ex->rawlen-2);
}

static void createSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i = 0, x = 0;

	length = decToBinRev(systemcode, binary);
	for (i = 0; i <= length; i++) {
		if (binary[i] == 0) {
			x = i * 2;
			createLow(x, x+1);
		} else { //so binary[i] == 1
			x = i * 2;
			createHigh(x, x + 1);
		}
	}
}

static void createFooter(void) {
	ev1527_ex->raw[48]=(AVG_PULSE_LENGTH);
	ev1527_ex->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code) {
	int systemcode = -1;
	int readonly = 0;
	int sendonly = 0;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "systemcode", &itmp) == 0)
		systemcode = (int)round(itmp);
	if(json_find_number(code, "readonly", &itmp) == 0)
		readonly = (int)round(itmp);
	if(json_find_number(code, "sendonly", &itmp) == 0)
		sendonly = (int)round(itmp);

	if(systemcode == -1) {
		logprintf(LOG_ERR, "ev1527_ex: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		createMessage(systemcode);
		if( readonly == 0 ){
			clearCode();
			createSystemCode(systemcode);
			createFooter();
		}
		ev1527_ex->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -r --readonly\t\t\read signals only\n");
	printf("\t -w --sendonly\t\t\send signals only\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif

void ev1527ExInit(void) {
	protocol_register(&ev1527_ex);
	protocol_set_id(ev1527_ex, "ev1527_ex");
	protocol_device_add(ev1527_ex, "ev1527_ex", "EV1527 Extended");
	ev1527_ex->devtype = SWITCH;
	ev1527_ex->hwtype = RF433;
	ev1527_ex->minrawlen = RAW_LENGTH;
	ev1527_ex->maxrawlen = RAW_LENGTH;
	ev1527_ex->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	ev1527_ex->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&ev1527_ex->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-7]$"));
	options_add(&ev1527_ex->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ev1527_ex->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ev1527_ex->options, 'r', "readonly", OPTION_NO_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);
	options_add(&ev1527_ex->options, 'w', "sendonly", OPTION_NO_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&ev1527_ex->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&ev1527_ex->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	ev1527_ex->parseCode=&parseCode;
	ev1527_ex->createCode=&createCode;
	ev1527_ex->printHelp=&printHelp;
	ev1527_ex->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ev1527_ex";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	ev1527ExInit();
}
#endif