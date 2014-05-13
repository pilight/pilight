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
    along with pilight. If not, see    <http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "rev_old.h"

void revOldCreateMessage(int id, int unit, int state) {
    rev_old_switch->message = json_mkobject();
    json_append_member(rev_old_switch->message, "id", json_mknumber(id));
    json_append_member(rev_old_switch->message, "unit", json_mknumber(unit));
    if(state == 1) {
        json_append_member(rev_old_switch->message, "state", json_mkstring("on"));
    } else {
        json_append_member(rev_old_switch->message, "state", json_mkstring("off"));
	}
}

void revOldParseBinary(void) {      
	int x = 0;

	/* Convert the one's and zero's into binary */
	for(x=0; x<rev_old_switch->rawlen; x+=4) {
		if(rev_old_switch->code[x+3] == 1) {
			rev_old_switch->binary[x/4]=1;
		} else {
			rev_old_switch->binary[x/4]=0;
		}
	}

	int unit = binToDec(rev_old_switch->binary, 6, 9);
	int state = rev_old_switch->binary[11];
	int id = binToDec(rev_old_switch->binary, 0, 5);

	revOldCreateMessage(id, unit, state);
}

void revOldCreateLow(int s, int e) {
    int i;

    for(i=s;i<=e;i+=4) {
        rev_old_switch->raw[i]=(rev_old_switch->plslen->length);
        rev_old_switch->raw[i+1]=(rev_old_switch->pulse*rev_old_switch->plslen->length);
        rev_old_switch->raw[i+2]=(rev_old_switch->pulse*rev_old_switch->plslen->length);
        rev_old_switch->raw[i+3]=(rev_old_switch->plslen->length);
    }
}
void revOldCreateHigh(int s, int e) {
    int i;

    for(i=s;i<=e;i+=4) {
        rev_old_switch->raw[i]=(rev_old_switch->plslen->length);
        rev_old_switch->raw[i+1]=(rev_old_switch->pulse*rev_old_switch->plslen->length);
        rev_old_switch->raw[i+2]=(rev_old_switch->plslen->length);
        rev_old_switch->raw[i+3]=(rev_old_switch->pulse*rev_old_switch->plslen->length);
    }
}

void revOldClearCode(void) {
    revOldCreateHigh(0,3);
    revOldCreateLow(4,47);
}

void revOldCreateUnit(int unit) {
    int binary[255];
    int length = 0;
    int i=0, x=0;

    length = decToBinRev(unit, binary);
    for(i=0;i<=length;i++) {
        if(binary[i]==1) {
            x=i*4;
            revOldCreateHigh(24+x, 24+x+3);
        }
    }
}

void revOldCreateId(int id) {
    int binary[255];
    int length = 0;
    int i=0, x=0;
    
    length = decToBinRev(id, binary);
    for(i=0;i<=length;i++) {
        x=i*4;
        if(binary[i]==1) {
            revOldCreateHigh(x, x+3);
        }
    }
}

void revOldCreateState(int state) {
    if(state == 0) {
        revOldCreateHigh(40,43);
        revOldCreateLow(44,47);
    } else {
        revOldCreateLow(40,43);
        revOldCreateHigh(44,47);
    }
}

void revOldCreateFooter(void) {
    rev_old_switch->raw[48]=(rev_old_switch->plslen->length);
    rev_old_switch->raw[49]=(PULSE_DIV*rev_old_switch->plslen->length);
}

int revOldCreateCode(JsonNode *code) {
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
        logprintf(LOG_ERR, "rev_old_switch: insufficient number of arguments");
        return EXIT_FAILURE;
    } else if(id > 63 || id < 0) {
        logprintf(LOG_ERR, "rev_old_switch: invalid id range");
        return EXIT_FAILURE;
    } else if(unit > 15 || unit < 0) {
        logprintf(LOG_ERR, "rev_old_switch: invalid unit range");
        return EXIT_FAILURE;
    } else {
        revOldCreateMessage(id, unit, state);
        revOldClearCode();
        revOldCreateUnit(unit);
        revOldCreateId(id);
        revOldCreateState(state);
        revOldCreateFooter();
    }
    return EXIT_SUCCESS;
}

void revOldPrintHelp(void) {
    printf("\t -t --on\t\t\tsend an on signal\n");
    printf("\t -f --off\t\t\tsend an off signal\n");
    printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
    printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void revOldInit(void) {

    protocol_register(&rev_old_switch);
    protocol_set_id(rev_old_switch, "rev_switch_old");    
    protocol_device_add(rev_old_switch, "rev_switch_old", "REV Old Switches");
    protocol_plslen_add(rev_old_switch, 258);
    rev_old_switch->devtype = SWITCH;
    rev_old_switch->hwtype = RF433;
    rev_old_switch->pulse = 3;
    rev_old_switch->rawlen = 50;
    rev_old_switch->binlen = 12;

    options_add(&rev_old_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
    options_add(&rev_old_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
    options_add(&rev_old_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]|[1][0-5])$");
    options_add(&rev_old_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^(6[0123]|[12345][0-9]|[0-9]{1})$");

    options_add(&rev_old_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
    
    rev_old_switch->parseBinary=&revOldParseBinary;
    rev_old_switch->createCode=&revOldCreateCode;
    rev_old_switch->printHelp=&revOldPrintHelp;
}

void compatibility(const char **version, const char **commit) {
	*version = "4.0";
	*commit = "18";
}

void init(void) {
	revOldInit();
}
