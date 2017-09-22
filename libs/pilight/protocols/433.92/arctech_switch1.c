/*
       Copyright (C) 2013 CurlyMo

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

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "arctech_switch1.h"

#define LEARN_REPEATS                  40
#define NORMAL_REPEATS         10
#define PULSE_MULTIPLIER       4
#define MIN_PULSE_LENGTH       250
#define MAX_PULSE_LENGTH       320
#define AVG_PULSE_LENGTH       315
#define RAW_LENGTH                             132

static int counter = 0;
static int nr = 0;
static int id = 0;
static int state = 0;
static int unit = 0;
static int all = 0;

static void createMessage(char **message, int id, int unit, int state, int all, int learn) {
       int x = snprintf((*message), 255, "{\"id\":%d,", id);
       if(all == 1) {
               x += snprintf(&(*message)[x], 255-x, "\"all\":1,");
       } else {
               x += snprintf(&(*message)[x], 255-x, "\"unit\":%d,", unit);
       }

       if(state == 1) {
               x += snprintf(&(*message)[x], 255-x, "\"state\":\"on\"");
       } else {
               x += snprintf(&(*message)[x], 255-x, "\"state\":\"off\"");
       }
       x += snprintf(&(*message)[x], 255-x, "}");

       if(learn == 1) {
               arctech_switch1->txrpt = LEARN_REPEATS;
       } else {
               arctech_switch1->txrpt = NORMAL_REPEATS;
       }
}

static void parseCode(int pulse) {
       if(pulse > 10000) {
               state = 0, all = 0, unit = 0;
               counter = 0, nr = 0, id = 0;
       }

       if((++counter % 4) == 1) {
               nr++;
               if(nr > 2 && nr < 29) {
                       if(pulse > 1000) {
                               id |= (1 << (29-nr-2));
                       }
               }
               if(nr == 29) {
                       if(pulse > 1000) {
                               state = 1;
                       } else {
                               state = 0;
                       }
               }
               if(nr == 28) {
                       if(pulse > 1000) {
                               all = 1;
                       } else {
                               all = 0;
                       }
               }
               if(nr > 31 && nr < 34) {
                       if(pulse > 1000) {
                               unit |= (1 << (34-nr));
                       }
               }
       }
       if(counter == 132) {
               char message[255], *p = message;
               createMessage(&p, id, unit, state, all, 0);
               printf("%s\n", message);
       }
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void arctechSwitch1Init(void) {
       protocol_register(&arctech_switch1);
       protocol_set_id(arctech_switch1, "arctech_switch1");
       arctech_switch1->devtype = SWITCH;
       arctech_switch1->hwtype = RF433_1;
       arctech_switch1->txrpt = NORMAL_REPEATS;
       arctech_switch1->minrawlen = RAW_LENGTH;
       arctech_switch1->maxrawlen = RAW_LENGTH;
       arctech_switch1->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
       arctech_switch1->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

       options_add(&arctech_switch1->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
       options_add(&arctech_switch1->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
       options_add(&arctech_switch1->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
       options_add(&arctech_switch1->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
       options_add(&arctech_switch1->options, 'a', "all", OPTION_OPT_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);
       options_add(&arctech_switch1->options, 'l', "learn", OPTION_NO_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

       options_add(&arctech_switch1->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
       options_add(&arctech_switch1->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

       arctech_switch1->parseCode1=&parseCode;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
       module->name = "arctech_switch1";
       module->version = "3.4";
       module->reqversion = "6.0";
       module->reqcommit = "84";
}

void init(void) {
       arctechSwitch1Init();
}
#endif
