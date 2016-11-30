/*
	Copyright (C) 2014 CurlyMo, Micha

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
#include "tfa_30_3209_02.h"

#define PULSE_MULTIPLIER	13
#define MIN_PULSE_LENGTH	112
#define MAX_PULSE_LENGTH	124
#define AVG_PULSE_LENGTH	111
#define RAW_LENGTH				74

typedef struct settings_t {
	double id;
	double channel;
	double temp;
	double humi;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(tfa_30_3209_02->rawlen == RAW_LENGTH) {
		if(tfa_30_3209_02->raw[tfa_30_3209_02->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   tfa_30_3209_02->raw[tfa_30_3209_02->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(void) {
	int i = 0, x = 0, binary[RAW_LENGTH/2];
	int id = 0;
	int channel = 0;
	double temperature = 0.0, humidity = 0.0;
	double humi_offset = 0.0, temp_offset = 0.0;

	for(x=1;x<tfa_30_3209_02->rawlen-2;x+=2) {
		if(tfa_30_3209_02->raw[x] > AVG_PULSE_LENGTH*PULSE_MULTIPLIER) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	id = binToDecRev(binary, 0, 9);
	channel = binToDecRev(binary, 10, 11) + 1;
	if (binToDecRev(binary, 12, 12) == 0) {
		temperature = (double)(binToDecRev(binary, 12, 27)-15)/160;	
	}
	else {
		temperature = (double)(binToDecRev(binary, 12, 27)-65536-15)/160;
	}
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

	tfa_30_3209_02->message = json_mkobject();
	json_append_member(tfa_30_3209_02->message, "id", json_mknumber(id, 0));
	json_append_member(tfa_30_3209_02->message, "channel", json_mknumber(channel, 0));
	json_append_member(tfa_30_3209_02->message, "temperature", json_mknumber(temperature, 1));
	json_append_member(tfa_30_3209_02->message, "humidity", json_mknumber(humidity, 0));
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
void tfa_30_3209_02Init(void) {

	protocol_register(&tfa_30_3209_02);
	protocol_set_id(tfa_30_3209_02, "tfa_30_3209_02");
	protocol_device_add(tfa_30_3209_02, "tfa_30_3209_02", "TFA weather stations");
	protocol_device_add(tfa_30_3209_02, "tfa_30_3209_02", "TFA Thermo-/Hygrosensor 30.3209.02 ");
	tfa_30_3209_02->devtype = WEATHER;
	tfa_30_3209_02->hwtype = RF433;
	tfa_30_3209_02->minrawlen = RAW_LENGTH;
	tfa_30_3209_02->maxrawlen = RAW_LENGTH;
	tfa_30_3209_02->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	tfa_30_3209_02->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&tfa_30_3209_02->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&tfa_30_3209_02->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&tfa_30_3209_02->options, 'c', "channel", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&tfa_30_3209_02->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");

	options_add(&tfa_30_3209_02->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&tfa_30_3209_02->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&tfa_30_3209_02->options, 0, "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&tfa_30_3209_02->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&tfa_30_3209_02->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&tfa_30_3209_02->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&tfa_30_3209_02->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	tfa_30_3209_02->parseCode=&parseCode;
	tfa_30_3209_02->checkValues=&checkValues;
	tfa_30_3209_02->validate=&validate;
	tfa_30_3209_02->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "tfa_30_3209_02";
	module->version = "0.1";
	module->reqversion = "7.0";
	module->reqcommit = "xx";
}

void init(void) {
	tfa_30_3209_02Init();
}
#endif
