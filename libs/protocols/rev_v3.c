/*
    Copyright (C) 2014 CurlyMo & easy12

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
#include "rev_v3.h"

static void rev3CreateMessage(int id, int unit, int state) {
    rev3_switch->message = json_mkobject();
    json_append_member(rev3_switch->message, "id", json_mknumber(id));
    json_append_member(rev3_switch->message, "unit", json_mknumber(unit));
    if(state == 1) {
        json_append_member(rev3_switch->message, "state", json_mkstring("on"));
    } else {
        json_append_member(rev3_switch->message, "state", json_mkstring("off"));
	}
}

static void rev3ParseBinary(void) {
	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<rev3_switch->rawlen; x+=4) {
		if(rev3_switch->code[x+3] == 1) {
			rev3_switch->binary[x/4]=1;
		} else {
			rev3_switch->binary[x/4]=0;
		}
	}

	int unit = binToDec(rev3_switch->binary, 6, 9);
	int state = rev3_switch->binary[11];
	int id = binToDec(rev3_switch->binary, 0, 5);

	rev3CreateMessage(id, unit, state);
}

static void rev3CreateLow(int s, int e) {
    int i;

    for(i=s;i<=e;i+=4) {
        rev3_switch->raw[i]=(rev3_switch->plslen->length);
        rev3_switch->raw[i+1]=(rev3_switch->pulse*rev3_switch->plslen->length);
        rev3_switch->raw[i+2]=(rev3_switch->pulse*rev3_switch->plslen->length);
        rev3_switch->raw[i+3]=(rev3_switch->plslen->length);
    }
}
static void rev3CreateHigh(int s, int e) {
    int i;

    for(i=s;i<=e;i+=4) {
        rev3_switch->raw[i]=(rev3_switch->plslen->length);
        rev3_switch->raw[i+1]=(rev3_switch->pulse*rev3_switch->plslen->length);
        rev3_switch->raw[i+2]=(rev3_switch->plslen->length);
        rev3_switch->raw[i+3]=(rev3_switch->pulse*rev3_switch->plslen->length);
    }
}

static void rev3ClearCode(void) {
    rev3CreateHigh(0,3);
    rev3CreateLow(4,47);
}

static void rev3CreateUnit(int unit) {
    int binary[255];
    int length = 0;
    int i=0, x=0;

    length = decToBinRev(unit, binary);
    for(i=0;i<=length;i++) {
        if(binary[i]==1) {
            x=i*4;
            rev3CreateHigh(24+x, 24+x+3);
        }
    }
}

static void rev3CreateId(int id) {
    int binary[255];
    int length = 0;
    int i=0, x=0;

    length = decToBinRev(id, binary);
    for(i=0;i<=length;i++) {
        x=i*4;
        if(binary[i]==1) {
            rev3CreateHigh(x, x+3);
        }
    }
}

static void rev3CreateState(int state) {
    if(state == 0) {
        rev3CreateHigh(40,43);
        rev3CreateLow(44,47);
    } else {
        rev3CreateLow(40,43);
        rev3CreateHigh(44,47);
    }
}

static void rev3CreateFooter(void) {
    rev3_switch->raw[48]=(rev3_switch->plslen->length);
    rev3_switch->raw[49]=(PULSE_DIV*rev3_switch->plslen->length);
}

static int rev3CreateCode(JsonNode *code) {
    int id = -1;
    int unit = -1;
    int state = -1;
    double itmp = -1;

    if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);

    if(json_find_number(code, "off", &itmp) == 0)
        state=0;
    else if(json_find_number(code, "on", &itmp) == 0)
        state=1;

    if(json_find_number(code, "unit", &itmp) == 0)
        unit = (int)round(itmp);

    if(id == -1 || unit == -1 || state == -1) {
        logprintf(LOG_ERR, "rev3_switch: insufficient number of arguments");
        return EXIT_FAILURE;
    } else if(id > 63 || id < 0) {
        logprintf(LOG_ERR, "rev3_switch: invalid id range");
        return EXIT_FAILURE;
    } else if(unit > 15 || unit < 0) {
        logprintf(LOG_ERR, "rev3_switch: invalid unit range");
        return EXIT_FAILURE;
    } else {
        rev3CreateMessage(id, unit, state);
        rev3ClearCode();
        rev3CreateUnit(unit);
        rev3CreateId(id);
        rev3CreateState(state);
        rev3CreateFooter();
    }
    return EXIT_SUCCESS;
}

static void rev3PrintHelp(void) {
    printf("\t -t --on\t\t\tsend an on signal\n");
    printf("\t -f --off\t\t\tsend an off signal\n");
    printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
    printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void rev3Init(void) {

    protocol_register(&rev3_switch);
    protocol_set_id(rev3_switch, "rev3_switch");
    protocol_device_add(rev3_switch, "rev3_switch", "Rev Switches v3");
    protocol_plslen_add(rev3_switch, 264);
    protocol_plslen_add(rev3_switch, 258);
    rev3_switch->devtype = SWITCH;
    rev3_switch->hwtype = RF433;
    rev3_switch->pulse = 3;
    rev3_switch->rawlen = 50;
    rev3_switch->binlen = 12;

    options_add(&rev3_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
    options_add(&rev3_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
    options_add(&rev3_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]|[1][0-5])$");
    options_add(&rev3_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(6[0123]|[12345][0-9]|[0-9]{1})$");

    options_add(&rev3_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

    rev3_switch->parseBinary=&rev3ParseBinary;
    rev3_switch->createCode=&rev3CreateCode;
    rev3_switch->printHelp=&rev3PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "rev3_switch";
	module->version = "0.8";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	rev3Init();
}
#endif
