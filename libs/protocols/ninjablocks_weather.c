/*
	Copyright (C) 2014 CurlyMo & wo_rasp

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
#include "ninjablocks_weather.h"

#define	PULSE_NINJA_WEATHER_SHORT	1000
#define PULSE_NINJA_WEATHER_LONG	2000
#define PULSE_NINJA_WEATHER_FOOTER	2120	// 72080/PULSE_DIV
#define PULSE_NINJA_WEATHER_LOWER	750		// SHORT*0,75
#define PULSE_NINJA_WEATHER_UPPER	1250	// SHORT * 1,25

typedef struct ninjablocks_weather_settings_t {
	double id;
	double unit;
	double temp;
	double humi;
	struct ninjablocks_weather_settings_t *next;
} ninjablocks_weather_settings_t;

static struct ninjablocks_weather_settings_t *ninjablocks_weather_settings = NULL;

static void ninjablocksWeatherCreateMessage(int id, int unit, int temperature, int humidity) {
	ninjablocks_weather->message = json_mkobject();
	json_append_member(ninjablocks_weather->message, "id", json_mknumber(id));
	json_append_member(ninjablocks_weather->message, "unit", json_mknumber(unit));
	json_append_member(ninjablocks_weather->message, "temperature", json_mknumber(temperature));
	json_append_member(ninjablocks_weather->message, "humidity", json_mknumber(humidity*100));
}

static void ninjablocksWeatherParseCode(void) {
	int x = 0, pRaw = 0;
	int iParity = 1, iParityData = -1;	// init for even parity
	int iHeaderSync = 12;				// 1100
	int iDataSync = 6;					// 110
	int temp_offset = 0;
	int humi_offset = 0;

	// Decode Biphase Mark Coded Differential Manchester (BMCDM) pulse stream into binary
	for(x=0; x<=ninjablocks_weather->binlen; x++) {
		if(ninjablocks_weather->raw[pRaw] > PULSE_NINJA_WEATHER_LOWER && ninjablocks_weather->raw[pRaw] < PULSE_NINJA_WEATHER_UPPER) {
			ninjablocks_weather->binary[x] = 1;
			iParityData=iParity;
			iParity=-iParity;
			pRaw++;
		} else {
			ninjablocks_weather->binary[x] = 0;
		}
		pRaw++;
	}
	if(iParityData<0)
		iParityData=0;

	// Binary record: 0-3 sync0, 4-7 unit, 8-9 id, 10-12 sync1, 13-19 humidity, 20-34 temperature, 35 even par, 36 footer
	int headerSync = binToDecRev(ninjablocks_weather->binary, 0,3);
	int unit = binToDecRev(ninjablocks_weather->binary, 4,7);
	int id = binToDecRev(ninjablocks_weather->binary, 8,9);
	int dataSync = binToDecRev(ninjablocks_weather->binary, 10,12);
	int humidity = binToDecRev(ninjablocks_weather->binary, 13,19);	// %
	int temperature = binToDecRev(ninjablocks_weather->binary, 20,34);
	// ((temp * (100 / 128)) - 5000) * 10 °C, 2 digits
	temperature = ((int)((double)(temperature * 0.78125)) - 5000);

	struct ninjablocks_weather_settings_t *tmp = ninjablocks_weather_settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON && fabs(tmp->unit-unit) < EPSILON) {
			humi_offset = (int)tmp->humi;
			temp_offset = (int)tmp->temp;
			break;
		}
		tmp = tmp->next;
	}

	temperature += temp_offset;
	humidity += humi_offset;

	if(iParityData == 0 && (iHeaderSync == headerSync || dataSync == iDataSync)) {
		ninjablocksWeatherCreateMessage(id, unit, temperature, humidity);
	}
}

static int ninjablocksWeatherCheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct ninjablocks_weather_settings_t *snode = NULL;
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;
		double unit = -1, id = -1;
		int match = 0;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "unit") == 0) {
					unit = jchild1->number_;
				}
				if(strcmp(jchild1->key, "id") == 0) {
					id = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}

		struct ninjablocks_weather_settings_t *tmp = ninjablocks_weather_settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON && fabs(tmp->unit-unit) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(!match) {
			if(!(snode = malloc(sizeof(struct ninjablocks_weather_settings_t)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			snode->id = id;
			snode->unit = unit;

			json_find_number(jvalues, "device-temperature-offset", &snode->temp);
			json_find_number(jvalues, "device-humidity-offset", &snode->humi);

			snode->next = ninjablocks_weather_settings;
			ninjablocks_weather_settings = snode;
		}
	}
	return 0;
}

static void ninjablocksWeatherGC(void) {
	struct ninjablocks_weather_settings_t *tmp = NULL;
	while(ninjablocks_weather_settings) {
		tmp = ninjablocks_weather_settings;
		ninjablocks_weather_settings = ninjablocks_weather_settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&ninjablocks_weather_settings);
}

#ifndef MODULE
__attribute__((weak))
#endif
void ninjablocksWeatherInit(void) {

	protocol_register(&ninjablocks_weather);
	protocol_set_id(ninjablocks_weather, "ninjablocks_weather");
	protocol_device_add(ninjablocks_weather, "ninjablocks_weather", "Ninjablocks Weather Sensors");
	// Footer length ratio: (72080/PULSE_DIV)/2120=2,120
	protocol_plslen_add(ninjablocks_weather, PULSE_NINJA_WEATHER_FOOTER);
	ninjablocks_weather->devtype = SENSOR;
	ninjablocks_weather->hwtype = RF433;
	// LONG=ninjablocks_PULSE_HIGH*SHORT
	ninjablocks_weather->pulse = 2;
	// dynamically between 41..70 footer is depending on raw pulse code
	ninjablocks_weather->rawlen = 70;
	ninjablocks_weather->minrawlen = 41;
	ninjablocks_weather->maxrawlen = 70;
	// sync-id[4]; Homecode[4], Channel Code[2], Sync[3], Humidity[7], Temperature[15], Footer [1]
	ninjablocks_weather->binlen = 35;

	options_add(&ninjablocks_weather->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]|1[0-5])$");
	options_add(&ninjablocks_weather->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&ninjablocks_weather->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&ninjablocks_weather->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");

	options_add(&ninjablocks_weather->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&ninjablocks_weather->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&ninjablocks_weather->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&ninjablocks_weather->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&ninjablocks_weather->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	ninjablocks_weather->parseCode=&ninjablocksWeatherParseCode;
	ninjablocks_weather->checkValues=&ninjablocksWeatherCheckValues;
	ninjablocks_weather->gc=&ninjablocksWeatherGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name =  "ninjablocks_weather";
	module->version =  "0.9";
	module->reqversion =  "5.0";
	module->reqcommit =  NULL;
}

void init(void) {
	ninjablocksWeatherInit();
}
#endif

