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
#include "alecto_ws1700.h"

typedef struct alecto_ws1700_settings_t {
	double id;
	double temp;
	double humi;
	struct alecto_ws1700_settings_t *next;
} alecto_ws1700_settings_t;

static struct alecto_ws1700_settings_t *alecto_ws1700_settings = NULL;

static void alectoWS1700ParseCode(void) {
	int i = 0, x = 0;
	int temperature = 0, id = 0, humidity = 0, battery = 0;
	int humi_offset = 0, temp_offset = 0;

	for(i=1;i<alecto_ws1700->rawlen-1;i+=2) {
		alecto_ws1700->binary[x++] = alecto_ws1700->code[i];
	}

	id = binToDecRev(alecto_ws1700->binary, 0, 11);
	battery = alecto_ws1700->binary[12];
	temperature = binToDecRev(alecto_ws1700->binary, 18, 27);
	humidity = binToDecRev(alecto_ws1700->binary, 28, 35);

	struct alecto_ws1700_settings_t *tmp = alecto_ws1700_settings;
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

	alecto_ws1700->message = json_mkobject();
	json_append_member(alecto_ws1700->message, "id", json_mknumber(id));
	json_append_member(alecto_ws1700->message, "temperature", json_mknumber(temperature));
	json_append_member(alecto_ws1700->message, "humidity", json_mknumber(humidity*10));
	json_append_member(alecto_ws1700->message, "battery", json_mknumber(battery));
}

static int alectoWS1700CheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct alecto_ws1700_settings_t *snode = NULL;
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

		struct alecto_ws1700_settings_t *tmp = alecto_ws1700_settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(!match) {
			if(!(snode = malloc(sizeof(struct alecto_ws1700_settings_t)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			snode->id = id;

			json_find_number(jvalues, "device-temperature-offset", &snode->temp);
			json_find_number(jvalues, "device-humidity-offset", &snode->humi);

			snode->next = alecto_ws1700_settings;
			alecto_ws1700_settings = snode;
		}
	}
	return 0;
}

static void alectoWS1700GC(void) {
	struct alecto_ws1700_settings_t *tmp = NULL;
	while(alecto_ws1700_settings) {
		tmp = alecto_ws1700_settings;
		alecto_ws1700_settings = alecto_ws1700_settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&alecto_ws1700_settings);
}

#ifndef MODULE
__attribute__((weak))
#endif
void alectoWS1700Init(void) {

	protocol_register(&alecto_ws1700);
	protocol_set_id(alecto_ws1700, "alecto_ws1700");
	protocol_device_add(alecto_ws1700, "alecto_ws1700", "Alecto WS1700 Stations");
	protocol_plslen_add(alecto_ws1700, 266);
	alecto_ws1700->devtype = WEATHER;
	alecto_ws1700->hwtype = RF433;
	alecto_ws1700->pulse = 15;
	alecto_ws1700->rawlen = 74;

	options_add(&alecto_ws1700->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&alecto_ws1700->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&alecto_ws1700->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&alecto_ws1700->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&alecto_ws1700->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&alecto_ws1700->options, 0, "device-temperature-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&alecto_ws1700->options, 0, "device-humidity-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&alecto_ws1700->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&alecto_ws1700->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&alecto_ws1700->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&alecto_ws1700->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	alecto_ws1700->parseCode=&alectoWS1700ParseCode;
	alecto_ws1700->checkValues=&alectoWS1700CheckValues;
	alecto_ws1700->gc=&alectoWS1700GC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "alecto_ws1700";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	alectoWS1700Init();
}
#endif
