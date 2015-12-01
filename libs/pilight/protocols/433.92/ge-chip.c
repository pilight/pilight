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
#include "ge_chip.h"

#define PULSE_MULTIPLIER	20
#define MIN_PULSE_LENGTH	261
#define MAX_PULSE_LENGTH	271
#define AVG_PULSE_LENGTH	266
#define RAW_LENGTH				76

typedef struct settings_t {
	double id;
	double ch,
	double temp;
	double humi;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(ge_chip->rawlen == RAW_LENGTH) {
		if(ge_chip->raw[ge_chip->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   ge_chip->raw[ge_chip->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(void) {
	int i = 0, x = 0, binary[RAW_LENGTH/2];
	int id = 0, battery = 0, channel = 0;
	double temperature = 0.0, humidity = 0.0;
	double humi_offset = 0.0, temp_offset = 0.0;

	for(x=1;x<ge_chip->rawlen-1;x+=2) {
		if(ge_chip->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	id = binToDecRev(binary, 0, 13);
	channel = binToDecRev(binary, 14, 15);
	battery = binary[16];
	temperature = binToDecRev(binary, 17, 27);
	humidity = binToDecRev(binary, 28, 35);

	struct settings_t *tmp = settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON) {
			humi_offset = tmp->humi;
			temp_offset = tmp->temp;
			break;
		}
		tmp = tmp->next;
	}
	
	temperature += temp_offset;
	humidity += humi_offset;

	ge_chip->message = json_mkobject();
	json_append_member(ge_chip->message, "id", json_mknumber(id, 1));
	json_append_member(ge_chip->message, "channel", json_mknumber(channel, 1));
	json_append_member(ge_chip->message, "temperature", json_mknumber(temperature/10, 1));
	json_append_member(ge_chip->message, "humidity", json_mknumber(humidity, 1));
	json_append_member(ge_chip->message, "battery", json_mknumber(battery, 1));
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
					id = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}

		struct settings_t *tmp = settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON) {
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
			snode->temp = 0;
			snode->humi = 0;

			json_find_number(jvalues, "temperature-offset", &snode->temp);
			json_find_number(jvalues, "humidity-offset", &snode->humi);

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
void ge_chipInit(void) {

	protocol_register(&ge_chip);
	protocol_set_id(ge_chip, "ge_chip");
	protocol_device_add(ge_chip, "ge_chip", "ge_chip Weather Stations");
	ge_chip->devtype = WEATHER;
	ge_chip->hwtype = RF433;
	ge_chip->minrawlen = RAW_LENGTH;
	ge_chip->maxrawlen = RAW_LENGTH;
	ge_chip->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	ge_chip->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;
	
	options_add(&ge_chip->options, 'c', "channel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&ge_chip->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&ge_chip->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&ge_chip->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&ge_chip->options, 'b', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");

	// options_add(&ge_chip->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&ge_chip->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&ge_chip->options, 0, "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&ge_chip->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&ge_chip->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&ge_chip->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&ge_chip->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&ge_chip->options, 0, "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	ge_chip->parseCode=&parseCode;
	ge_chip->checkValues=&checkValues;
	ge_chip->validate=&validate;
	ge_chip->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ge_chip";
	module->version = "0.1";
	module->reqversion = "7.0";
	module->reqcommit = "84";
}

void init(void) {
	ge_chipInit();
}
#endif
