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
#include "quigg_gt7000_screen.h"

#define	PULSE_QUIGG_SCREEN_SHORT	700
#define	PULSE_QUIGG_SCREEN_LONG		1400
#define	PULSE_QUIGG_SCREEN_FOOTER	81000
#define	PULSE_QUIGG_SCREEN_50		PULSE_QUIGG_SCREEN_SHORT+(PULSE_QUIGG_SCREEN_LONG-PULSE_QUIGG_SCREEN_SHORT)/2

static void quiggGT7000SrCreateMessage(int id, int state, int unit, int all) {
	// logprintf(LOG_DEBUG, "CreateMessage id:%d state:%d unit:%d all:%d",id, state, unit, all);

	quigg_gt7000_screen->message = json_mkobject();
	json_append_member(quigg_gt7000_screen->message, "id", json_mknumber(id, 0));
	if(all==1) {
		json_append_member(quigg_gt7000_screen->message, "all", json_mknumber(all, 0));
	} else {
		json_append_member(quigg_gt7000_screen->message, "unit", json_mknumber(unit, 0));
	}
	if(state==0) {
		json_append_member(quigg_gt7000_screen->message, "state", json_mkstring("up"));
	} else {
		json_append_member(quigg_gt7000_screen->message, "state", json_mkstring("down"));
	}
}

static void quiggGT7000SrParseCode(void) {
	int x = 0, dec_unit[4] = {0, 3, 1, 2};
	int iParity = 1, iParityData = -1;	// init for even parity
	int iSwitch = 0;

	// 42 bytes are the number of raw bytes
	// Byte 1,2 in raw buffer is the first logical byte, rawlen-3,-2 is the parity bit, rawlen-1 is the footer
	for(x=0; x<quigg_gt7000_screen->rawlen-1; x+=2) {
		if(quigg_gt7000_screen->raw[x+1] > PULSE_QUIGG_SCREEN_50) {
			quigg_gt7000_screen->binary[x/2] = 1;
			if((x/2) > 11 && (x/2) < 19) {
				iParityData=iParity;
				iParity=-iParity;
			}
		} else {
			quigg_gt7000_screen->binary[x/2] = 0;
		}
	}
	if(iParityData < 0)
		iParityData=0;

	int id = binToDec(quigg_gt7000_screen->binary, 0, 11);
	int unit = binToDecRev(quigg_gt7000_screen->binary, 12, 13);
	int all = binToDecRev(quigg_gt7000_screen->binary, 14, 14);
	int state = binToDecRev(quigg_gt7000_screen->binary, 15, 15);
	int screen = binToDecRev(quigg_gt7000_screen->binary, 16, 16);
	int parity = binToDecRev(quigg_gt7000_screen->binary, 19, 19);

	unit = dec_unit[unit];

	// Prepare dim up/down mode for appropriate handling with ScreenCreateMessage
	iSwitch = (screen * 2) + state;	// 00-turn off 01-turn on 10-dim up 11-dim down

	switch(iSwitch) {
		case 0:
		case 1:
			screen=-1;	// mode handled by quigg_switch protocol driver
		break;
		case 2:
		case 3:
		default:
		break;
	}
	if((iParityData == parity) && (screen != -1)) {
		quiggGT7000SrCreateMessage(id, state, unit, all);
	}
}

static void quiggGT7000SrCreateZero(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_gt7000_screen->raw[i] = PULSE_QUIGG_SCREEN_SHORT;
		quigg_gt7000_screen->raw[i+1] = PULSE_QUIGG_SCREEN_LONG;
	}
}

static void quiggGT7000SrCreateOne(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_gt7000_screen->raw[i] = PULSE_QUIGG_SCREEN_LONG;
		quigg_gt7000_screen->raw[i+1] = PULSE_QUIGG_SCREEN_SHORT;
	}
}

static void quiggGT7000SrCreateHeader(void) {
	quigg_gt7000_screen->raw[0] = PULSE_QUIGG_SCREEN_SHORT;
}

static void quiggGT7000SrCreateFooter(void) {
	quigg_gt7000_screen->raw[quigg_gt7000_screen->rawlen-1] = PULSE_QUIGG_SCREEN_FOOTER;
}

static void quiggGT7000SrClearCode(void) {
	quiggGT7000SrCreateHeader();
	quiggGT7000SrCreateZero(1,quigg_gt7000_screen->rawlen-3);
}

static void quiggGT7000SrCreateId(int id) {
	int binary[255];
	int length;
	int i, x;

	x = 1;
	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i] == 1) {
			quiggGT7000SrCreateOne(x, x+1);
		}
	x = x+2;
	}
}

