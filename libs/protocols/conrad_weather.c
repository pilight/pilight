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

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "conrad_weather.h"

void conradWeatherParseCode(void) {
	int temp1 = 0, temp2 = 0, temp3 = 0;
	int humi1 = 0, humi2 = 0;
	int temperature = 0, id = 0;
	int humidity = 0, battery = 0;
	int channel = 0;
	int i = 0, x = 0;

	for(i=1;i<conrad_weather->rawlen-2;i+=2) {
		conrad_weather->binary[x++] = conrad_weather->code[i];
	}

	id = binToDecRev(conrad_weather->binary, 2, 9);
	channel = binToDecRev(conrad_weather->binary, 12, 13) + 1;

	temp1 = binToDecRev(conrad_weather->binary, 14, 17);
	temp2 = binToDecRev(conrad_weather->binary, 18, 21);
	temp3 = binToDecRev(conrad_weather->binary, 22, 25);
	                                                     /* Convert F to C */
	temperature = (int)((float)(((((temp3*256) + (temp2*16) + (temp1))*10) - 9000) - 3200) * ((float)5/(float)9));

	humi1 = binToDecRev(conrad_weather->binary, 26, 29);
	humi2 = binToDecRev(conrad_weather->binary, 30, 33);
	humidity = (humi1)+(humi2*16);

	if(binToDecRev(conrad_weather->code, 34, 35) == 1) {
		battery = 0;
	} else {
		battery = 1;
	}

	conrad_weather->message = json_mkobject();
	json_append_member(conrad_weather->message, "id", json_mknumber(id));
	json_append_member(conrad_weather->message, "temperature", json_mknumber(temperature));
	json_append_member(conrad_weather->message, "humidity", json_mknumber(humidity));
	json_append_member(conrad_weather->message, "battery", json_mknumber(battery));
	json_append_member(conrad_weather->message, "channel", json_mknumber(channel));
}

void conradWeatherInit(void) {
	protocol_register(&conrad_weather);
	protocol_set_id(conrad_weather, "conrad_weather");
	protocol_device_add(conrad_weather, "conrad_weather", "Conrad Weather Stations");
	protocol_plslen_add(conrad_weather, 225);
	protocol_plslen_add(conrad_weather, 235);
	conrad_weather->devtype = WEATHER;
	conrad_weather->hwtype = RF433;
	conrad_weather->pulse = 20;
	conrad_weather->rawlen = 86;

	options_add(&conrad_weather->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&conrad_weather->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&conrad_weather->options, 'c', "channel", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&conrad_weather->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&conrad_weather->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&conrad_weather->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&conrad_weather->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&conrad_weather->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&conrad_weather->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&conrad_weather->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	conrad_weather->parseCode=&conradWeatherParseCode;
}