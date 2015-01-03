/*
        Copyright (C) 2014 CurlyMo and Edak (the author of this file)

        This file is part of pilight.

        pilight is free software: you can redistribute it and/or modify it under the
        terms of the GNU General Public License as published by the Free Software
        Foundation, either version 3 of the License, or (at your option) any later
        version.

        pilight is distributed in the hope that it will be useful, but WITHOUT ANY
        WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
        A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with pilight. If not, see <http://www.gnu.org/licenses/>
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
#include "kambrook.h"




// Message format for the kambrook power adaptors is  as follows:
//	01010101 IIIIIIII IIIIIIII IIIIIIII CCCCCCCC 11111111 
//	Where:
//		01010101 = start of message
//		I = Device ID (A 24-bit number normally unique per pack of three switches and hardcoded in the remote contoller)
//		C = Device Code (a combination of the Unit code and the state code)
//		11111111 = end of message
//
//	The Unit code corresponds to the letters and numbers on the original remote, where A1=1, A5=5, B1=6, ... ,D5=20
// 	To keep the code to a minimum only 16 bits were used for the device ID, the remainder are zeroed, this is also because I have never seen a remote with a code this high

static void kambrookCreateMessage(int id, int unit, int state) {
        kambrook->message = json_mkobject();
        json_append_member(kambrook->message, "id", json_mknumber(id,0));
        json_append_member(kambrook->message, "unit", json_mknumber(unit,0));
        if(state == 0) {
                json_append_member(kambrook->message, "state", json_mkstring("on"));
        } else {
                json_append_member(kambrook->message, "state", json_mkstring("off"));
        }
}


static void kambrookCreateLow(int s, int e) {
        int i;

        for(i=s;i<=e;i+=2) {
                kambrook->raw[i]=(kambrook->plslen->length);
                kambrook->raw[i+1]=(kambrook->plslen->length);
        }
}


static void kambrookCreateHigh(int s, int e) {
        int i;

        for(i=s;i<=e;i+=2) {
                kambrook->raw[i]=(kambrook->pulse*kambrook->plslen->length);
                kambrook->raw[i+1]=(kambrook->plslen->length);
        }
}

static void kambrookCreateHeader(void){
        int i;
        for(i=2;i<16;i+=4) {
                kambrookCreateHigh(i,i+1);
        }
}

static void kambrookCreateId(int id) {
        int binary[255];
        int length = 0;
        int i=0, x=0;

        length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
                if(binary[i]==1) {
                        x=i*2;
                        kambrookCreateHigh(62-x,62-x+1);
                }
        }
}

static void kambrookClearCode(void) {
        kambrookCreateLow(0,94);
}


static void kambrookCreateFooter(void) {
        kambrookCreateHigh(80,94);
        kambrook->raw[95]=(PULSE_DIV*kambrook->plslen->length);
}

static void kambrookCreateOverallCode(int unit, int state) {
        int binary[255];
        int length = 0;
        int i=0, x=0;
	int overallcode=0;

	while(unit>5) {
		overallcode+=16;
		unit-=5;
	}
	overallcode+=(unit*2)-1+state;

        length = decToBinRev(overallcode, binary);
        for(i=0;i<=length;i++) {
                if(binary[i]==1) {
                        x=i*2;
                        kambrookCreateHigh(78-x, 78-x+1);
                }
        }
}


static int kambrookCreateCode(JsonNode *code) {
        int id = -1;
        int unit = -1;
        int state = -1;
        double itmp = 0;

        if(json_find_number(code, "id", &itmp) == 0)
                id = (int)round(itmp);
        if(json_find_number(code, "unit", &itmp) == 0)
                unit = (int)round(itmp);
        if(json_find_number(code, "off", &itmp) == 0)
                state=1;
        else if(json_find_number(code, "on", &itmp) == 0)
                state=0;

        if(id == -1 || unit == -1 || state == -1) {
                logprintf(LOG_ERR, "kambrook: insufficient number of arguments");
                return EXIT_FAILURE;
        } else if(id > 29999 || id < 1) {
                logprintf(LOG_ERR, "kambrook: invalid id range");
                return EXIT_FAILURE;
        } else if(unit > 19 || unit < 1) {
                logprintf(LOG_ERR, "kambrook: invalid unit range");
                return EXIT_FAILURE;
        } else {
                kambrookCreateMessage(id, unit, state);
		kambrookClearCode();
                kambrookCreateHeader();
                kambrookCreateId(id);
                kambrookCreateOverallCode(unit,state);
                kambrookCreateFooter();
        }
        return EXIT_SUCCESS;
}

static void kambrookPrintHelp(void) {
        printf("\t -i --id=id\tcontrol a device with this id (systemcode) (1-29999)\n");
        printf("\t -u --unit=unit\t\tcontrol a device with this unit (1-19)\n");
        printf("\t -t --on\t\t\tsend an on signal\n");
        printf("\t -f --off\t\t\tsend an off signal\n");}
 
#ifndef MODULE
__attribute__((weak))
#endif
void kambrookInit(void) {
    protocol_register(&kambrook);
    protocol_set_id(kambrook, "kambrook");
    protocol_device_add(kambrook, "kambrook", "Kambrook Powerpoint protocol");
        protocol_plslen_add(kambrook, 284);
        kambrook->devtype = SWITCH;
        kambrook->hwtype = RF433;
        kambrook->pulse = 2;
        kambrook->rawlen = 96;
        kambrook->binlen = 48;

        options_add(&kambrook->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(([0-2][0-9][0-9][0-9][0-9])|\\d{4}|\\d{3}|\\d{2}|[1-9])$");
        options_add(&kambrook->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-1]?[0-9])$");
        options_add(&kambrook->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
        options_add(&kambrook->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

        options_add(&kambrook->options, 0, "readonly", OPTION_HAS_VALUE,GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	kambrook->createCode=&kambrookCreateCode;
        kambrook->printHelp=&kambrookPrintHelp;
}
 
#ifdef MODULE
void compatibility(struct module_t *module) {
    module->name = "kambrook";
    module->version = "0.1";
    module->reqversion = "5.0";
    module->reqcommit = NULL;
}
 
void init(void) {
    kambrookInit();
}

#endif
