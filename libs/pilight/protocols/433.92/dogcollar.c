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
#include "dogcollar.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	408
#define MAX_PULSE_LENGTH	398
#define AVG_PULSE_LENGTH	403
#define RAW_LENGTH				50


static int validate(void) {
	if(dogcollar->rawlen == RAW_LENGTH) {
		if(dogcollar->raw[dogcollar->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   dogcollar->raw[dogcollar->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int id, int function) {
        dogcollar->message = json_mkobject();
        json_append_member(dogcollar->message, "id", json_mknumber(id,0));
        json_append_member(dogcollar->message, "function", json_mknumber(function,0));
}


static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		dogcollar->raw[i]=(AVG_PULSE_LENGTH);
		dogcollar->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}

}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		dogcollar->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		dogcollar->raw[i+1]=(AVG_PULSE_LENGTH);
	}
}
static void clearCode(void) {
        createLow(0,48);
        createHigh(36,37);
}

static void createId(int id) {
        int binary[255];
        int length = 0;
        int i=0, x=0;

        length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
                if(binary[i]==1) {
                        x=i*2;
                        createHigh(x,x+1);
                }
        }
}

static void createFooter(void) {
        dogcollar->raw[48]=(2*AVG_PULSE_LENGTH);
        dogcollar->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static void createFunction(int function) {
	if(function == 1)
     	createHigh(46,47);  //light
    if(function == 2)
        createHigh(44,45);  //beep
    if(function == 3)
        createHigh(42,43);  //vibrate low
    if(function == 4)
        createHigh(40,41);  //vibrate high
}


static int createCode(struct JsonNode *code) {
	int id = -1;
	int function = -1;
	double itmp = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "function", &itmp) == 0)
		function = (int)round(itmp);

	if(id == -1 || function == -1) {
		logprintf(LOG_ERR, "dogcollar: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 29999 || id < 1) {
		logprintf(LOG_ERR, "dogcollar: invalid id range");
		return EXIT_FAILURE;
	} else if(function > 4 || function < 1) {
		logprintf(LOG_ERR, "dogcollar: invalid function");
		return EXIT_FAILURE;
	} else {
        createMessage(id, function);
		clearCode();
        createId(id);
        createFunction(function);
        createFooter();
		dogcollar->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
        printf("\t -i --id=id\t\tcontrol a device with this id (1-29999)\n");
        printf("\t -f --function=function\t\t (1=light, 2=beep, 3=vibrate-low, 4=vibrate-high)\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void dogcollarInit(void) {

	protocol_register(&dogcollar);
	protocol_set_id(dogcollar, "dogcollar");
	protocol_device_add(dogcollar, "dogcollar", "dogcollar Switches");
	dogcollar->devtype = SCREEN;
	dogcollar->hwtype = RF433;
	dogcollar->minrawlen = RAW_LENGTH;
	dogcollar->maxrawlen = RAW_LENGTH;
	dogcollar->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	dogcollar->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

        options_add(&dogcollar->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]+$");
    options_add(&dogcollar->options, 'f', "function", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[1-4]$");

	options_add(&dogcollar->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&dogcollar->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	dogcollar->createCode=&createCode;
    dogcollar->printHelp=&printHelp;
	dogcollar->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "dogcollar";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	dogcollarInit();
}
#endif
