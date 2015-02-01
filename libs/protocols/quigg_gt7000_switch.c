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
#include "quigg_gt7000_switch.h"

#define PULSE_QUIGG_SHORT	700
#define PULSE_QUIGG_LONG	1400
#define PULSE_QUIGG_FOOTER	81000
#define PULSE_QUIGG_50 PULSE_QUIGG_SHORT+(PULSE_QUIGG_LONG-PULSE_QUIGG_SHORT)/2

static void quiggGT7000SwCreateMessage(int id, int state, int unit, int all) {
	quigg_gt7000_switch->message = json_mkobject();
	json_append_member(quigg_gt7000_switch->message, "id", json_mknumber(id, 0));
	if(all==1) {
		json_append_member(quigg_gt7000_switch->message, "all", json_mknumber(all, 0));
	} else {
		json_append_member(quigg_gt7000_switch->message, "unit", json_mknumber(unit, 0));
	}

	if(state==1) {
		json_append_member(quigg_gt7000_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(quigg_gt7000_switch->message, "state", json_mkstring("off"));
	}
}

static void quiggGT7000SwParseCode(void) {
	int x = 0, dec_unit[4] = {0, 3, 1, 2};
	int iParity=1, iParityData=-1; // init for even parity

	for(x=0; x<quigg_gt7000_switch->rawlen-1; x+=2) {
		if(quigg_gt7000_switch->raw[x+1] > PULSE_QUIGG_50) {
			quigg_gt7000_switch->binary[x/2] = 1;
			if((x / 2) > 11 && (x / 2) < 19) {
				iParityData = iParity;
				iParity = -iParity;
			}
		} else {
			quigg_gt7000_switch->binary[x/2] = 0;
		}
	}

	if(iParityData < 0)
		iParityData=0;

	int id = binToDec(quigg_gt7000_switch->binary, 0, 11);
	int unit = binToDecRev(quigg_gt7000_switch->binary, 12, 13);
	int all = binToDecRev(quigg_gt7000_switch->binary, 14, 14);
	int state = binToDecRev(quigg_gt7000_switch->binary, 15, 15);
	int dimm = binToDecRev(quigg_gt7000_switch->binary, 16, 16);
	int parity = binToDecRev(quigg_gt7000_switch->binary, 19, 19);

	unit = dec_unit[unit];

	if((dimm == 1) && (state == 1))		// Mode is handled by quigg_gt7000_screen protocol
		dimm = 2;

	if (iParityData == parity && dimm < 1) {
		quiggGT7000SwCreateMessage(id, state, unit, all);
	}
}

static void quiggGT7000SwCreateZero(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_gt7000_switch->raw[i] = PULSE_QUIGG_SHORT;
		quigg_gt7000_switch->raw[i+1] = PULSE_QUIGG_LONG;
	}
}

static void quiggGT7000SwCreateOne(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_gt7000_switch->raw[i] = PULSE_QUIGG_LONG;
		quigg_gt7000_switch->raw[i+1] = PULSE_QUIGG_SHORT;
	}
}

static void quiggGT7000SwCreateHeader(void) {
	quigg_gt7000_switch->raw[0] = PULSE_QUIGG_SHORT;
}

static void quiggGT7000SwCreateFooter(void) {
	quigg_gt7000_switch->raw[quigg_gt7000_switch->rawlen-1] = PULSE_QUIGG_FOOTER;
}

static void quiggGT7000SwClearCode(void) {
	quiggGT7000SwCreateHeader();
	quiggGT7000SwCreateZero(1, quigg_gt7000_switch->rawlen-3);
}

static void quiggGT7000SwCreateId(int id) {
	int binary[255];
	int length;
	int i, x;

	x = 1;
	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i] == 1) {
			quiggGT7000SwCreateOne(x, x+1);
		}
	x = x+2;
	}
}

