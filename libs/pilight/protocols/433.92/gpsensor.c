/*
	Copyright (C) 2014 CurlyMo & TheWheel 

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
#include "gpsensor.h"
//
// Protocol characteristics: SYNC bit: 250/4500, Logical 0: 250/900, Logical 1: 250/1800
//
#define PULSE_MULTIPLIER	12
#define MIN_PULSE_LENGTH	120
#define AVG_PULSE_LENGTH	250
#define MAX_PULSE_LENGTH	150
#define RAW_LENGTH			146

typedef struct settings_t {
	double id;
	double channel;
	double value;
	double powof10;
	char munit[5];
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(gpsensor->rawlen == RAW_LENGTH) {
		if(gpsensor->raw[gpsensor->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   gpsensor->raw[gpsensor->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int id, int channel, int battery, double sensorvalue, char *munit) {
	gpsensor->message = json_mkobject();
	json_append_member(gpsensor->message, "id", json_mknumber(id, 0));
	json_append_member(gpsensor->message, "channel", json_mknumber(channel, 0));
	json_append_member(gpsensor->message, "battery", json_mknumber(battery, 0));
	json_append_member(gpsensor->message, "sensorvalue", json_mknumber(sensorvalue, 2));
	json_append_member(gpsensor->message, "munit", json_mkstring(munit));
}

static void parseCode(void) {
	int i = 0, x = 0, binary[RAW_LENGTH/2];
	int header = 0, id = 0, channel = 0, powof10 = 0, battery = 0, parity = 0;
	double value_offset = 0.0;
	double sensorvalue = 0.0;
	char munit[5];

	if(gpsensor->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "gpsensor: parsecode - invalid parameter passed %d", gpsensor->rawlen);
		return;
	}

	for(x=1;x<gpsensor->rawlen-1;x+=2) {
		if(gpsensor->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
			parity++;
		} else {
			binary[i++] = 0;
		}
	}

	if ((parity % 2) != 0) {
		logprintf(LOG_ERR, "gpsensor: parsecode - parity error");
		return;
	}
	header = binToDecRev(binary, 0, 3);
	if (header != 12) {
		logprintf(LOG_ERR, "gpsensor: parsecode - header error (is %d, should be 12)", header);
		return;
	}
	id = binToDecRev(binary, 4, 11);
	channel = binToDecRev(binary, 12, 14);
	battery = binary[15];
	sensorvalue = (double)binToDecRev(binary, 16, 35);
	powof10 = (double)binToDecRev(binary, 36, 38);
	munit[0] = (char)binToDecRev(binary, 39, 46);
	munit[1] = (char)binToDecRev(binary, 47, 54);
	munit[2] = (char)binToDecRev(binary, 55, 62);
	munit[3] = (char)binToDecRev(binary, 63, 70);
	munit[4] = '\0';

	if (binary[16]) {
		sensorvalue -= 1048576;
	}
	sensorvalue /= pow(10, powof10);
	//logprintf(LOG_ERR, "gpsensor: parseCode - sensorID %d, Channel %d, Value %f %s",
	//		                      id, channel, sensorvalue, munit);

	struct settings_t *tmp = settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON) {
			value_offset = tmp->value;
			break;
		}
		tmp = tmp->next;
	}

	sensorvalue += value_offset;

	createMessage(id, channel, battery, sensorvalue, munit);
}

static int checkValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct settings_t *snode = NULL;
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;
		double id = -1, channel = -1;
		int match = 0;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "id") == 0) {
					id = jchild1->number_;
				}
				if(strcmp(jchild1->key, "channel") == 0) {
					channel = jchild1->number_;
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
			snode->channel = channel;
			snode->value = 0;
			snode->powof10 = 0;

			json_find_number(jvalues, "sensorvalue-offset", &snode->value);

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
void gpsensorInit(void) {

	protocol_register(&gpsensor);
	protocol_set_id(gpsensor, "gpsensor");
	protocol_device_add(gpsensor, "gpsensor", "General Purpose Sensor");
	gpsensor->devtype = GPSENSOR;
	gpsensor->hwtype = RF433;
	gpsensor->minrawlen = RAW_LENGTH;
	gpsensor->maxrawlen = RAW_LENGTH;
	gpsensor->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	gpsensor->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&gpsensor->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&gpsensor->options, 'c', "channel", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&gpsensor->options, 't', "sensorvalue", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,20}$");
	//options_add(&gpsensor->options, 'd', "powof10", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&gpsensor->options, 'b', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");
	//options_add(&gpsensor->options, 'm', "munit", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, "^[[:alnum:]_]+$");

	options_add(&gpsensor->options, 0, "sensorvalue-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&gpsensor->options, 0, "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&gpsensor->options, 0, "sensorvalue-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");

	gpsensor->parseCode=&parseCode;
	gpsensor->checkValues=&checkValues;
	gpsensor->validate=&validate;
	gpsensor->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "gpsensor";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	gpsensorInit();
}
#endif

