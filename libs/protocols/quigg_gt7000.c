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
#include "quigg_gt7000.h"

#define PULSE_QUIGG_SHORT 700
#define PULSE_QUIGG_LONG 1400
#define PULSE_QUIGG_FOOTER 81000
#define PULSE_QUIGG_50 PULSE_QUIGG_SHORT+(PULSE_QUIGG_LONG-PULSE_QUIGG_SHORT)/2

static void quiggGT7000CreateMessage(int id, int state, int unit, int all) {
	quigg_gt7000->message = json_mkobject();
	json_append_member(quigg_gt7000->message, "id", json_mknumber(id, 0));
	if(all==1) {
		json_append_member(quigg_gt7000->message, "all", json_mknumber(all, 0));
	} else {
		json_append_member(quigg_gt7000->message, "unit", json_mknumber(unit, 0));
	}

	if(state==1) {
		json_append_member(quigg_gt7000->message, "state", json_mkstring("on"));
	} else {
		json_append_member(quigg_gt7000->message, "state", json_mkstring("off"));
	}
}

static void quiggGT7000ParseCode(void) {
	int x = 0, dec_unit[4] = {0, 3, 1, 2};
	int iParity=1, iParityData=-1; // init for even parity

	for(x=0; x<quigg_gt7000->rawlen-1; x+=2) {
		if(quigg_gt7000->raw[x+1] > PULSE_QUIGG_50) {
			quigg_gt7000->binary[x/2] = 1;
			if((x / 2) > 11 && (x / 2) < 19) {
				iParityData = iParity;
				iParity = -iParity;
			}
		} else {
			quigg_gt7000->binary[x/2] = 0;
		}
	}

	if(iParityData < 0)
		iParityData=0;

	int id = binToDecRev(quigg_gt7000->binary, 0, 11);
	int unit = binToDecRev(quigg_gt7000->binary, 12, 13);
	int all = binToDecRev(quigg_gt7000->binary, 14, 14);
	int state = binToDecRev(quigg_gt7000->binary, 15, 15);
	int dimm = binToDecRev(quigg_gt7000->binary, 16, 16);
	int parity = binToDecRev(quigg_gt7000->binary, 19, 19);

	unit = dec_unit[unit];

	if((dimm == 1) && (state == 1))
		dimm = 2;

	if (iParityData == parity && dimm < 1) {
		quiggGT7000CreateMessage(id, state, unit, all);
	}
}

static void quiggGT7000CreateZero(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_gt7000->raw[i] = PULSE_QUIGG_SHORT;
		quigg_gt7000->raw[i+1] = PULSE_QUIGG_LONG;
	}
}

static void quiggGT7000CreateOne(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_gt7000->raw[i] = PULSE_QUIGG_LONG;
		quigg_gt7000->raw[i+1] = PULSE_QUIGG_SHORT;
	}
}

static void quiggGT7000CreateHeader(void) {
	quigg_gt7000->raw[0] = PULSE_QUIGG_SHORT;
}

static void quiggGT7000CreateFooter(void) {
	quigg_gt7000->raw[quigg_gt7000->rawlen-1] = PULSE_QUIGG_FOOTER;
}

static void quiggGT7000ClearCode(void) {
	quiggGT7000CreateHeader();
	quiggGT7000CreateZero(1, quigg_gt7000->rawlen-3);
}

static void quiggGT7000CreateId(int id) {
	int binary[255];
	int length = 0;
	int i = 0, x = 0;

	x = 23;
	length = decToBin(id, binary);
	for(i=length;i>=0;i--) {
		if(binary[i] == 1) {
			quiggGT7000CreateOne(x, x+1);
		}
	x = x-2;
	}
}

