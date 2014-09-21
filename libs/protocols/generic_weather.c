/*
	Copyright (C) 2013 CurlyMo

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
#include "generic_weather.h"

static void genWeatherCreateMessage(int id, int temperature, int humidity, int battery) {
	generic_weather->message = json_mkobject();
	json_append_member(generic_weather->message, "id", json_mknumber(id));
	if(temperature > -999) {
		json_append_member(generic_weather->message, "temperature", json_mknumber(temperature));
	}
	if(humidity > -999) {
		json_append_member(generic_weather->message, "humidity", json_mknumber(humidity));
	}
	if(battery > -1) {
		json_append_member(generic_weather->message, "battery", json_mknumber(battery));
	}
}

static int genWeatherCreateCode(JsonNode *code) {
	double itmp = 0;
	int id = -999;
	int temp = -999;
	int humi = -999;
	int batt = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "temperature", &itmp) == 0)
		temp = (int)round(itmp);
	if(json_find_number(code, "humidity", &itmp) == 0)
		humi = (int)round(itmp);
	if(json_find_number(code, "battery", &itmp) == 0)
		batt = (int)round(itmp);

	if(id == -999 && temp == -999 && humi == -999 && batt == -1) {
		logprintf(LOG_ERR, "generic_weather: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		genWeatherCreateMessage(id, temp, humi, batt);
	}
	return EXIT_SUCCESS;
}

static void genWeatherPrintHelp(void) {
	printf("\t -t --temperature=temperature\tset the temperature\n");
	printf("\t -h --humidity=humidity\t\tset the humidity\n");
	printf("\t -b --battery=battery\t\tset the battery level\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void genWeatherInit(void) {

	protocol_register(&generic_weather);
	protocol_set_id(generic_weather, "generic_weather");
	protocol_device_add(generic_weather, "generic_weather", "Generic Weather Stations");
	generic_weather->devtype = WEATHER;

	options_add(&generic_weather->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&generic_weather->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&generic_weather->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");
	options_add(&generic_weather->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");

	options_add(&generic_weather->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&generic_weather->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&generic_weather->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&generic_weather->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&generic_weather->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	generic_weather->printHelp=&genWeatherPrintHelp;
	generic_weather->createCode=&genWeatherCreateCode;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "generic_weather";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	genWeatherInit();
}
#endif
