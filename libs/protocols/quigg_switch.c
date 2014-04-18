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
/*
    Copyright (C) 2014 wo_rasp
    
    Globaltronics / Quigg GT-7000 Protocol support
    Support for Aldi / Tevion Power Switches GT-Fsi-04a
    14.4.2014 V0.9
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "quigg_switch.h"

void quiGGSwCreateMessage(int id, int state, int unit, int all) {
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

void quiGGSwParseCode(void) {

	int unit = binToDecRev(quigg_switch->binary, 25, 28);
	int id = binToDecRev(quigg_switch->binary, 3, 12);
	int all = binToDecRev(quigg_switch->binary, 29, 30);
	int state = binToDecRev(quigg_switch->binary, 31, 32);
	int dimm = binToDecRev(quigg_switch->binary, 33, 34);

	if (dimm==1 && unit==3) unit = 5;
	quiGGSwCreateMessage(id, state, unit, all);
}

void quiGGSwCreateLow(int s, int e) {
	int i;
	for(i = s; i<=e; i+=2) {
		quigg_switch->raw[i] = ((quigg_switch->pulse+1)*quigg_switch->plslen->length);
		quigg_switch->raw[i+1] = quigg_switch->plslen->length*10;
	}
}

void quiGGSwCreateHigh(int s, int e) {
	int i;
	for(i = s; i<=e; i+=2) {
		quigg_switch->raw[i] = quigg_switch->plslen->length*10;
		quigg_switch->raw[i+1] = ((quigg_switch->pulse+1)*quigg_switch->plslen->length);
	}
}

void quiGGSwClearCode(void) {
	quigg_switch->raw[0] = 700;
	quiGGSwCreateLow(1,39);
}

void quiGGSwCreateStart(void) {
	quiGGSwCreateHigh(1, 2);	// Sync
}

void quiGGSwCreateId(int id) {
        int binary[255];
        int length = 0;
        int i = 0, x = 0;

	x = 11;
        length = decToBin(id, binary);
        for(i = length; i>=0; i--) {
                if((binary[i]==1)) {
                        quiGGSwCreateHigh(x, x+1);
                }
		x = x-2;
        }
}

void quiGGSwCreateUnit(int unit) {
                switch (unit) {
			case 0:
			quiGGSwCreateLow(25, 30);	// 1
                        break;
			case 1:
			quiGGSwCreateHigh(25, 26);	// 2
			quiGGSwCreateHigh(37, 38);	// why ???
			break;
			case 2:
			quiGGSwCreateHigh(25, 28);	// 3
			quiGGSwCreateHigh(37, 38);	// why ???
			break;
			case 3:
			quiGGSwCreateHigh(27, 28);	// 4
			break;
			case 4:
			quiGGSwCreateHigh(27, 28);	// Dimm
			quiGGSwCreateHigh(33, 34);
			quiGGSwCreateHigh(37, 38);
			case 5:
			quiGGSwCreateHigh(25, 30);	// All
			break;
			default:
			break;
                }
}

void quiGGSwCreateState(int state) {
	if(state==1) {
		quiGGSwCreateHigh(31, 32);
	} else {
		quiGGSwCreateLow(31, 32);
	}
}

void quiGGSwCreateParity(void) {
	int i,p;
	p = -1;
	for (i = 1; i<=37; i=i+2) {
		if ((quigg_switch->raw[i]==quigg_switch->plslen->length*10)) {
			p = -p;
		}
	}
	if ((p==-1)) {
		quiGGSwCreateHigh(39,40);
	}
}

void quiGGSwCreateFooter(void) {
	quigg_switch->raw[41]=PULSE_DIV*quigg_switch->plslen->length*2;
}

int quiGGSwCreateCode(JsonNode *code) {
	int unit = -1;
	int id = -1;
	int state = -1;
	int all = 0;
	int tmp;

        json_find_number(code, "id", &id);
        json_find_number(code, "unit", &unit);
        json_find_number(code, "all", &all);
        if(json_find_number(code, "off", &tmp)==0)
                state=0;
        else if(json_find_number(code, "on", &tmp)==0)
                state = 1;

        if(id==-1 || (unit==-1 && all==0) || state==-1) {
                logprintf(LOG_ERR, "quigg_switch: insufficient number of arguments");
                return EXIT_FAILURE;
        } else if(id>31 || id<0) {
                logprintf(LOG_ERR, "quigg_switch: invalid programm code id range");
                return EXIT_FAILURE;
        } else if((unit > 4 || unit < 0) && all == 0) {
                logprintf(LOG_ERR, "quigg_switch: invalid button code unit range");
                return EXIT_FAILURE;
        } else {
                if(unit==-1 && all==1) {
                        unit = 5;
                }
		quiGGSwCreateMessage(id, state, unit, all);
		quiGGSwClearCode();
		quiGGSwCreateStart();
		quiGGSwCreateId(id);
		quiGGSwCreateUnit(unit);
		quiGGSwCreateState(state);
		quiGGSwCreateParity();
		quiGGSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

void quiGGSwPrintHelp(void) {
	printf("\t -i --id=id\t\tcontrol one or multiple devices with this id\n");
	printf("\t -u --unit=id\t\tcontrol the device unit with this code\n");
	printf("\t -t --on\t\t\tsend an on signal to device\n");
	printf("\t -f --off\t\t\tsend an off signal to device\n");
	printf("\t -a --id=all\t\tcommand to all devices with this id\n");
}

void quiGGSwInit(void) {

	protocol_register(&quigg_switch);
	protocol_set_id(quigg_switch, "quigg_switch");
	protocol_device_add(quigg_switch, "quigg_switch", "Quigg GT7000 Switches");
	protocol_plslen_add(quigg_switch, 120);
	quigg_switch->devtype = SWITCH;
	quigg_switch->hwtype = RF433;
	quigg_switch->pulse = 5;
	quigg_switch->rawlen = 42;  // 42 start: low-short(700); 20 times high-low 0(short-long) or 1(long/short)
                                    //                 and footer: high-footer 548 times (65760)
	quigg_switch->binlen = 21;  // 20 id-dev[5], id-dev[5], null[1], unit[2], unit_all[1]
                                    //          on/off[1], dimm[1], null[1], var[1], Parity[1]

	options_add(&quigg_switch->options, 'a', "all", no_value, 0, NULL);
	options_add(&quigg_switch->options, 't', "on", no_value, config_state, NULL);
	options_add(&quigg_switch->options, 'f', "off", no_value, config_state, NULL);
	options_add(&quigg_switch->options, 'i', "id", has_value, config_id, "^([1-9]|([1-2][0-9]|[3][0-1])$");
	options_add(&quigg_switch->options, 'u', "unit", has_value, config_id, "^([0-4])$");

	protocol_setting_add_string(quigg_switch, "states", "on,off");
	protocol_setting_add_number(quigg_switch, "readonly", 0);

	quigg_switch->parseCode=&quiGGSwParseCode;
	quigg_switch->createCode=&quiGGSwCreateCode;
	quigg_switch->printHelp=&quiGGSwPrintHelp;
}
