/*
	Copyright (C) 2014 CurlyMo

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

	Rafael: original was this beamish_switch.c , i have changed this for Daycom Switches to daycom.c   feb,2015
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
#include "daycom.h"

#define PULSE_MULTIPLIER        4
#define MIN_PULSE_LENGTH        280
#define MAX_PULSE_LENGTH        296
#define AVG_PULSE_LENGTH        284
#define RAW_LENGTH              50

static int validate(void) {
        if(daycom->rawlen == RAW_LENGTH) {
                if(daycom->raw[daycom->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
                   daycom->raw[daycom->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
                        return 0;
                }
        }

        return -1;
}

static void daycomCreateMessage(int id, int systemcode, int unit, int state) {
	daycom->message = json_mkobject();
	json_append_member(daycom->message, "id", json_mknumber(id, 0));
	json_append_member(daycom->message, "systemcode", json_mknumber(systemcode, 0));
	json_append_member(daycom->message, "unit", json_mknumber(unit, 0));
	if(state == 0) {
		json_append_member(daycom->message, "state", json_mkstring("on"));
	} else {
		json_append_member(daycom->message, "state", json_mkstring("off"));
	}
}

static void daycomParseCode(void) {
        int binary[RAW_LENGTH/2], x = 0, i = 0;

        int id = -1, state = -1, unit = -1, systemcode = -1;


        for(x=0;i<daycom->rawlen;x+=2) {
                if(daycom->raw[x] > AVG_PULSE_LENGTH*(PULSE_MULTIPLIER/2)) {
                        binary[i++] = 1;
                } else {
                        binary[i++] = 0;
                }
        }

        id = binToDecRev(binary, 0, 5);
        systemcode = binToDecRev(binary, 6, 19);
        unit = binToDecRev(binary, 21, 23 );
        state = binary[20];
        daycomCreateMessage(id, systemcode, unit, state);
}

static void daycomCreateLow(int s, int e) {
        int i;
        for(i=s;i<=e;i+=2) {
                daycom->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
                daycom->raw[i+1]=(AVG_PULSE_LENGTH);
        }
}
static void daycomCreateHigh(int s, int e) {
        int i;
        for(i=s;i<=e;i+=2) {
                daycom->raw[i]=(AVG_PULSE_LENGTH);
                daycom->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
        }
}

static void daycomClearCode(void) {
	daycomCreateHigh(0,47);
}

static void daycomCreateId(int id) {
        int binary[255];
        int length = 0;
        int i=0, x=0;

        length = decToBinRev(id, binary);
        for(i=0;i<=length;i++) {
                if(binary[i]==1) {
                        x=i*2;
                        daycomCreateLow(11-(x+1), 11-x);
                }
        }
}


static void daycomCreateSystemCode(int systemcode) {
        int binary[255];
        int length = 0;
        int i=0, x=0;

        length = decToBinRev(systemcode, binary);
        for(i=0;i<=length;i++) {
                if(binary[i]==1) {
                        x=i*2;
                        daycomCreateLow(39-(x+1), 39-x);
                }
        }
}

static void daycomCreateUnit(int unit) {
        int binary[255];
        int length = 0;
        int i=0, x=0;

        length = decToBinRev(unit, binary);
        for(i=0;i<=length;i++) {
                if(binary[i]==1) {
                        x=i*2;
                        daycomCreateLow(47-(x+1), 47-x);
                }
        }
}

static void daycomCreateState(int state) {
        if(state == 0) {
                daycomCreateLow(40, 41);
        }
}


static void daycomCreateFooter(void) {
        daycom->raw[48]=(AVG_PULSE_LENGTH);
        daycom->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int daycomCreateCode(JsonNode *code) {
        int id = -1;
        int systemcode = -1;
        int unit = -1;
        int state = -1;
        double itmp = -1;

        if(json_find_number(code, "id", &itmp) == 0)
                id = (int)round(itmp);
        if(json_find_number(code, "systemcode", &itmp) == 0)
                systemcode = (int)round(itmp);
        if(json_find_number(code, "unit", &itmp) == 0)
                unit = (int)round(itmp);
        if(json_find_number(code, "off", &itmp) == 0)
                state=1;
        if(json_find_number(code, "on", &itmp) == 0)
                state=0;

        if(id == -1 || systemcode == -1 || unit == -1 || state == -1) {
                logprintf(LOG_ERR, "daycom: insufficient number of arguments");
                return EXIT_FAILURE;
        } else if(id > 63 || id < 0) {
                logprintf(LOG_ERR, "daycom: invalid id range");
                return EXIT_FAILURE;
        } else if(systemcode > 16383 || systemcode < 0) {
                logprintf(LOG_ERR, "daycom: invalid systemcode range");
                return EXIT_FAILURE;
        } else if(unit > 7 || unit < 0) {
                logprintf(LOG_ERR, "daycom: invalid unit range");
                return EXIT_FAILURE;
        } else {
                daycomCreateMessage(id, systemcode, unit, state);
                daycomClearCode();
                daycomCreateId(id);
                daycomCreateSystemCode(systemcode);
                daycomCreateState(state);
                daycomCreateUnit(unit);
                daycomCreateFooter();
		daycom->rawlen = RAW_LENGTH;
		state = 0;
        }
        return EXIT_SUCCESS;
}

static void daycomPrintHelp(void) {
	printf("\t -i --id=Ident\t the Transmitter\n");
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif

void daycomInit(void) {

	protocol_register(&daycom);
	protocol_set_id(daycom, "daycom");
	protocol_device_add(daycom, "daycom", "Daycom Switches");
        daycom->devtype = SWITCH;
        daycom->hwtype = RF433;
        daycom->minrawlen = RAW_LENGTH;
        daycom->maxrawlen = RAW_LENGTH;
        daycom->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
        daycom->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

        options_add(&daycom->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
        options_add(&daycom->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
        options_add(&daycom->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-7])$");
        options_add(&daycom->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
        options_add(&daycom->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-6]?[0-9]$");

	options_add(&daycom->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

        daycom->parseCode=&daycomParseCode;
        daycom->createCode=&daycomCreateCode;
        daycom->printHelp=&daycomPrintHelp;
        daycom->validate=&validate;
}
#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
        module->name = "daycom";
        module->version = "0.02";
        module->reqversion = "6.0";
        module->reqcommit = "2";
}

void init(void) {
	daycomInit();
}
#endif
