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
#include "quigg_screen.h"

#define	PULSE_QUIGG_SCREEN_SHORT	700
#define	PULSE_QUIGG_SCREEN_LONG	1400
#define	PULSE_QUIGG_SCREEN_FOOTER	81000
#define	PULSE_QUIGG_SCREEN_50		PULSE_QUIGG_SCREEN_SHORT+(PULSE_QUIGG_SCREEN_LONG-PULSE_QUIGG_SCREEN_SHORT)/2

static void quiggScreenCreateMessage(int id, int state, int unit, int all) {
	// logprintf(LOG_DEBUG, "CreateMessage id:%d state:%d unit:%d all:%d",id, state, unit, all);

	quigg_screen->message = json_mkobject();
	json_append_member(quigg_screen->message, "id", json_mknumber(id));
	if(all==1) {
		json_append_member(quigg_screen->message, "all", json_mknumber(all));
	} else {
		json_append_member(quigg_screen->message, "unit", json_mknumber(unit));
	}
	if(state==0) {
		json_append_member(quigg_screen->message, "state", json_mkstring("up"));
	} else {
		json_append_member(quigg_screen->message, "state", json_mkstring("down"));
	}
}

static void quiggScreenParseCode(void) {
	int x = 0, dec_unit[4] = {0, 3, 1, 2};
	int iParity = 1, iParityData = -1;	// init for even parity
	int iSwitch = 0;

	// 42 bytes are the number of raw bytes
	// Byte 1,2 in raw buffer is the first logical byte, rawlen-3,-2 is the parity bit, rawlen-1 is the footer
	for(x=0; x<quigg_screen->rawlen-1; x+=2) {
		if(quigg_screen->raw[x+1] > PULSE_QUIGG_SCREEN_50) {
			quigg_screen->binary[x/2] = 1;
			if((x/2) > 11 && (x/2) < 19) {
				iParityData=iParity;
				iParity=-iParity;
			}
		} else {
			quigg_screen->binary[x/2] = 0;
		}
	}
	if(iParityData < 0)
		iParityData=0;

	int id = binToDecRev(quigg_screen->binary, 0, 11);
	int unit = binToDecRev(quigg_screen->binary, 12, 13);
	int all = binToDecRev(quigg_screen->binary, 14, 14);
	int state = binToDecRev(quigg_screen->binary, 15, 15);
	int screen = binToDecRev(quigg_screen->binary, 16, 16);
	int parity = binToDecRev(quigg_screen->binary, 19, 19);

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
		quiggScreenCreateMessage(id, state, unit, all);
	}
}

static void quiggScreenCreateZero(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_screen->raw[i] = PULSE_QUIGG_SCREEN_SHORT;
		quigg_screen->raw[i+1] = PULSE_QUIGG_SCREEN_LONG;
	}
}

static void quiggScreenCreateOne(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		quigg_screen->raw[i] = PULSE_QUIGG_SCREEN_LONG;
		quigg_screen->raw[i+1] = PULSE_QUIGG_SCREEN_SHORT;
	}
}

static void quiggScreenCreateHeader(void) {
	quigg_screen->raw[0] = PULSE_QUIGG_SCREEN_SHORT;
}

static void quiggScreenCreateFooter(void) {
	quigg_screen->raw[quigg_screen->rawlen-1] = PULSE_QUIGG_SCREEN_FOOTER;
}

static void quiggScreenClearCode(void) {
	quiggScreenCreateHeader();
	quiggScreenCreateZero(1,quigg_screen->rawlen-3);
}

static void quiggScreenCreateId(int id) {
	int binary[255];
	int length = 0;
	int i = 0, x = 0;

	x = 23;
	length = decToBin(id, binary);
	for(i=length;i>=0;i--) {
		if(binary[i] == 1) {
			quiggScreenCreateOne(x, x+1);
		}
		x = x-2;
	}
}

