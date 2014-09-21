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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "teknihall.h"

typedef struct teknihall_settings_t {
	double id;
	double temp;
	double humi;
	struct teknihall_settings_t *next;
} teknihall_settings_t;

static struct teknihall_settings_t *teknihall_settings = NULL;

static void teknihallParseCode(void) {
	int i = 0, x = 0;
	int temperature = 0, id = 0, humidity = 0, battery = 0;
	int humi_offset = 0, temp_offset = 0;

	for(i=1;i<teknihall->rawlen-1;i+=2) {
		teknihall->binary[x++] = teknihall->code[i];
	}

	id = binToDecRev(teknihall->binary, 0, 7);
	battery = teknihall->binary[8];
	temperature = binToDecRev(teknihall->binary, 14, 23);
	humidity = binToDecRev(teknihall->binary, 24, 30);

	struct teknihall_settings_t *tmp = teknihall_settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON) {
			humi_offset = (int)tmp->humi;
			temp_offset = (int)tmp->temp;
			break;
		}
		tmp = tmp->next;
	}

	temperature += temp_offset;
	humidity += humi_offset;

	teknihall->message = json_mkobject();
	json_append_member(teknihall->message, "id", json_mknumber(id));
	json_append_member(teknihall->message, "temperature", json_mknumber(temperature));
	json_append_member(teknihall->message, "humidity", json_mknumber(humidity*10));
	json_append_member(teknihall->message, "battery", json_mknumber(battery));
}

static int teknihallCheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct teknihall_settings_t *snode = NULL;
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

		struct teknihall_settings_t *tmp = teknihall_settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(!match) {
			if(!(snode = malloc(sizeof(struct teknihall_settings_t)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			snode->id = id;

			json_find_number(jvalues, "device-temperature-offset", &snode->temp);
			json_find_number(jvalues, "device-humidity-offset", &snode->humi);

			snode->next = teknihall_settings;
			teknihall_settings = snode;
		}
	}
	return 0;
}

static void teknihallGC(void) {
	struct teknihall_settings_t *tmp = NULL;
	while(teknihall_settings) {
		tmp = teknihall_settings;
		teknihall_settings = teknihall_settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&teknihall_settings);
}

#ifndef MODULE
__attribute__((weak))
#endif
void teknihallInit(void) {

	protocol_register(&teknihall);
	protocol_set_id(teknihall, "teknihall");
	protocol_device_add(teknihall, "teknihall", "Teknihall Weather Stations");
	protocol_plslen_add(teknihall, 266);
	teknihall->devtype = WEATHER;
	teknihall->hwtype = RF433;
	teknihall->pulse = 15;
	teknihall->rawlen = 76;

	options_add(&teknihall->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&teknihall->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&teknihall->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&teknihall->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&teknihall->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&teknihall->options, 0, "device-temperature-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&teknihall->options, 0, "device-humidity-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&teknihall->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&teknihall->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&teknihall->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&teknihall->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	teknihall->parseCode=&teknihallParseCode;
	teknihall->checkValues=&teknihallCheckValues;
	teknihall->gc=&teknihallGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "teknihall";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	teknihallInit();
}
#endif
