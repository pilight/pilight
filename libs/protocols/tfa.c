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
#include "tfa.h"

void tfaParseCode(void) {
	int temp1 = 0, temp2 = 0, temp3 = 0;
	int humi1 = 0, humi2 = 0;
	int temperature = 0, id = 0;
	int humidity = 0, battery = 0;
	int channel = 0;
	int i = 0, x = 0;

	for(i=1;i<tfa->rawlen-2;i+=2) {
		tfa->binary[x++] = tfa->code[i];
	}

	id = binToDecRev(tfa->binary, 2, 9);
	channel = binToDecRev(tfa->binary, 12, 13) + 1;

	temp1 = binToDecRev(tfa->binary, 14, 17);
	temp2 = binToDecRev(tfa->binary, 18, 21);
	temp3 = binToDecRev(tfa->binary, 22, 25);
	                                                     /* Convert F to C */
	temperature = (int)((float)(((((temp3*256) + (temp2*16) + (temp1))*10) - 9000) - 3200) * ((float)5/(float)9));

	humi1 = binToDecRev(tfa->binary, 26, 29);
	humi2 = binToDecRev(tfa->binary, 30, 33);
	humidity = ((humi1)+(humi2*16))*100;

	if(binToDecRev(tfa->code, 34, 35) == 1) {
		battery = 0;
	} else {
		battery = 1;
	}

	tfa->message = json_mkobject();
	json_append_member(tfa->message, "id", json_mknumber(id));
	json_append_member(tfa->message, "temperature", json_mknumber(temperature));
	json_append_member(tfa->message, "humidity", json_mknumber(humidity));
	json_append_member(tfa->message, "battery", json_mknumber(battery));
	json_append_member(tfa->message, "channel", json_mknumber(channel));
}

void tfaInit(void) {
	protocol_register(&tfa);
	protocol_set_id(tfa, "tfa");
	protocol_device_add(tfa, "tfa", "TFA weather stations");
	protocol_device_add(tfa, "conrad_weather", "Conrad Weather Stations");
	protocol_plslen_add(tfa, 220);
	protocol_plslen_add(tfa, 230);
	protocol_plslen_add(tfa, 240);
	protocol_plslen_add(tfa, 250);
	tfa->devtype = WEATHER;
	tfa->hwtype = RF433;
	tfa->pulse = 20;
	tfa->rawlen = 86;

	options_add(&tfa->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&tfa->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&tfa->options, 'c', "channel", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&tfa->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&tfa->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&tfa->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&tfa->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&tfa->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&tfa->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&tfa->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	tfa->parseCode=&tfaParseCode;
}