static void quiggGT7000SrCreateUnit(int unit) {
	switch (unit) {
		case 0:
			quiggGT7000SrCreateZero(25, 30);	// Unit 0 screen
			quiggGT7000SrCreateOne(33, 34);
			quiggGT7000SrCreateOne(37, 38);
		break;
		case 1:
			quiggGT7000SrCreateOne(25, 26);	// Unit 1 screen
			quiggGT7000SrCreateOne(33, 34);
		break;
		case 2:
			quiggGT7000SrCreateOne(25, 28);	// Unit 2 screen
			quiggGT7000SrCreateOne(33, 34);
		break;
		case 3:
			quiggGT7000SrCreateOne(27, 28);	// Unit 3 screen
			quiggGT7000SrCreateOne(33, 34);
			quiggGT7000SrCreateOne(37, 38);
		break;
		case 4:
			quiggGT7000SrCreateOne(25, 30);	// MASTER (all) screen
			quiggGT7000SrCreateOne(33, 34);
			quiggGT7000SrCreateOne(37, 38);
		break;
		default:
		break;
	}
}

static void quiggGT7000SrCreateState(int state) {
	if(state==1)
		quiggGT7000SrCreateOne(31, 32);	// dim down
}

static void quiggGT7000SrCreateParity(void) {
	int i, p = 1;	// init even parity, without system ID
	for(i=25;i<=37;i+=2) {
		if(quigg_gt7000_screen->raw[i] == PULSE_QUIGG_SCREEN_LONG) {
			p = -p;
		}
	}
	if(p == -1) {
		quiggGT7000SrCreateOne(39,40);
	}
}

static int quiggGT7000SrCreateCode(JsonNode *code) {
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

	if(json_find_number(code, "up", &itmp) == 0)
		state=0;
	if(json_find_number(code, "down", &itmp) == 0)
		state=1;

	if(id==-1 || (unit==-1 && all==0) || state==-1) {
		logprintf(LOG_ERR, "quigg_gt7000_screen: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 4095 || id < 0) {
		logprintf(LOG_ERR, "quigg_gt7000_screen: invalid system id range");
		return EXIT_FAILURE;
	} else if((unit > 3 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "quigg_gt7000_screen: invalid unit code range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 4;
		}

		quiggGT7000SrCreateMessage(id, state, unit, all);
		quiggGT7000SrClearCode();
		quiggGT7000SrCreateId(id);
		quiggGT7000SrCreateUnit(unit);
		quiggGT7000SrCreateState(state);
		quiggGT7000SrCreateParity();
		quiggGT7000SrCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void quiggGT7000SrPrintHelp(void) {
	printf("\t -i --id=id\t\t\tselect the device units with this system id\n");
	printf("\t -a --id=all\t\t\tcontrol all devices selected with the system id\n");
	printf("\t -u --unit=unit\t\t\tcontrol the individual device unit selected with the system id\n");
	printf("\t -f --up\t\t\tsend an darken DOWN command to the selected device\n");
	printf("\t -t --down\t\t\tsend an brighten UP command to the selected device\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void quiggGT7000SrInit(void) {

	protocol_register(&quigg_gt7000_screen);
	protocol_set_id(quigg_gt7000_screen, "quigg_gt7000_screen");
	protocol_device_add(quigg_gt7000_screen, "quigg_gt7000_screen", "Quigg Switch Screen");
	protocol_plslen_add(quigg_gt7000_screen, (int)PULSE_QUIGG_SCREEN_FOOTER/PULSE_DIV);	// SHORT: GT-FSI-04a range: 620... 960

	quigg_gt7000_screen->devtype = SCREEN;
	quigg_gt7000_screen->hwtype = RF433;
	quigg_gt7000_screen->pulse = 2;	// LONG=QUIGG_PULSE_HIGH*SHORT
	quigg_gt7000_screen->lsb = 0;
	quigg_gt7000_screen->rawlen = 42;	// 42 SHORT (>600)[0]; 20 times 0-(SHORT-LONG) or 1-(LONG-SHORT) [1-2 .. 39-40];
								// footer PULSE_DIV*SHORT (>6000) [41]
	quigg_gt7000_screen->binlen = 20;	// 20 sys-id[0 .. 11]; unit[12,13], unit_all[14], on/off[15], screen[16],
								// null[17], var[18]; Parity[19]

	options_add(&quigg_gt7000_screen->options, 'a', "all", OPTION_NO_VALUE, DEVICES_SETTING, JSON_NUMBER, NULL, NULL);
	options_add(&quigg_gt7000_screen->options, 't', "up", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_gt7000_screen->options, 'f', "down", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_gt7000_screen->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&quigg_gt7000_screen->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-3][0-9][0-9][0-9]|40[0-8][0-9]|409[0-5])$");

	options_add(&quigg_gt7000_screen->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	quigg_gt7000_screen->parseCode=&quiggGT7000SrParseCode;
	quigg_gt7000_screen->createCode=&quiggGT7000SrCreateCode;
	quigg_gt7000_screen->printHelp=&quiggGT7000SrPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "quigg_gt7000_screen";
	module->version = "1.4";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	quiggGT7000SrInit();
}
#endif

