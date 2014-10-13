/*
	Copyright (C) 2014 CurlyMo & Tommybear1979

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
#include "alecto_wx500.h"

typedef struct alecto_settings_t {
	double id;
	double temp;
	double humi;
	struct alecto_settings_t *next;
} alecto_settings_t;

static struct alecto_settings_t *alecto_settings = NULL;

static void alectoWX500ParseCode(void) {
	int i = 0, x = 0, type = 0;
	int temperature = 0, id = 0;
	int temp_offset = 0, humi_offset = 0;
	int battery = 0;
	int winddir = 0, windavg = 0, windgust = 0;
	int rain = 0, humidity = 0;
	int n0 = 0, n1 = 0, n2 = 0, n3 = 0;
	int n4 = 0, n5 = 0, n6 = 0, n7 = 0, n8 = 0;
	int checksum = 1;

	for(i=1;i<alectoWX500->rawlen-1;i+=2) {
		alectoWX500->binary[x++] = alectoWX500->code[i];
	};

	n8=binToDec(alectoWX500->binary, 32, 36);
	n7=binToDec(alectoWX500->binary, 28, 31);
	n6=binToDec(alectoWX500->binary, 24, 27);
	n5=binToDec(alectoWX500->binary, 20, 23);
	n4=binToDec(alectoWX500->binary, 16, 19);
	n3=binToDec(alectoWX500->binary, 12, 15);
	n2=binToDec(alectoWX500->binary, 8, 11);
	n1=binToDec(alectoWX500->binary, 4, 7);
	n0=binToDec(alectoWX500->binary, 0, 3);

	struct alecto_settings_t *tmp = alecto_settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON){
			humi_offset = (int)tmp->humi;
			temp_offset = (int)tmp->temp;
			break;
		}
		tmp = tmp->next;
	}

	if((n2 & 0x6) != 0x6) {
		type = 0x1;
		checksum = (0xf-n0-n1-n2-n3-n4-n5-n6-n7) & 0xf;
		if(n8 != checksum) {
			type=0x5;
			return;
		}
	//Wind average * 0.2
	} else if(n3 == 0x1) {
		type = 0x2;
		checksum = (0xf-n0-n1-n2-n3-n4-n5-n6-n7) & 0xf;
		if(n8 != checksum){
			type=0x5;
			return;
		}
	//Wind direction & gust
	} else if((n3 & 0x7) == 0x7) {
		type = 0x3;
		checksum = (0xf-n0-n1-n2-n3-n4-n5-n6-n7) & 0xf;
		if(n8 != checksum) {
			type=0x5;
			return;
		}
	//Rain
	} else if(n3 == 0x3)	{
		type = 0x4;
		checksum = (0x7+n0+n1+n2+n3+n4+n5+n6+n7) & 0xf;
		if(n8 != checksum){
			type = 0x5;
			return;
		}
	//Catch
	} else 	{
		type = 0x5;
		return;
	}

	alectoWX500->message = json_mkobject();
	switch(type) {
		case 1:
			id = binToDec(alectoWX500->binary, 0, 7);
			temperature = binToDec(alectoWX500->binary, 12, 22);
			humidity = (binToDec(alectoWX500->binary, 28, 31) * 100) + (binToDec(alectoWX500->binary, 24,27 ) * 10);
			battery = !alectoWX500->binary[8];

			temperature += temp_offset;
			humidity += humi_offset;

			json_append_member(alectoWX500->message, "temperature", json_mknumber(temperature));
			json_append_member(alectoWX500->message, "humidity", json_mknumber(humidity));
			json_append_member(alectoWX500->message, "battery", json_mknumber(battery));
		break;
		case 2:
			id = binToDec(alectoWX500->binary, 0, 7);
			windavg = binToDec(alectoWX500->binary, 24, 31) * 2;
			battery = !alectoWX500->binary[8];

			json_append_member(alectoWX500->message, "windavg", json_mknumber(windavg));
			json_append_member(alectoWX500->message, "battery", json_mknumber(battery));
		break;
		case 3:
			id = binToDec(alectoWX500->binary, 0, 7);
			winddir = binToDec(alectoWX500->binary, 15, 23);
			windgust = binToDec(alectoWX500->binary, 24, 31) * 2;
			battery = !alectoWX500->binary[8];

			json_append_member(alectoWX500->message, "winddir", json_mknumber(winddir));
			json_append_member(alectoWX500->message, "windgust", json_mknumber(windgust));
			json_append_member(alectoWX500->message, "battery", json_mknumber(battery));
		break;
		case 4:
			id = binToDec(alectoWX500->binary, 0, 7);
			rain = binToDec(alectoWX500->binary, 16, 30) * 5;
			battery = !alectoWX500->binary[8];
			json_append_member(alectoWX500->message, "rain", json_mknumber(rain));
			json_append_member(alectoWX500->message, "battery", json_mknumber(battery));
		break;
		default:
			type=0x5;
			json_delete(alectoWX500->message);
			alectoWX500->message = NULL;
			return;
		break;
	}
}

static int alectoWX500CheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct alecto_settings_t *snode = NULL;
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

		struct alecto_settings_t *tmp = alecto_settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(!match) {
			if(!(snode = malloc(sizeof(struct alecto_settings_t)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			snode->id = id;

			json_find_number(jvalues, "device-temperature-offset", &snode->temp);
			json_find_number(jvalues, "device-humidity-offset", &snode->humi);

			snode->next = alecto_settings;
			alecto_settings = snode;
		}
	}
	return 0;
}

static void alectoWX500GC(void) {
	struct alecto_settings_t *tmp = NULL;
	while(alecto_settings) {
		tmp = alecto_settings;
		alecto_settings = alecto_settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&alecto_settings);
}

#ifndef MODULE
__attribute__((weak))
#endif
void alectoWX500Init(void) {

	protocol_register(&alectoWX500);
	protocol_set_id(alectoWX500, "alecto_wx500");
	protocol_device_add(alectoWX500, "alecto_wx500", "Alecto WX500 Weather Stations");
	protocol_device_add(alectoWX500, "auriol_h13726", "Auriol H13726 Weather Stations");
	protocol_device_add(alectoWX500, "ventus_wsxxx", "Ventus WSXXX Weather Stations");
	protocol_device_add(alectoWX500, "hama_ews1500", "Hama EWS1500 Weather Stations");
	protocol_device_add(alectoWX500, "meteoscan_w1XX", "Meteoscan W1XXX Weather Stations");
	protocol_device_add(alectoWX500, "balance_rf_ws105", "Balance RF-WS105 Weather Stations");
	protocol_plslen_add(alectoWX500, 270);
	protocol_plslen_add(alectoWX500, 260);
	protocol_plslen_add(alectoWX500, 250);
	protocol_plslen_add(alectoWX500, 240);
	alectoWX500->devtype = WEATHER;
	alectoWX500->hwtype = RF433;
	alectoWX500->pulse = 16;
	alectoWX500->rawlen = 74;
	alectoWX500->binlen = 36;

	options_add(&alectoWX500->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&alectoWX500->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&alectoWX500->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&alectoWX500->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&alectoWX500->options, 'w', "windavg", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&alectoWX500->options, 'd', "winddir", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&alectoWX500->options, 'g', "windgust", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&alectoWX500->options, 'r', "rain", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");

	options_add(&alectoWX500->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&alectoWX500->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&alectoWX500->options, 0, "device-humidity-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&alectoWX500->options, 0, "device-temperature-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&alectoWX500->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&alectoWX500->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&alectoWX500->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&alectoWX500->options, 0, "gui-show-wind", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&alectoWX500->options, 0, "gui-show-rain", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	alectoWX500->parseCode=&alectoWX500ParseCode;
	alectoWX500->checkValues=&alectoWX500CheckValues;
	alectoWX500->gc=&alectoWX500GC;
}

#ifdef MODULAR
void compatibility(const char **version, const char **commit) {
	module->name = "alecto_wx500";
	module->version = "0.8";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	alectoWX500Init();
}
#endif
