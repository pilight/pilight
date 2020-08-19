/*
	Copyright (C) 2014, 2018 CurlyMo & DonBernos & Michael Behrisch & Ligius

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
	
	Protocol info:
	From https://forum.pilight.org/showthread.php?tid=3351&pid=22798#pid22798
	ID ID ID CC         8-bit ID, the two least significant might encode the channel
	BF CC               4 bits of flags:
							B  =1 battery good
							F  =1 forced transmission
							CC =  channel, zero based
	TT TT TT TT TT TT   12 bits signed integer, temp in Celsius/10
	11 11               const / reserved
	HH HH HH HH         8 bits, either 
							- humidity (or 1111 xxxx if not available); or
							- a CRC, e.g. Rubicson, algorithm in source code linked above

	From: https://github.com/aquaticus/nexus433
	Every bit starts with 500 µs high pulse. 
	The length of the following low state decides if this is 1 (2000 µs) or 0 (1000 µs). 
	There is a 4000 µs long low state period before every 36 bits.

	See also https://github.com/merbanan/rtl_433/blob/master/src/devices/nexus.c
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/binary.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/gc.h"
#include "../../core/log.h"
#include "../../core/pilight.h"
#include "../protocol.h"
#include "nexus.h"

#define PULSE_TOLERANCE 150
#define MESSAGE_BITS 36
#define RAW_LENGTH (MESSAGE_BITS * 2 + 2)
#define MAXBITS (RAW_LENGTH / 2)

enum PulseType {
    START_P = 500,
    ZERO_P = 1000,
    ONE_P = 2000,
    SYNC_P = 4000
};

typedef struct settings_t {
    int id;
    int channel;
    int battery;
    double temperature_offset;
    double humidity_offset;
    double temperature_decimals;
    struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

/**
 * Validate whether a raw pulse length matches a known type
 */
static bool isValidPulse(uint16_t pulseLength, enum PulseType measureAgainst) {
    return (pulseLength > (measureAgainst - PULSE_TOLERANCE)) && (pulseLength < (measureAgainst + PULSE_TOLERANCE));
}

static int validate(void) {
    if(nexus->rawlen >= RAW_LENGTH && isValidPulse(nexus->raw[0], START_P) && isValidPulse(nexus->raw[RAW_LENGTH - 2], START_P)) {
        // the first and second-to-last pulse should be a short one
        return 0;
    }
    return -1;
}

static void parseCode(void) {
    int id = 0, battery = 0, channel = 0;
    double temperature = 0.0, humidity = 0.0;
    int binary[MAXBITS];
    int x = 0, i = 0;

    // decode pulses into bits, we only parse the needed amount and ignore everything after
    for(x = 1; x < RAW_LENGTH - 1; x += 2) {
        if(!isValidPulse(nexus->raw[x - 1], START_P)) {
            return;
        }
        if(isValidPulse(nexus->raw[x], ONE_P)) {
            binary[i++] = 1;
        } else if(isValidPulse(nexus->raw[x], ZERO_P)) {
            binary[i++] = 0;
        } else {
            // invalid pulse length
            return;
        }
    }

    // bit 10 should be 0 and bits 25-28 should be 1
    if(binary[9] != 0) {
        return;
    }
    if(binary[24] != 1 || binary[25] != 1 || binary[26] != 1 || binary[27] != 1) {
        return;
    }

    // decode bits into data
    id = binToDecRev(binary, 0, 7);
    battery = binary[8] ? 1 : 0;
    channel = binToDecRev(binary, 10, 11);
    temperature = (double)binToSignedRev(binary, 12, 23);
    humidity = (double)binToDecRev(binary, 28, 35);

    temperature /= 10;
    double temperature_decimals = 1;

    // find the matching settings struct; you cannot access settings->anyfield without this!
    struct settings_t *tmp = settings;
    while (tmp) {
        if (tmp->id == id) {
            // store or apply the settings
            temperature += settings->temperature_offset;
            humidity += settings->humidity_offset;
            temperature_decimals = settings->temperature_decimals;
            break;
        }
        tmp = tmp->next;
    }

    // build the JSON object
    nexus->message = json_mkobject();
    json_append_member(nexus->message, "id", json_mknumber(id, 0));
    json_append_member(nexus->message, "channel", json_mknumber(channel, 0));
    json_append_member(nexus->message, "battery", json_mknumber(battery, 0));
    json_append_member(nexus->message, "temperature", json_mknumber(temperature, temperature_decimals));
    json_append_member(nexus->message, "humidity", json_mknumber(humidity, 0));
}

