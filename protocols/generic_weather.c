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

void genWeatherCreateMessage(int id, int temperature, int precision_temperature, int humidity, int precision_humidity) {
	generic_weather->message = json_mkobject();
	json_append_member(generic_weather->message, "id", json_mknumber(id));
	json_append_member(generic_weather->message, "temperature", json_mknumber(temperature));
	json_append_member(generic_weather->message, "precision_temperature", json_mknumber(precision_temperature));
	json_append_member(generic_weather->message, "humidity", json_mknumber(humidity));
	json_append_member(generic_weather->message, "precision_humidity", json_mknumber(precision_humidity));

}

int genWeatherCreateCode(JsonNode *code) {
	int id = -1;
	int temp = -1;
	int humi = -1;
	int prec_temp = -1;
	int prec_humi = -1;

	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "temperature", &tmp) == 0)
		temp = atoi(tmp);
	if(json_find_string(code, "humidity", &tmp) == 0)
		humi = atoi(tmp);
	if(json_find_string(code, "precision_temperature", &tmp) == 0)
		prec_temp = atoi(tmp);
	if(json_find_string(code, "precision_humidity", &tmp) == 0)
		prec_humi = atoi(tmp);

	
	if(id == -1 || temp == -1 || humi == -1 || prec_temp == -1 || prec_humi == -1) {
		logprintf(LOG_ERR, "generic_weather: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
	  genWeatherCreateMessage(id, temp, prec_temp, humi, prec_humi);
	}
	return EXIT_SUCCESS;
}

void genWeatherPrintHelp(void) {
	printf("\t -t --temperature=temperature\t\t\tset the temperature\n");
	printf("\t -pt --precision_temperature=precision_temperature\t\t\tset the decimal precision of temperature\n");

	printf("\t -h --humidity=humidity\t\t\tset the humidity\n");
	printf("\t -ph --precision_humidity=precision_humidity\t\t\tset the decimal precision of humidity\n");

	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void genWeatherInit(void) {
	
	protocol_register(&generic_weather);
	generic_weather->id = malloc(16);
	strcpy(generic_weather->id, "generic_weather");
	protocol_add_device(generic_weather, "generic_weather", "Generic weather stations");
	generic_weather->type = WEATHER;

	options_add(&generic_weather->options, 'h', "humidity", has_value, config_value, "[0-9]");
	options_add(&generic_weather->options, 't', "temperature", has_value, config_value, "[0-9]");
	options_add(&generic_weather->options, 'ph', "precision_humidity", has_value, config_value, "[0-9]");
	options_add(&generic_weather->options, 'pt', "precision_temperature", has_value, config_value, "[0-9]");

	options_add(&generic_weather->options, 'i', "id", has_value, config_id, "[0-9]");

	generic_weather->printHelp=&genWeatherPrintHelp;
	generic_weather->createCode=&genWeatherCreateCode;
}
