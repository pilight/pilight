/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "teknihall.h"

#define PULSE_MULTIPLIER	20
#define MIN_PULSE_LENGTH	261
#define MAX_PULSE_LENGTH	271
#define AVG_PULSE_LENGTH	266
#define RAW_LENGTH				76

typedef struct settings_t {
	double id;
	double temp;
	double humi;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(teknihall->rawlen == RAW_LENGTH) {
		if(teknihall->raw[teknihall->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   teknihall->raw[teknihall->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(char *message) {
	int i = 0, x = 0, binary[RAW_LENGTH/2];
	int id = 0, battery = 0;
	double temperature = 0.0, humidity = 0.0;
	double humi_offset = 0.0, temp_offset = 0.0;

	if(teknihall->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "teknihall: parsecode - invalid parameter passed %d", teknihall->rawlen);
		return;
	}

	for(x=1;x<teknihall->rawlen-1;x+=2) {
		if(teknihall->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	id = binToDecRev(binary, 0, 7);
	battery = binary[8];
	temperature = binToDecRev(binary, 14, 23);
	humidity = binToDecRev(binary, 24, 30);

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

	snprintf(message, 255,
		"{\"id\":%d,\"temperature\":%.1f,\"humidity\":%.1f,\"battery\":%d}",
		id, temperature/10, humidity, battery
	);
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
				OUT_OF_MEMORY
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
void teknihallInit(void) {

	protocol_register(&teknihall);
	protocol_set_id(teknihall, "teknihall");
	protocol_device_add(teknihall, "teknihall", "Teknihall Weather Stations");
	teknihall->devtype = WEATHER;
	teknihall->hwtype = RF433;
	teknihall->minrawlen = RAW_LENGTH;
	teknihall->maxrawlen = RAW_LENGTH;
	teknihall->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	teknihall->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&teknihall->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&teknihall->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&teknihall->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&teknihall->options, 'b', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");

	// options_add(&teknihall->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&teknihall->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&teknihall->options, 0, "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&teknihall->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&teknihall->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&teknihall->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&teknihall->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&teknihall->options, 0, "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	teknihall->parseCode=&parseCode;
	teknihall->checkValues=&checkValues;
	teknihall->validate=&validate;
	teknihall->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "teknihall";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	teknihallInit();
}
#endif
