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
#include "arctech_switch.h"

static void arctechSwCreateMessage(int id, int unit, int state, int all) {
	arctech_switch->message = json_mkobject();
	json_append_member(arctech_switch->message, "id", json_mknumber(id));
	if(all == 1) {
		json_append_member(arctech_switch->message, "all", json_mknumber(all));
	} else {
		json_append_member(arctech_switch->message, "unit", json_mknumber(unit));
	}

	if(state == 1) {
		json_append_member(arctech_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(arctech_switch->message, "state", json_mkstring("off"));
	}
}

static void arctechSwParseBinary(void) {
	int unit = binToDecRev(arctech_switch->binary, 28, 31);
	int state = arctech_switch->binary[27];
	int all = arctech_switch->binary[26];
	int id = binToDecRev(arctech_switch->binary, 0, 25);

	arctechSwCreateMessage(id, unit, state, all);
}

static void arctechSwCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_switch->raw[i]=(arctech_switch->plslen->length);
		arctech_switch->raw[i+1]=(arctech_switch->plslen->length);
		arctech_switch->raw[i+2]=(arctech_switch->plslen->length);
		arctech_switch->raw[i+3]=(arctech_switch->pulse*arctech_switch->plslen->length);
	}
}

static void arctechSwCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_switch->raw[i]=(arctech_switch->plslen->length);
		arctech_switch->raw[i+1]=(arctech_switch->pulse*arctech_switch->plslen->length);
		arctech_switch->raw[i+2]=(arctech_switch->plslen->length);
		arctech_switch->raw[i+3]=(arctech_switch->plslen->length);
	}
}

static void arctechSwClearCode(void) {
	arctechSwCreateLow(2, 132);
}

static void arctechSwCreateStart(void) {
	arctech_switch->raw[0]=(arctech_switch->plslen->length);
	arctech_switch->raw[1]=(9*arctech_switch->plslen->length);
}

static void arctechSwCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechSwCreateHigh(106-x, 106-(x-3));
		}
	}
}

static void arctechSwCreateAll(int all) {
	if(all == 1) {
		arctechSwCreateHigh(106, 109);
	}
}

static void arctechSwCreateState(int state) {
	if(state == 1) {
		arctechSwCreateHigh(110, 113);
	}
}

static void arctechSwCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechSwCreateHigh(130-x, 130-(x-3));
		}
	}
}

static void arctechSwCreateFooter(void) {
	arctech_switch->raw[131]=(PULSE_DIV*arctech_switch->plslen->length);
}

static int arctechSwCreateCode(JsonNode *code) {
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
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(id == -1 || (unit == -1 && all == 0) || state == -1) {
		logprintf(LOG_ERR, "arctech_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 67108863 || id < 1) {
		logprintf(LOG_ERR, "arctech_switch: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 15 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "arctech_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 0;
		}
		arctechSwCreateMessage(id, unit, state, all);
		arctechSwCreateStart();
		arctechSwClearCode();
		arctechSwCreateId(id);
		arctechSwCreateAll(all);
		arctechSwCreateState(state);
		arctechSwCreateUnit(unit);
		arctechSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void arctechSwPrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void arctechSwInit(void) {

	protocol_register(&arctech_switch);
	protocol_set_id(arctech_switch, "arctech_switch");
	protocol_device_add(arctech_switch, "kaku_switch", "KlikAanKlikUit Switches");
	protocol_device_add(arctech_switch, "dio_switch", "D-IO Switches");
	protocol_device_add(arctech_switch, "nexa_switch", "Nexa Switches");
	protocol_device_add(arctech_switch, "coco_switch", "CoCo Technologies Switches");
	protocol_device_add(arctech_switch, "intertechno_switch", "Intertechno Switches");
	protocol_plslen_add(arctech_switch, 315);
	protocol_plslen_add(arctech_switch, 303);
	protocol_plslen_add(arctech_switch, 251);
	arctech_switch->devtype = SWITCH;
	arctech_switch->hwtype = RF433;
	arctech_switch->pulse = 5;
	arctech_switch->rawlen = 132;
	arctech_switch->lsb = 3;
	options_add(&arctech_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
	options_add(&arctech_switch->options, 'a', "all", OPTION_OPT_VALUE, CONFIG_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&arctech_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	arctech_switch->parseBinary=&arctechSwParseBinary;
	arctech_switch->createCode=&arctechSwCreateCode;
	arctech_switch->printHelp=&arctechSwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "arctech_switch";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	arctechSwInit();
}
#endif
