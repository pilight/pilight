/*
	Copyright (C) 2019 CurlyMo & kithack

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
#include "xiron.h"

#define PULSE_MULTIPLIER	20
#define MIN_PULSE_LENGTH	94
#define MAX_PULSE_LENGTH	141
#define AVG_PULSE_LENGTH	151
#define RAW_LENGTH		74

typedef struct settings_t {
	int id;
	int channel;
	double temp;
	double humi;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(xiron->rawlen == RAW_LENGTH) {
		if(xiron->raw[xiron->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   xiron->raw[xiron->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(char **message) {
	int i = 0, x = 0, binary[RAW_LENGTH/2];
	int id = 0, battery = 0, channel = 0;
	double humi_offset = 0.0, temp_offset = 0.0;
	double temperature = 0.0, humidity = 0.0;

	if(xiron->rawlen > RAW_LENGTH) {
		logprintf(LOG_ERR, "xiron: parsecode - invalid parameter passed %d", xiron->rawlen);
		return;
	}

	for(x=1;x<xiron->rawlen-1;x+=2) {
		if(xiron->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	id = binToDecRev(binary, 0, 7);
	battery = binary[8];
	channel = binToDecRev(binary, 10, 11);
	temperature = (double)binToSignedRev(binary, 12, 23);
	humidity = (double)binToDecRev(binary, 28, 35);

	temperature /= 10;

	struct settings_t *tmp = settings;
	while(tmp) {
		if(tmp->id == id && tmp->channel == channel) {
			humi_offset = tmp->humi;
			temp_offset = tmp->temp;
			break;
		}
		tmp = tmp->next;
	}

	temperature += temp_offset;
	humidity += humi_offset;


	snprintf((*message), 255,
		"{\"id\":%d,\"temperature\":%.2f,\"humidity\":%.2f,\"channel\":%d,\"battery\":%d}",
		id, temperature, humidity, channel, battery);
}

static int checkValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id")) != NULL) {
		struct settings_t *snode = NULL;
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;
		int channel = -1, id = -1;
		int match = 0;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "channel") == 0) {
					channel = (int)jchild1->number_;
				}
				if(strcmp(jchild1->key, "id") == 0) {
					id = (int)jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}

		struct settings_t *tmp = settings;
		while(tmp) {
			if(tmp->id == id && tmp->channel == channel) {
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
			snode->channel = channel;
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
void xironInit(void) {

	protocol_register(&xiron);
	protocol_set_id(xiron, "xiron");
	protocol_device_add(xiron, "xiron", "Xiron Weather Stations");
	xiron->devtype = WEATHER;
	xiron->hwtype = RF433;
	xiron->minrawlen = RAW_LENGTH;
	xiron->maxrawlen = RAW_LENGTH;
	xiron->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	xiron->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&xiron->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&xiron->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&xiron->options, "c", "channel", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&xiron->options, "h", "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&xiron->options, "b", "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");

	// options_add(&xiron->options, "0", "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&xiron->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&xiron->options, "0", "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&xiron->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&xiron->options, "0", "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&xiron->options, "0", "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&xiron->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&xiron->options, "0", "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	xiron->parseCode=&parseCode;
	xiron->checkValues=&checkValues;
	xiron->validate=&validate;
	xiron->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "xiron";
	module->version = "2.5";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	xironInit();
}
#endif
