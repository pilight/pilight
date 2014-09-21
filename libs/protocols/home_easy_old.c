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
#include "home_easy_old.h"

static void homeEasyOldCreateMessage(int systemcode, int unitcode, int state, int all) {
	home_easy_old->message = json_mkobject();
	json_append_member(home_easy_old->message, "systemcode", json_mknumber(systemcode));
	if(all == 1) {
		json_append_member(home_easy_old->message, "all", json_mknumber(all));
	} else {
		json_append_member(home_easy_old->message, "unitcode", json_mknumber(unitcode));
	}
	if(state == 0) {
		json_append_member(home_easy_old->message, "state", json_mkstring("on"));
	} else {
		json_append_member(home_easy_old->message, "state", json_mkstring("off"));
	}
}

static void homeEasyOldParseBinary(void) {
	int systemcode = 15-binToDecRev(home_easy_old->binary, 1, 4);
	int unitcode = 15-binToDecRev(home_easy_old->binary, 5, 8);
	int all = home_easy_old->binary[9];
	int state = home_easy_old->binary[11];
	int check = home_easy_old->binary[10];
	if(check == 0) {
		homeEasyOldCreateMessage(systemcode, unitcode, state, all);
	}
}

static void homeEasyOldCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		home_easy_old->raw[i]=(home_easy_old->plslen->length);
		home_easy_old->raw[i+1]=(home_easy_old->pulse*home_easy_old->plslen->length);
		home_easy_old->raw[i+2]=(home_easy_old->pulse*home_easy_old->plslen->length);
		home_easy_old->raw[i+3]=(home_easy_old->plslen->length);
	}
}

static void homeEasyOldCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		home_easy_old->raw[i]=(home_easy_old->plslen->length);
		home_easy_old->raw[i+1]=(home_easy_old->pulse*home_easy_old->plslen->length);
		home_easy_old->raw[i+2]=(home_easy_old->plslen->length);
		home_easy_old->raw[i+3]=(home_easy_old->pulse*home_easy_old->plslen->length);
	}
}

static void homeEasyOldClearCode(void) {
	homeEasyOldCreateHigh(0,47);
}

static void homeEasyOldCreateStart(void) {
	homeEasyOldCreateLow(0,3);
}

static void homeEasyOldCreateSystemCode(int systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			homeEasyOldCreateHigh(20+x, 20+x+3);
		}
	}
}

static void homeEasyOldCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			homeEasyOldCreateHigh(4+x, 4+x+3);
		}
	}
}

static void homeEasyOldCreateAll(int all) {
	if(all == 1) {
		homeEasyOldCreateHigh(36, 39);
	}
}

static void homeEasyOldCreateState(int state) {
	if(state == 0) {
		homeEasyOldCreateHigh(44, 47);
	}
}

static void homeEasyOldCreateFooter(void) {
	home_easy_old->raw[48]=(home_easy_old->plslen->length);
	home_easy_old->raw[49]=(PULSE_DIV*home_easy_old->plslen->length);
}

static int homeEasyOldCreateCode(JsonNode *code) {
	int systemcode = -1;
	int unitcode = -1;
	int state = -1;
	int all = -1;
	double itmp = 0;

	if(json_find_number(code, "systemcode", &itmp) == 0)
		systemcode = (int)round(itmp);
	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);
	if(json_find_number(code, "all", &itmp) == 0)
		all = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(systemcode == -1 || (unitcode == -1 && all == 0) || state == -1) {
		logprintf(LOG_ERR, "home_easy_old: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 15 || systemcode < 0) {
		logprintf(LOG_ERR, "home_easy_old: invalid systemcode range");
		return EXIT_FAILURE;
	} else if((unitcode > 15 || unitcode < 0) && all == 0) {
		logprintf(LOG_ERR, "arctech_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(unitcode == -1 && all == 1) {
			unitcode = 15;
		}
		homeEasyOldCreateMessage(systemcode, unitcode, state, all);
		homeEasyOldClearCode();
		homeEasyOldCreateStart();
		homeEasyOldCreateSystemCode(systemcode);
		homeEasyOldCreateUnitCode(unitcode);
		homeEasyOldCreateAll(all);
		homeEasyOldCreateState(state);
		homeEasyOldCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void homeEasyOldPrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void homeEasyOldInit(void) {

	protocol_register(&home_easy_old);
	protocol_set_id(home_easy_old, "home_easy_old");
	protocol_device_add(home_easy_old, "home_easy_old", "Old Home Easy Switches");
	protocol_plslen_add(home_easy_old, 289);
	home_easy_old->devtype = SWITCH;
	home_easy_old->hwtype = RF433;
	home_easy_old->pulse = 3;
	home_easy_old->rawlen = 50;
	home_easy_old->binlen = 12;
	home_easy_old->lsb = 3;

	options_add(&home_easy_old->options, 's', "systemcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&home_easy_old->options, 'u', "unitcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&home_easy_old->options, 'a', "all", OPTION_NO_VALUE, CONFIG_STATE, JSON_NUMBER, NULL, NULL);
	options_add(&home_easy_old->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&home_easy_old->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&home_easy_old->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	home_easy_old->parseBinary=&homeEasyOldParseBinary;
	home_easy_old->createCode=&homeEasyOldCreateCode;
	home_easy_old->printHelp=&homeEasyOldPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "home_easy_old";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	homeEasyOldInit();
}
#endif