static void quiggGT7000CreateUnit(int unit) {
	switch (unit) {
		case 0:
			quiggGT7000CreateZero(25, 30);	// 1st row
		break;
		case 1:
			quiggGT7000CreateOne(25, 26);	// 2nd row
			quiggGT7000CreateOne(37, 38);	// needs to be set
		break;
		case 2:
			quiggGT7000CreateOne(25, 28);	// 3rd row
			quiggGT7000CreateOne(37, 38);	// needs to be set
		break;
		case 3:
			quiggGT7000CreateOne(27, 28);	// 4th row
		break;
		case 4:
			quiggGT7000CreateOne(25, 30);	// 6th row MASTER (all)
		break;
		default:
		break;
	}
}

static void quiggGT7000CreateState(int state) {
	if(state==1)
		quiggGT7000CreateOne(31, 32); //on
}

static void quiggGT7000CreateParity(void) {
	int i,p;
	p = 1;			// init even parity, without system ID
	for(i=25;i<=37;i+=2) {
		if(quigg_gt7000->raw[i] == PULSE_QUIGG_LONG) {
			p = -p;
		}
	}
	if(p==-1) {
		quiggGT7000CreateOne(39, 40);
	}
}

static int quiggGT7000CreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "quigg_gt7000: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 4095 || id < 0) {
		logprintf(LOG_ERR, "quigg_gt7000: invalid programm code id range");
		return EXIT_FAILURE;
	} else if((unit > 3 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "quigg_gt7000: invalid button code unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 4;
		}
		quiggGT7000CreateMessage(id, state, unit, all);
		quiggGT7000ClearCode();
		quiggGT7000CreateId(id);
		quiggGT7000CreateUnit(unit);
		quiggGT7000CreateState(state);
		quiggGT7000CreateParity();
		quiggGT7000CreateFooter();
	}
	return EXIT_SUCCESS;
}

static void quiggGT7000PrintHelp(void) {
	printf("\t -i --id=id\t\t\tcontrol one or multiple devices with this id\n");
	printf("\t -u --unit=unit\t\t\tcontrol the device unit with this code\n");
	printf("\t -t --on\t\t\tsend an on signal to device\n");
	printf("\t -f --off\t\t\tsend an off signal to device\n");
	printf("\t -a --id=all\t\t\tcommand to all devices with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void quiggGT7000Init(void) {

	protocol_register(&quigg_gt7000);
	protocol_set_id(quigg_gt7000, "quigg_gt7000");
	protocol_device_add(quigg_gt7000, "quigg_gt7000", "Quigg GT-7000 Switches");
	protocol_plslen_add(quigg_gt7000, (int)PULSE_QUIGG_FOOTER/PULSE_DIV); // SHORT: GT-FSI-04a range: 620... 960
	quigg_gt7000->devtype = SWITCH;
	quigg_gt7000->hwtype = RF433;
	quigg_gt7000->pulse = 2;        // LONG=QUIGG_PULSE_HIGH*SHORT
	quigg_gt7000->lsb = 0;
	// 42 SHORT (>600)[0]; 20 times 0-(SHORT-LONG) or 1-(LONG-SHORT) [1-2 .. 39-40];
	// footer PULSE_DIV*SHORT (>6000) [41]
	quigg_gt7000->rawlen = 42;
	// 20 sys-id[0 .. 11]; unit[12,13], unit_all[14], on/off[15], dimm[16],
	// null[17], var[18]; Parity[19]
	quigg_gt7000->binlen = 20;

	options_add(&quigg_gt7000->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_gt7000->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_gt7000->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&quigg_gt7000->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-3][0-9][0-9][0-9]|40[0-8][0-9]|409[0-5])$");
	options_add(&quigg_gt7000->options, 'a', "all", OPTION_NO_VALUE, DEVICES_SETTING, JSON_NUMBER, NULL, NULL);

	options_add(&quigg_gt7000->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	quigg_gt7000->parseCode=&quiggGT7000ParseCode;
	quigg_gt7000->createCode=&quiggGT7000CreateCode;
	quigg_gt7000->printHelp=&quiggGT7000PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "quigg_gt7000";
	module->version = "1.3";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	quiggGT7000Init();
}
#endif
