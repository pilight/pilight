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
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "threechan.h"

struct threechan_settings_t {
	double id;
	double temp;
	double humi;
	struct threechan_settings_t *next;
} threechan_settings_t;

struct threechan_settings_t *threechan_settings = NULL;

void threechanParseCode(void) {
	int i = 0, x = 0;
	int temperature = 0, id = 0, humidity = 0, battery = 0;
	int humi_offset = 0, temp_offset = 0;

	for(i=1;i<threechan->rawlen-1;i+=2) {
		threechan->binary[x++] = threechan->code[i];
	}

	id = binToDecRev(threechan->binary, 0, 11);
	battery = threechan->binary[12];
	temperature = binToDecRev(threechan->binary, 18, 27);
	humidity = binToDecRev(threechan->binary, 28, 35);

	struct threechan_settings_t *tmp = threechan_settings;
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
	
	threechan->message = json_mkobject();
	json_append_member(threechan->message, "id", json_mknumber(id));
	json_append_member(threechan->message, "temperature", json_mknumber(temperature));
	json_append_member(threechan->message, "humidity", json_mknumber(humidity*10));
	json_append_member(threechan->message, "battery", json_mknumber(battery));
}

int threechanCheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct threechan_settings_t *snode = NULL;
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

		struct threechan_settings_t *tmp = threechan_settings;		
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}
		
		if(!match) {
			if(!(snode = malloc(sizeof(struct threechan_settings_t)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			snode->id = id;

			json_find_number(jvalues, "device-temperature-offset", &snode->temp);
			json_find_number(jvalues, "device-humidity-offset", &snode->humi);

			snode->next = threechan_settings;
			threechan_settings = snode;
		}
	}
	return 0;
}

void threechanGC(void) {
	struct threechan_settings_t *tmp = NULL;		
	while(threechan_settings) {
		tmp = threechan_settings;
		threechan_settings = threechan_settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&threechan_settings);
}

void threechanInit(void) {

	protocol_register(&threechan);
	protocol_set_id(threechan, "threechan");
	protocol_device_add(threechan, "threechan", "3 Channel Weather Stations");
	protocol_plslen_add(threechan, 266);
	threechan->devtype = WEATHER;
	threechan->hwtype = RF433;
	threechan->pulse = 15;
	threechan->rawlen = 74;

	options_add(&threechan->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&threechan->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&threechan->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&threechan->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&threechan->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&threechan->options, 0, "device-temperature-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&threechan->options, 0, "device-humidity-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&threechan->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&threechan->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&threechan->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&threechan->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	threechan->parseCode=&threechanParseCode;
	threechan->checkValues=&threechanCheckValues;
	threechan->gc=&threechanGC;
}
