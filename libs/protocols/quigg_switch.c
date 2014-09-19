/*
	Copyright (C) 2014 CurlyMo & wo_rasp

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
#include "quigg_switch.h"

#define PULSE_QUIGG_SHORT 700
#define PULSE_QUIGG_LONG 1400
#define PULSE_QUIGG_FOOTER 81000
#define PULSE_QUIGG_50 PULSE_QUIGG_SHORT+(PULSE_QUIGG_LONG-PULSE_QUIGG_SHORT)/2

static void quiggSwCreateMessage(int id, int state, int unit, int all) {
	quigg_switch->message = json_mkobject();
	json_append_member(quigg_switch->message, "id", json_mknumber(id));
	if(all==1) {
		json_append_member(quigg_switch->message, "all", json_mknumber(all));
	} else {
		json_append_member(quigg_switch->message, "unit", json_mknumber(unit));
	}

	if(state==1) {
		json_append_member(quigg_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(quigg_switch->message, "state", json_mkstring("off"));
	}
}

static void quiggSwParseCode(void) {
	int x = 0, dec_unit[4] = {0, 3, 1, 2};
	int iParity=1, iParityData=-1; // init for even parity

	for(x=0; x<quigg_switch->rawlen-1; x+=2) {
		if(quigg_switch->raw[x+1] > PULSE_QUIGG_50) {
			quigg_switch->binary[x/2] = 1;
			if((x / 2) > 11 && (x / 2) < 19) {
				iParityData = iParity;
				iParity =- iParity;
			}
		} else {
			quigg_switch->binary[x/2] = 0;
		}
	}

	if(iParityData < 0)
		iParityData=0;

	int id = binToDecRev(quigg_switch->binary, 0, 11);
	int unit = binToDecRev(quigg_switch->binary, 12, 13);
	int all = binToDecRev(quigg_switch->binary, 14, 14);
	int state = binToDecRev(quigg_switch->binary, 15, 15);
	int dimm = binToDecRev(quigg_switch->binary, 16, 16);
	int parity = binToDecRev(quigg_switch->binary, 19, 19);

	unit = dec_unit[unit];

	if((dimm == 1) && (state == 1))
		dimm = 2;

	if (iParityData == parity && dimm < 1) {
		quiggSwCreateMessage(id, state, unit, all);
	}
}

static void quiggSwCreateZero(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_switch->raw[i] = PULSE_QUIGG_SHORT;
		quigg_switch->raw[i+1] = PULSE_QUIGG_LONG;
	}
}

static void quiggSwCreateOne(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_switch->raw[i] = PULSE_QUIGG_LONG;
		quigg_switch->raw[i+1] = PULSE_QUIGG_SHORT;
	}
}

static void quiggSwCreateHeader(void) {
	quigg_switch->raw[0] = PULSE_QUIGG_SHORT;
}

static void quiggSwCreateFooter(void) {
	quigg_switch->raw[quigg_switch->rawlen-1] = PULSE_QUIGG_FOOTER;
}

static void quiggSwClearCode(void) {
	quiggSwCreateHeader();
	quiggSwCreateZero(1, quigg_switch->rawlen-3);
}

static void quiggSwCreateId(int id) {
	int binary[255];
	int length = 0;
	int i = 0, x = 0;

	x = 23;
	length = decToBin(id, binary);
	for(i=length;i>=0;i--) {
		if(binary[i] == 1) {
			quiggSwCreateOne(x, x+1);
		}
	x = x-2;
	}
}

static void quiggSwCreateUnit(int unit) {
	switch (unit) {
		case 0:
			quiggSwCreateZero(25, 30);	// 1st row
		break;
		case 1:
			quiggSwCreateOne(25, 26);	// 2nd row
			quiggSwCreateOne(37, 38);	// needs to be set
		break;
		case 2:
			quiggSwCreateOne(25, 28);	// 3rd row
			quiggSwCreateOne(37, 38);	// needs to be set
		break;
		case 3:
			quiggSwCreateOne(27, 28);	// 4th row
		break;
		case 4:
			quiggSwCreateOne(25, 30);	// 6th row MASTER (all)
		break;
		default:
		break;
	}
}

static void quiggSwCreateState(int state) {
	if(state==1)
		quiggSwCreateOne(31, 32); //on
}

static void quiggSwCreateParity(void) {
	int i,p;
	p = 1;			// init even parity, without system ID
	for(i=25;i<=37;i+=2) {
		if(quigg_switch->raw[i] == PULSE_QUIGG_LONG) {
			p = -p;
		}
	}
	if(p==-1) {
		quiggSwCreateOne(39, 40);
	}
}

static int quiggSwCreateCode(JsonNode *code) {
	int unit = -1;
	int id = -1;
	int state = -1;
	int all = 0;
	double itmp = -1;

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

	if(id==-1 || (unit==-1 && all==0) || state==-1) {
		logprintf(LOG_ERR, "quigg_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 4095 || id < 0) {
		logprintf(LOG_ERR, "quigg_switch: invalid programm code id range");
		return EXIT_FAILURE;
	} else if((unit > 3 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "quigg_switch: invalid button code unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 4;
		}
		quiggSwCreateMessage(id, state, unit, all);
		quiggSwClearCode();
		quiggSwCreateId(id);
		quiggSwCreateUnit(unit);
		quiggSwCreateState(state);
		quiggSwCreateParity();
		quiggSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void quiggSwPrintHelp(void) {
	printf("\t -i --id=id\t\t\tcontrol one or multiple devices with this id\n");
	printf("\t -u --unit=unit\t\t\tcontrol the device unit with this code\n");
	printf("\t -t --on\t\t\tsend an on signal to device\n");
	printf("\t -f --off\t\t\tsend an off signal to device\n");
	printf("\t -a --id=all\t\t\tcommand to all devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void quiggSwInit(void) {

	protocol_register(&quigg_switch);
	protocol_set_id(quigg_switch, "quigg_switch");
	protocol_device_add(quigg_switch, "quigg_switch", "Quigg Switches");
	protocol_plslen_add(quigg_switch, (int)PULSE_QUIGG_FOOTER/PULSE_DIV); // SHORT: GT-FSI-04a range: 620... 960
	quigg_switch->devtype = SWITCH;
	quigg_switch->hwtype = RF433;
	quigg_switch->pulse = 2;        // LONG=QUIGG_PULSE_HIGH*SHORT
	quigg_switch->lsb = 0;
	// 42 SHORT (>600)[0]; 20 times 0-(SHORT-LONG) or 1-(LONG-SHORT) [1-2 .. 39-40];
	// footer PULSE_DIV*SHORT (>6000) [41]
	quigg_switch->rawlen = 42;
	// 20 sys-id[0 .. 11]; unit[12,13], unit_all[14], on/off[15], dimm[16],
	// null[17], var[18]; Parity[19]
	quigg_switch->binlen = 20;

	options_add(&quigg_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&quigg_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-3][0-9][0-9][0-9]|40[0-8][0-9]|409[0-5])$");
	options_add(&quigg_switch->options, 'a', "all", OPTION_NO_VALUE, CONFIG_SETTING, JSON_NUMBER, NULL, NULL);

	options_add(&quigg_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	quigg_switch->parseCode=&quiggSwParseCode;
	quigg_switch->createCode=&quiggSwCreateCode;
	quigg_switch->printHelp=&quiggSwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "quigg_switch";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	quiggSwInit();
}
#endif
