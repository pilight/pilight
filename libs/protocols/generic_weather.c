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

#include "settings.h"
#include "log.h"
#include "protocol.h"
#include "binary.h"
#include "gc.h"
#include "generic_weather.h"

void genWeatherCreateMessage(int id, int temperature, int humidity) {
	generic_weather->message = json_mkobject();
	json_append_member(generic_weather->message, "id", json_mknumber(id));
	if(temperature > -999) {
		json_append_member(generic_weather->message, "temperature", json_mknumber(temperature));
	}
	if(humidity > -999) {
		json_append_member(generic_weather->message, "humidity", json_mknumber(humidity));
	}
}

int genWeatherCreateCode(JsonNode *code) {
	int id = -999;
	int temp = -999;
	int humi = -999;
	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "temperature", &tmp) == 0)
		temp = atoi(tmp);
	if(json_find_string(code, "humidity", &tmp) == 0)
		humi = atoi(tmp);

	if(id == -999 || (temp == -999 && humi == -999)) {
		logprintf(LOG_ERR, "generic_weather: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		genWeatherCreateMessage(id, temp, humi);
	}
	return EXIT_SUCCESS;
}

void genWeatherPrintHelp(void) {
	printf("\t -t --temperature=temperature\tset the temperature\n");
	printf("\t -h --humidity=humidity\t\tset the humidity\n");
	printf("\t -b --battery=battery\t\tset the battery level\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void genWeatherInit(void) {
	
	protocol_register(&generic_weather);
	generic_weather->id = malloc(16);
	strcpy(generic_weather->id, "generic_weather");
	protocol_device_add(generic_weather, "generic_weather", "Generic weather stations");
	generic_weather->type = WEATHER;

	options_add(&generic_weather->options, 'h', "humidity", has_value, config_value, "[0-9]");
	options_add(&generic_weather->options, 't', "temperature", has_value, config_value, "[0-9]");
	options_add(&generic_weather->options, 'b', "battery", has_value, config_value, "[01]");
	options_add(&generic_weather->options, 'i', "id", has_value, config_id, "[0-9]");

	protocol_setting_add_number(generic_weather, "decimals", 2, 0);	
	protocol_setting_add_number(generic_weather, "humidity", 1, 0);
	protocol_setting_add_number(generic_weather, "temperature", 1, 0);
	protocol_setting_add_number(generic_weather, "battery", 0, 0);

	generic_weather->printHelp=&genWeatherPrintHelp;
	generic_weather->createCode=&genWeatherCreateCode;
}
