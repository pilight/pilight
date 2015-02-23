/*
        Copyright (C) 2014 CurlyMo and edak (the author of this file)

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
#include "dogcollar.h"




static void dogcollarCreateMessage(int id, int function) {
        dogcollar->message = json_mkobject();
        json_append_member(dogcollar->message, "id", json_mknumber(id,0));
        json_append_member(dogcollar->message, "function", json_mknumber(function,0));
}


static void dogcollarCreateLow(int s, int e) {
        int i;

        for(i=s;i<=e;i+=2) {
                dogcollar->raw[i]=(dogcollar->plslen->length);
                dogcollar->raw[i+1]=(dogcollar->pulse*dogcollar->plslen->length);
        }
}


static void dogcollarCreateHigh(int s, int e) {
        int i;

        for(i=s;i<=e;i+=2) {
                dogcollar->raw[i]=(dogcollar->pulse*dogcollar->plslen->length);
                dogcollar->raw[i+1]=(dogcollar->plslen->length);
        }
}


static void dogcollarCreateId(int id) {
        int binary[255];
        int length = 0;
        int i=0, x=0;

        length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
                if(binary[i]==1) {
                        x=i*2;
                        dogcollarCreateHigh(x,x+1);
                }
        }
}

static void dogcollarClearCode(void) {
        dogcollarCreateLow(0,48);
        dogcollarCreateHigh(36,37);
}


static void dogcollarCreateFooter(void) {
        dogcollar->raw[48]=(2*dogcollar->plslen->length);
        dogcollar->raw[49]=(PULSE_DIV*dogcollar->plslen->length);
}

static void dogcollarCreateFunction(int function) {
	if(function == 1)
        	dogcollarCreateHigh(46,47);  //light
        if(function == 2)
                dogcollarCreateHigh(44,45);  //beep
        if(function == 3)
                dogcollarCreateHigh(42,43);  //vibrate low
        if(function == 4)
                dogcollarCreateHigh(40,41);  //vibrate high
}


static int dogcollarCreateCode(JsonNode *code) {
        int id = -1;
        int function = -1;
        double itmp = 0;

        if(json_find_number(code, "id", &itmp) == 0)
                id = (int)round(itmp);
        if(json_find_number(code, "function", &itmp) == 0)
                function = (int)round(itmp);

        if(id == -1 || function == -1 ) {
                logprintf(LOG_ERR, "dogcollar: insufficient number of arguments");
                return EXIT_FAILURE;
        } else if(id > 29999 || id < 1) {
                logprintf(LOG_ERR, "dogcollar: invalid id range");
                return EXIT_FAILURE;
        } else if(function > 4 || function < 1) {
                logprintf(LOG_ERR, "dogcollar: invalid function");
                return EXIT_FAILURE;
        } else {
                dogcollarCreateMessage(id, function);
		dogcollarClearCode();
                dogcollarCreateId(id);
                dogcollarCreateFunction(function);
                dogcollarCreateFooter();
        }
        return EXIT_SUCCESS;
}

static void dogcollarPrintHelp(void) {
        printf("\t -i --id=id\t\tcontrol a device with this id (1-29999)\n");
        printf("\t -f --function=function\t\t (1=light, 2=beep, 3=vibrate-low, 4=vibrate-high)\n");
}
#ifndef MODULE
__attribute__((weak))
#endif
void dogcollarInit(void) {
    protocol_register(&dogcollar);
    protocol_set_id(dogcollar, "dogcollar");
    protocol_device_add(dogcollar, "dogcollar", "Dog behaviour controller");
        protocol_plslen_add(dogcollar, 403);
        dogcollar->devtype = SCREEN;
        dogcollar->hwtype = RF433;
        dogcollar->pulse = 3;
        dogcollar->rawlen = 50;
        dogcollar->binlen = 24;

        options_add(&dogcollar->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]+$");
        options_add(&dogcollar->options, 'f', "function", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[1-4]$");

        options_add(&dogcollar->options, 0, "readonly", OPTION_HAS_VALUE,GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	dogcollar->createCode=&dogcollarCreateCode;
        dogcollar->printHelp=&dogcollarPrintHelp;
}
 
#ifdef MODULE
void compatibility(struct module_t *module) {
    module->name = "dogcollar";
    module->version = "0.1";
    module->reqversion = "5.0";
    module->reqcommit = NULL;
}
 
void init(void) {
    dogcollarInit();
}

#endif