static void quiggScreenCreateUnit(int unit) {
	switch (unit) {
		case 0:
			quiggScreenCreateZero(25, 30);	// Unit 0 screen
			quiggScreenCreateOne(33, 34);
			quiggScreenCreateOne(37, 38);
		break;
		case 1:
			quiggScreenCreateOne(25, 26);	// Unit 1 screen
			quiggScreenCreateOne(33, 34);
		break;
		case 2:
			quiggScreenCreateOne(25, 28);	// Unit 2 screen
			quiggScreenCreateOne(33, 34);
		break;
		case 3:
			quiggScreenCreateOne(27, 28);	// Unit 3 screen
			quiggScreenCreateOne(33, 34);
			quiggScreenCreateOne(37, 38);
		break;
		case 4:
			quiggScreenCreateOne(25, 30);	// MASTER (all) screen
			quiggScreenCreateOne(33, 34);
			quiggScreenCreateOne(37, 38);
		break;
		default:
		break;
	}
}

static void quiggScreenCreateState(int state) {
	if(state==1)
		quiggScreenCreateOne(31, 32);	// dim down
}

static void quiggScreenCreateParity(void) {
	int i, p = 1;	// init even parity, without system ID
	for(i=25;i<=37;i+=2) {
		if(quigg_screen->raw[i] == PULSE_QUIGG_SCREEN_LONG) {
			p = -p;
		}
	}
	if(p == -1) {
		quiggScreenCreateOne(39,40);
	}
}

static int quiggScreenCreateCode(JsonNode *code) {
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
		logprintf(LOG_ERR, "quigg_screen: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 4095 || id < 0) {
		logprintf(LOG_ERR, "quigg_screen: invalid system id range");
		return EXIT_FAILURE;
	} else if((unit > 3 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "quigg_screen: invalid unit code range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 4;
		}

		quiggScreenCreateMessage(id, state, unit, all);
		quiggScreenClearCode();
		quiggScreenCreateId(id);
		quiggScreenCreateUnit(unit);
		quiggScreenCreateState(state);
		quiggScreenCreateParity();
		quiggScreenCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void quiggScreenPrintHelp(void) {
	printf("\t -i --id=id\t\t\tselect the device units with this system id\n");
	printf("\t -a --id=all\t\t\tcontrol all devices selected with the system id\n");
	printf("\t -u --unit=unit\t\t\tcontrol the individual device unit selected with the system id\n");
	printf("\t -f --up\t\t\tsend an darken DOWN command to the selected device\n");
	printf("\t -t --down\t\t\tsend an brighten UP command to the selected device\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void quiggScreenInit(void) {

	protocol_register(&quigg_screen);
	protocol_set_id(quigg_screen, "quigg_screen");
	protocol_device_add(quigg_screen, "quigg_screen", "Quigg Switch Screen");
	protocol_plslen_add(quigg_screen, (int)PULSE_QUIGG_SCREEN_FOOTER/PULSE_DIV);	// SHORT: GT-FSI-04a range: 620... 960

	quigg_screen->devtype = SCREEN;
	quigg_screen->hwtype = RF433;
	quigg_screen->pulse = 2;	// LONG=QUIGG_PULSE_HIGH*SHORT
	quigg_screen->lsb = 0;
	quigg_screen->rawlen = 42;	// 42 SHORT (>600)[0]; 20 times 0-(SHORT-LONG) or 1-(LONG-SHORT) [1-2 .. 39-40];
								// footer PULSE_DIV*SHORT (>6000) [41]
	quigg_screen->binlen = 20;	// 20 sys-id[0 .. 11]; unit[12,13], unit_all[14], on/off[15], screen[16],
								// null[17], var[18]; Parity[19]

	options_add(&quigg_screen->options, 'a', "all", OPTION_NO_VALUE, CONFIG_SETTING, JSON_NUMBER, NULL, NULL);
	options_add(&quigg_screen->options, 't', "up", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_screen->options, 'f', "down", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&quigg_screen->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&quigg_screen->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-3][0-9][0-9][0-9]|40[0-8][0-9]|409[0-5])$");

	options_add(&quigg_screen->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	quigg_screen->parseCode=&quiggScreenParseCode;
	quigg_screen->createCode=&quiggScreenCreateCode;
	quigg_screen->printHelp=&quiggScreenPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "quigg_screen";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	quiggScreenInit();
}
#endif

