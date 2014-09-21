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
#include "conrad_rsl_switch.h"

static int conradRSLCodes[5][4][2];

static void conradRSLSwCreateMessage(int id, int unit, int state) {
	conrad_rsl_switch->message = json_mkobject();

	if(id == 4) {
		json_append_member(conrad_rsl_switch->message, "all", json_mknumber(1));
	} else {
		json_append_member(conrad_rsl_switch->message, "id", json_mknumber(id+1));
	}
	json_append_member(conrad_rsl_switch->message, "unit", json_mknumber(unit+1));
	if(state == 1) {
		json_append_member(conrad_rsl_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(conrad_rsl_switch->message, "state", json_mkstring("off"));
	}
}

static void conradRSLSwParseCode(void) {
	int x = 0;
	int id = 0, unit = 0, state = 0;

	/* Convert the one's and zero's into binary */
	for(x=0;x<conrad_rsl_switch->rawlen;x+=2) {
		if(conrad_rsl_switch->code[x+1] == 1) {
			conrad_rsl_switch->binary[x/2]=0;
		} else {
			conrad_rsl_switch->binary[x/2]=1;
		}
	}

	int check = binToDecRev(conrad_rsl_switch->binary, 0, 7);
	int match = 0;
	for(id=0;id<5;id++) {
		for(unit=0;unit<4;unit++) {
			if(conradRSLCodes[id][unit][0] == check) {
				state = 0;
				match = 1;
			}
			if(conradRSLCodes[id][unit][1] == check) {
				state = 1;
				match = 1;
			}
			if(match) {
				break;
			}
		}
		if(match) {
			break;
		}
	}
	conradRSLSwCreateMessage(id, unit, state);
}

static void conradRSLSwCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		conrad_rsl_switch->raw[i]=((conrad_rsl_switch->pulse+1)*conrad_rsl_switch->plslen->length);
		conrad_rsl_switch->raw[i+1]=conrad_rsl_switch->plslen->length*3;
	}
}

static void conradRSLSwCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		conrad_rsl_switch->raw[i]=conrad_rsl_switch->plslen->length*3;
		conrad_rsl_switch->raw[i+1]=((conrad_rsl_switch->pulse+1)*conrad_rsl_switch->plslen->length);
	}
}

static void conradRSLSwClearCode(void) {
	int i = 0, x = 0;
	conradRSLSwCreateHigh(0,65);
	// for(i=0;i<65;i+=2) {
		// x=i*2;
		// conradRSLSwCreateHigh(x,x+1);
	// }

	int binary[255];
	int length = 0;

	length = decToBinRev(23876, binary);
	for(i=0;i<=length;i++) {
		x=i*2;
		if(binary[i]==1) {
			conradRSLSwCreateLow(x+16, x+16+1);
		} else {
			conradRSLSwCreateHigh(x+16, x+16+1);
		}
	}
}

static void conradRSLSwCreateId(int id, int unit, int state) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	int code = conradRSLCodes[id][unit][state];

	length = decToBin(code, binary);
	for(i=0;i<=length;i++) {
		x=i*2;
		if(binary[i]==1) {
			conradRSLSwCreateLow(x, x+1);
		} else {
			conradRSLSwCreateHigh(x, x+1);
		}
	}
}

static void conradRSLSwCreateFooter(void) {
	conrad_rsl_switch->raw[64]=(conrad_rsl_switch->plslen->length*3);
	conrad_rsl_switch->raw[65]=(PULSE_DIV*conrad_rsl_switch->plslen->length);
}

static int conradRSLSwCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	int unit = -1;
	int all = 0;
	double itmp = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "all", &itmp) == 0)
		all = 1;
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(unit == -1 || (id == -1 && all == 0) || state == -1) {
		logprintf(LOG_ERR, "conrad_rsl_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if((id > 5 || id < 0) && all == 0) {
		logprintf(LOG_ERR, "conrad_rsl_switch: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 5 || unit < 0) {
		logprintf(LOG_ERR, "conrad_rsl_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(all == 1) {
			id = 5;
		}
		id -= 1;
		unit -= 1;
		conradRSLSwCreateMessage(id, unit, state);
		conradRSLSwClearCode();
		conradRSLSwCreateId(id, unit, state);
		conradRSLSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void conradRSLSwPrintHelp(void) {
	printf("\t -i --id=id\tcontrol a device with this id\n");
	printf("\t -i --unit=unit\tcontrol a device with this unit\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -a --all\t\t\tsend an all signal\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void conradRSLSwInit(void) {

	memset(conradRSLCodes, 0, 33);
	conradRSLCodes[0][0][0] = 190;
	conradRSLCodes[0][0][1] = 182;
	conradRSLCodes[0][1][0] = 129;
	conradRSLCodes[0][1][1] = 142;
	conradRSLCodes[0][2][0] = 174;
	conradRSLCodes[0][2][1] = 166;
	conradRSLCodes[0][3][0] = 158;
	conradRSLCodes[0][3][1] = 150;

	conradRSLCodes[1][0][0] = 181;
	conradRSLCodes[1][0][1] = 185;
	conradRSLCodes[1][1][0] = 141;
	conradRSLCodes[1][1][1] = 133;
	conradRSLCodes[1][2][0] = 165;
	conradRSLCodes[1][2][1] = 169;
	conradRSLCodes[1][3][0] = 149;
	conradRSLCodes[1][3][1] = 153;

	conradRSLCodes[2][0][0] = 184;
	conradRSLCodes[2][0][1] = 176;
	conradRSLCodes[2][1][0] = 132;
	conradRSLCodes[2][1][1] = 136;
	conradRSLCodes[2][2][0] = 168;
	conradRSLCodes[2][2][1] = 160;
	conradRSLCodes[2][3][0] = 152;
	conradRSLCodes[2][3][1] = 144;

	conradRSLCodes[3][0][0] = 178;
	conradRSLCodes[3][0][1] = 188;
	conradRSLCodes[3][1][0] = 138;
	conradRSLCodes[3][1][1] = 130;
	conradRSLCodes[3][2][0] = 162;
	conradRSLCodes[3][2][1] = 172;
	conradRSLCodes[3][3][0] = 146;
	conradRSLCodes[3][3][1] = 156;

	conradRSLCodes[4][0][0] = 163;
	conradRSLCodes[4][0][1] = 147;

	protocol_register(&conrad_rsl_switch);
	protocol_set_id(conrad_rsl_switch, "conrad_rsl_switch");
	protocol_device_add(conrad_rsl_switch, "conrad_rsl_switch", "Conrad RSL Switches");
	protocol_plslen_add(conrad_rsl_switch, 204);
	protocol_plslen_add(conrad_rsl_switch, 194);
	conrad_rsl_switch->devtype = SWITCH;
	conrad_rsl_switch->hwtype = RF433;
	conrad_rsl_switch->pulse = 6;
	conrad_rsl_switch->rawlen = 66;
	conrad_rsl_switch->binlen = 33;

	options_add(&conrad_rsl_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[1-4]$");
	options_add(&conrad_rsl_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[1-4]$");
	options_add(&conrad_rsl_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&conrad_rsl_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&conrad_rsl_switch->options, 'a', "all", OPTION_OPT_VALUE, CONFIG_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&conrad_rsl_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	conrad_rsl_switch->parseCode=&conradRSLSwParseCode;
	conrad_rsl_switch->createCode=&conradRSLSwCreateCode;
	conrad_rsl_switch->printHelp=&conradRSLSwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "conrad_rsl_switch";
	module->version = "0.4";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	conradRSLSwInit();
}
#endif
