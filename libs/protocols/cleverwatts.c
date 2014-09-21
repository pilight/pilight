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
#include "cleverwatts.h"

static void cleverwattsCreateMessage(int id, int unit, int state, int all) {
	cleverwatts->message = json_mkobject();
	json_append_member(cleverwatts->message, "id", json_mknumber(id));
	if(all == 0) {
		json_append_member(cleverwatts->message, "all", json_mknumber(1));
	} else {
		json_append_member(cleverwatts->message, "unit", json_mknumber(unit));
	}
	if(state == 0)
		json_append_member(cleverwatts->message, "state", json_mkstring("on"));
	else
		json_append_member(cleverwatts->message, "state", json_mkstring("off"));
}

static void cleverwattsParseCode(void) {
	int i = 0, x = 0;
	int id = 0, state = 0, unit = 0, all = 0;

	for(i=1;i<cleverwatts->rawlen-1;i+=2) {
		cleverwatts->binary[x++] = cleverwatts->code[i];
	}
	id = binToDecRev(cleverwatts->binary, 0, 19);
	state = cleverwatts->binary[20];
	unit = binToDecRev(cleverwatts->binary, 21, 22);
	all = cleverwatts->binary[23];

	cleverwattsCreateMessage(id, unit, state, all);
}

static void cleverwattsCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		cleverwatts->raw[i]=(cleverwatts->plslen->length);
		cleverwatts->raw[i+1]=(cleverwatts->pulse*cleverwatts->plslen->length);
	}
}

static void cleverwattsCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		cleverwatts->raw[i]=(cleverwatts->pulse*cleverwatts->plslen->length);
		cleverwatts->raw[i+1]=(cleverwatts->plslen->length);
	}
}

static void cleverwattsClearCode(void) {
	cleverwattsCreateHigh(0,47);
}

static void cleverwattsCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			cleverwattsCreateLow(39-(x+1), 39-x);
		}
	}
}

static void cleverwattsCreateAll(int all) {
	if(all == 0) {
		cleverwattsCreateLow(46, 47);
	}
}

static void cleverwattsCreateState(int state) {
	if(state == 1) {
		cleverwattsCreateLow(40, 41);
	}
}

static void cleverwattsCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			cleverwattsCreateLow(45-(x+1), 45-x);
		}
	}
}

static void cleverwattsCreateFooter(void) {
	cleverwatts->raw[48]=(cleverwatts->plslen->length);
	cleverwatts->raw[49]=(PULSE_DIV*cleverwatts->plslen->length);
}

static int cleverwattsCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "all", &itmp)	== 0)
		all = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=0;

	if(id == -1 || (unit == -1 && all == 0) || state == -1) {
		logprintf(LOG_ERR, "cleverwatts: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 1048575 || id < 1) {
		logprintf(LOG_ERR, "cleverwatts: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 3 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "cleverwatts: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 3;
		}
		cleverwattsCreateMessage(id, unit, state, all ^ 1);
		cleverwattsClearCode();
		cleverwattsCreateId(id);
		cleverwattsCreateState(state);
		cleverwattsCreateUnit(unit);
		cleverwattsCreateAll(all);
		cleverwattsCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void cleverwattsPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void cleverwattsInit(void) {

	protocol_register(&cleverwatts);
	protocol_set_id(cleverwatts, "cleverwatts");
	protocol_device_add(cleverwatts, "cleverwatts", "Cleverwatts Switches");
	protocol_plslen_add(cleverwatts, 269);
	cleverwatts->devtype = SWITCH;
	cleverwatts->hwtype = RF433;
	cleverwatts->pulse = 4;
	cleverwatts->rawlen = 50;
	cleverwatts->binlen = 12;

	options_add(&cleverwatts->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&cleverwatts->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&cleverwatts->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&cleverwatts->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,6}|10([0-3][0-9]{4}|4([0-7][0-9]{3}|8([0-4][0-9]{2}|5([0-6][0-9]|7[0-5])))))$");
	options_add(&cleverwatts->options, 'a', "all", OPTION_OPT_VALUE, CONFIG_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&cleverwatts->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	cleverwatts->parseCode=&cleverwattsParseCode;
	cleverwatts->createCode=&cleverwattsCreateCode;
	cleverwatts->printHelp=&cleverwattsPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "cleverwatts";
	module->version = "0.8";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	cleverwattsInit();
}
#endif