static int checkValues(struct JsonNode *jvalues) {
    struct JsonNode *jid = NULL;

    if((jid = json_find_member(jvalues, "id"))) {
        struct settings_t *snode = NULL;
        struct JsonNode *jchild = NULL;
        struct JsonNode *jchild1 = NULL;
        double id = -1;
        int match = 0;

        jchild = json_first_child(jid);
        while(jchild) {
            jchild1 = json_first_child(jchild);
            while(jchild1) {
                if(strcmp(jchild1->key, "id") == 0) {
                    id = (int)jchild1->number_;
                }
                jchild1 = jchild1->next;
            }
            jchild = jchild->next;
        }

        struct settings_t *tmp = settings;
        while(tmp) {
            if(tmp->id == id) {
                match = 1;
                break;
            }
            tmp = tmp->next;
        }

        if(match == 0) {
            if((snode = MALLOC(sizeof(struct settings_t))) == NULL) {
                fprintf(stderr, "out of memory\n");
                exit(EXIT_FAILURE);
            }
            snode->id = id;
            snode->temperature_offset = 0;
            snode->humidity_offset = 0;
            snode->temperature_decimals = 1;

            json_find_number(jvalues, "temperature-offset", &snode->temperature_offset);
            json_find_number(jvalues, "humidity-offset", &snode->humidity_offset);
            json_find_number(jvalues, "temperature-decimals", &snode->temperature_decimals);

            snode->next = settings;
            settings = snode;
        }
    }
    return 0;
}

static void gc(void) {
    struct settings_t *tmp = NULL;
    while(settings) {
        tmp = settings;
        settings = settings->next;
        FREE(tmp);
    }
    if(settings != NULL) {
        FREE(settings);
    }
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void nexusInit(void) {
    protocol_register(&nexus);
    protocol_set_id(nexus, "nexus");
    protocol_device_add(nexus, "nexus", "Nexus Weather Stations");
    protocol_device_add(nexus, "dgr8h", "Digoo DG-R8H/DG-R8S Weather Stations");
    protocol_device_add(nexus, "sencor21ts", "Sencor SWS 21TS Weather Stations");

    nexus->devtype = WEATHER;
    nexus->hwtype = RF433;
    nexus->minrawlen = RAW_LENGTH;
    nexus->maxrawlen = RAW_LENGTH;
    nexus->mingaplen = (int)((double)SYNC_P * 0.9);  // minimum gap between pulse trains (sync pulse)
    nexus->maxgaplen = (int)((double)SYNC_P * 1.1);  // not used

    options_add(&nexus->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
    options_add(&nexus->options, "c", "channel", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-3]");
    options_add(&nexus->options, "b", "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");
    options_add(&nexus->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
    options_add(&nexus->options, "h", "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");

    options_add(&nexus->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
    options_add(&nexus->options, "0", "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
    options_add(&nexus->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
    options_add(&nexus->options, "0", "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
    options_add(&nexus->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
    options_add(&nexus->options, "0", "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
    options_add(&nexus->options, "0", "show-id", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
    options_add(&nexus->options, "0", "show-channel", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

    nexus->parseCode = &parseCode;
    nexus->checkValues = &checkValues;
    nexus->validate = &validate;
    nexus->gc = &gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "nexus";
    module->version = "1.0";
    module->reqversion = "6.0";
    module->reqcommit = "84";
}

void init(void) {
    nexusInit();
}
#endif