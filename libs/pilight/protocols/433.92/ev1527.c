/*
	Copyright (C) 2015 CurlyMo and Meloen

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

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "ev1527.h"


#define LEARN_REPEATS		40
#define NORMAL_REPEATS		10

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	340
#define MAX_PULSE_LENGTH	400
#define AVG_PULSE_LENGTH	370
#define RAW_LENGTH		50

static int validate(void) {
	if(ev1527->rawlen == RAW_LENGTH) {
		if(ev1527->raw[ev1527->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   ev1527->raw[ev1527->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int unitcode, int state, int state2, int state3, int state4) {
	ev1527->message = json_mkobject();
	json_append_member(ev1527->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 0) {
		json_append_member(ev1527->message, "state", json_mkstring("off"));
	} else {
		json_append_member(ev1527->message, "state", json_mkstring("on"));
	}
        if(state2 == 0) {
                json_append_member(ev1527->message, "state2", json_mkstring("off"));
        } else {
                json_append_member(ev1527->message, "state2", json_mkstring("on"));
        }
        if(state3 == 0) {
                json_append_member(ev1527->message, "state3", json_mkstring("off"));
        } else {
                json_append_member(ev1527->message, "state3", json_mkstring("on"));
        }
        if(state4 == 0) {
                json_append_member(ev1527->message, "state4", json_mkstring("off"));
        } else {
                json_append_member(ev1527->message, "state4", json_mkstring("on"));
        }

}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], x = 0, i = 0;

	for(x=0;x<ev1527->rawlen-2;x+=2) {
		if(ev1527->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	int unitcode = binToDec(binary, 0, 19);
	int state = binary[20];
        int state2 = binary[21];
        int state3 = binary[22];
        int state4 = binary[23];
        createMessage(unitcode, state, state2, state3, state4);
}

static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		ev1527->raw[i]=(AVG_PULSE_LENGTH);
		ev1527->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);

	}
}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		ev1527->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		ev1527->raw[i+1]=(AVG_PULSE_LENGTH);

	}
}

static void createUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	if (length>19) return;
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			createHigh(x, x+1);
		}
	}
}

static void createState(int state[]) {
	if(state[0] == 1) {
		createHigh(40, 41);
	} else {
		createLow(40, 41);
	}

	if(state[1] == 1) {
		createHigh(42, 43);
	} else {
		createLow(42, 43);
	}

	if(state[2] == 1) {
		createHigh(44, 45);
	} else {
		createLow(44, 45);
	}

	if(state[3] == 1) {
		createHigh(46, 47);
	} else {
		createLow(46, 47);
	}

}

static void clearCode(void) {
	createLow(0,48);
}

static void createFooter(void) {
	ev1527->raw[48]=(AVG_PULSE_LENGTH);
	ev1527->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code) {
	int unitcode = -1;
	int poscode = -1;
	int state[4];
	double itmp = 0;

	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);

	if(json_find_number(code, "positioncode", &itmp) == 0)
		poscode = (int)round(itmp);

	if(unitcode == -1 || poscode == -1) {
		logprintf(LOG_ERR, "ev1527: insufficient number of arguments");
		return EXIT_FAILURE;
	} 

	if(poscode > 4 || poscode < 1) {
		logprintf(LOG_ERR, "ev1527: bad value of an argument");
		return EXIT_FAILURE;
        }

	state[0] = 0;
	state[1] = 0;
	state[2] = 0; 
	state[3] = 0;
	if(json_find_number(code, "on", &itmp) == 0)
		state[poscode-1]=1;

	createMessage(unitcode, state[0], state[1], state[2], state[3]);
	clearCode();
	createUnitCode(unitcode);
	createState(state);
	createFooter();
	ev1527->rawlen = RAW_LENGTH;

	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -u --unitcode=code\tcontrol a device with this code\n");
	printf("\t -c --positioncode=code\tcontrol a device position (1-4)\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void ev1527Init(void) {

	protocol_register(&ev1527);
	protocol_set_id(ev1527, "ev1527");
	protocol_device_add(ev1527, "ev1527", "ev1527 4 bit sensor or switch");
	ev1527->devtype = SWITCH;
	ev1527->hwtype = RF433;
        ev1527->txrpt = NORMAL_REPEATS;
	ev1527->minrawlen = RAW_LENGTH;
	ev1527->maxrawlen = RAW_LENGTH;
	ev1527->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	ev1527->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&ev1527->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&ev1527->options, 'c', "positioncode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, (void*)1, "^[1234]{1}$");
	options_add(&ev1527->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ev1527->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	ev1527->parseCode=&parseCode;
	ev1527->createCode=&createCode;
	ev1527->printHelp=&printHelp;
	ev1527->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ev1527";
	module->version = "2.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	ev1527Init();
}
#endif