static void quiggGT7000SwCreateUnit(int unit) {
	switch (unit) {
		case 0:
			quiggGT7000SwCreateZero(25, 30);	// 1st row
		break;
		case 1:
			quiggGT7000SwCreateOne(25, 26);	// 2nd row
			quiggGT7000SwCreateOne(37, 38);	// needs to be set
		break;
		case 2:
			quiggGT7000SwCreateOne(25, 28);	// 3rd row
			quiggGT7000SwCreateOne(37, 38);	// needs to be set
		break;
		case 3:
			quiggGT7000SwCreateOne(27, 28);	// 4th row
		break;
		case 4:
			quiggGT7000SwCreateOne(25, 30);	// 6th row MASTER (all)
		break;
		default:
		break;
	}
}

static void quiggGT7000SwCreateState(int state) {
	if(state==1)
		quiggGT7000SwCreateOne(31, 32); //on
}

static void quiggGT7000SwCreateParity(void) {
	int i,p;
	p = 1;			// init even parity, without system ID
	for(i=25;i<=37;i+=2) {
		if(quigg_gt7000_switch->raw[i] == PULSE_QUIGG_LONG) {
			p = -p;
		}
	}
	if(p==-1) {
		quiggGT7000SwCreateOne(39, 40);
	}
}

static int quiggGT7000SwCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "quigg_gt7000_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 4095 || id < 0) {
		logprintf(LOG_ERR, "quigg_gt7000_switch: invalid programm code id range");
		return EXIT_FAILURE;
	} else if((unit > 3 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "quigg_gt7000_switch: invalid button code unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 4;
		}
		quiggGT7000SwCreateMessage(id, state, unit, all);
		quiggGT7000SwClearCode();
		quiggGT7000SwCreateId(id);
		quiggGT7000SwCreateUnit(unit);
		quiggGT7000SwCreateState(state);
		quiggGT7000SwCreateParity();
		quiggGT7000SwCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void quiggGT7000SwPrintHelp(void) {
	printf("\t -i --id=id\t\t\tcontrol one or multiple devices with this id\n");
	printf("\t -u --unit=unit\t\t\tcontrol the device unit with this code\n");
	printf("\t -t --on\t\t\tsend an on signal to device\n");
	printf("\t -f --off\t\t\tsend an off signal to device\n");
	printf("\t -a --id=all\t\t\tcommand to all devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void quiggGT7000SwInit(void) {

	protocol_register(&quigg_gt7000_switch);
	protocol_set_id(quigg_gt7000_switch, "quigg_gt7000_switch");
	protocol_device_add(quigg_gt7000_switch, "quigg_gt7000_switch", "Quigg GT-7000 Switches");
	protocol_plslen_add(quigg_gt7000_switch, (int)PULSE_QUIGG_FOOTER/PULSE_DIV); // SHORT: GT-FSI-04a range: 620... 960
	quigg_gt7000_switch->devtype = SWITCH;
	quigg_gt7000_switch->hwtype = RF433;
	quigg_gt7000_switch->pulse = 2;        // LONG=QUIGG_PULSE_HIGH*SHORT
	quigg_gt7000_switch->lsb = 0;
	// 42 SHORT (>600)[0]; 20 times 0-(SHORT-LONG) or 1-(LONG-SHORT) [1-2 .. 39-40];
	// footer PULSE_DIV*SHORT (>6000) [41]
	quigg_gt7000_switch->rawlen = 42;
	// 20 sys-id[0 .. 11]; unit[12,13], unit_all[14], on/off[15], dimm[16],
	// null[17], var[18]; Parity[19]
	quigg_gt7000_switch->binlen = 20;

	options_add(&quigg_gt7000_switch->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_gt7000_switch->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_gt7000_switch->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&quigg_gt7000_switch->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-3][0-9][0-9][0-9]|40[0-8][0-9]|409[0-5])$");
	options_add(&quigg_gt7000_switch->options, 'a', "all", OPTION_NO_VALUE, DEVICES_SETTING, JSON_NUMBER, NULL, NULL);

	options_add(&quigg_gt7000_switch->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	quigg_gt7000_switch->parseCode=&quiggGT7000SwParseCode;
	quigg_gt7000_switch->createCode=&quiggGT7000SwCreateCode;
	quigg_gt7000_switch->printHelp=&quiggGT7000SwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "quigg_gt7000_switch";
	module->version = "1.4";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	quiggGT7000SwInit();
}
#endif
