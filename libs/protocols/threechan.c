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
#include "threechan.h"

void threechanParseCode(void) {
	int i = 0, x = 0;
	int temperature;
	int id;
	int humidity;
	int battery;

	for(i=1;i<threechan->rawlen-1;i+=2) {
		threechan->binary[x++] = threechan->code[i];
	}

	id = binToDecRev(threechan->binary, 0, 11);
	battery = threechan->binary[12];
	temperature = binToDecRev(threechan->binary, 18, 27);
	humidity = binToDecRev(threechan->binary, 28, 35);

	threechan->message = json_mkobject();
	json_append_member(threechan->message, "id", json_mknumber(id));
	json_append_member(threechan->message, "temperature", json_mknumber(temperature));
	json_append_member(threechan->message, "humidity", json_mknumber(humidity*10));
	json_append_member(threechan->message, "battery", json_mknumber(battery));
}

void threechanInit(void) {

	protocol_register(&threechan);
	protocol_set_id(threechan, "threechan");
	protocol_device_add(threechan, "threechan", "3 Channel Weather Stations");
	protocol_plslen_add(threechan, 266);
	protocol_conflict_add(threechan, "alecto");
	protocol_conflict_add(threechan, "teknihall");
	threechan->devtype = WEATHER;
	threechan->hwtype = RF433;
	threechan->pulse = 15;
	threechan->rawlen = 74;

	options_add(&threechan->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&threechan->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&threechan->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&threechan->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&threechan->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&threechan->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&threechan->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&threechan->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&threechan->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	threechan->parseCode=&threechanParseCode;
}
