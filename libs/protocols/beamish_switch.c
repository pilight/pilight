/*
	Copyright (C) 2014 CurlyMo & wo-rasp

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
#include "beamish_switch.h"

static int beamishSwMap[7]={0, 192, 48, 12, 3, 15, 195};

static void beamishSwCreateMessage(int id, int unit, int state, int all) {
	beamish_switch->message = json_mkobject();
	json_append_member(beamish_switch->message, "id", json_mknumber(id));
	if(all == 1) {
		json_append_member(beamish_switch->message, "all", json_mknumber(1));
	} else {
		json_append_member(beamish_switch->message, "unit", json_mknumber(unit));
	}
	if(state == 0) {
		json_append_member(beamish_switch->message, "state", json_mkstring("off"));
	} else {
		json_append_member(beamish_switch->message, "state", json_mkstring("on"));
	}
}

static void beamishSwParseCode(void) {
	int i = 0, x = 0, y = 0;
	int id = -1, state = -1, unit = -1, all = 0, code = 0;

	for(i=0;i<beamish_switch->rawlen;i+=2) {
		beamish_switch->binary[x++] = beamish_switch->code[i];
	}
	id = binToDecRev(beamish_switch->binary, 0, 15);

	code = binToDecRev(beamish_switch->binary, 16, 23);

	for(y=0;y<7;y++) {
		if(beamishSwMap[y] == code) {
			unit = y;
			break;
		}
	}

	if(unit == 5) {
		state = 1;
		all = 1;
	}
	if(unit == 6) {
		state = 0;
		all = 1;
	}

	beamishSwCreateMessage(id, unit, state, all);
}

static void beamishSwCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		beamish_switch->raw[i]=(beamish_switch->plslen->length);
		beamish_switch->raw[i+1]=(beamish_switch->pulse*beamish_switch->plslen->length);
	}
}

static void beamishSwCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		beamish_switch->raw[i]=(beamish_switch->pulse*beamish_switch->plslen->length);
		beamish_switch->raw[i+1]=(beamish_switch->plslen->length);
	}
}

static void beamishSwClearCode(void) {
	beamishSwCreateHigh(0,47);
}

static void beamishSwCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			beamishSwCreateLow(31-(x+1), 31-x);
		}
	}
}

static void beamishSwCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			beamishSwCreateLow(47-(x+1), 47-x);
		}
	}
}

static void beamishSwCreateFooter(void) {
	beamish_switch->raw[48]=(beamish_switch->plslen->length);
	beamish_switch->raw[49]=(PULSE_DIV*beamish_switch->plslen->length);
}

static int beamishSwCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "all", &itmp) == 0)
		all = 1;
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(id == -1 || (unit == -1 && all == 0) || (state == -1 && all == 1)) {
		logprintf(LOG_ERR, "beamish_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 65535 || id < 1) {
		logprintf(LOG_ERR, "beamish_switch: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 4 || unit < 1) && all == 0) {
		logprintf(LOG_ERR, "beamish_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(all == 1 && state == 1)
			unit = 5;
		if(all == 1 && state == 0)
			unit = 6;

		beamishSwCreateMessage(id, unit, state, all);
		beamishSwClearCode();
		beamishSwCreateId(id);
		unit = beamishSwMap[unit];
		beamishSwCreateUnit(unit);
		beamishSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void beamishSwPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void beamishSwInit(void) {

	protocol_register(&beamish_switch);
	protocol_set_id(beamish_switch, "beamish_switch");
	protocol_device_add(beamish_switch, "beamish_switch", "beamish_switch Switches");
	protocol_plslen_add(beamish_switch, 323);
	beamish_switch->devtype = SWITCH;
	beamish_switch->hwtype = RF433;
	beamish_switch->pulse = 4;
	beamish_switch->rawlen = 50;
	beamish_switch->binlen = 12;

	options_add(&beamish_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&beamish_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&beamish_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([1-4])$");
	options_add(&beamish_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
	options_add(&beamish_switch->options, 'a', "all", OPTION_NO_VALUE, 0, JSON_NUMBER, NULL, NULL);

	options_add(&beamish_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	beamish_switch->parseCode=&beamishSwParseCode;
	beamish_switch->createCode=&beamishSwCreateCode;
	beamish_switch->printHelp=&beamishSwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "beamish_switch";
	module->version = "0.9";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	beamishSwInit();
}
#endif


