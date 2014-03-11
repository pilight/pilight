/*
	Copyright (C) 2014 CurlyMo & Meloen

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
#include "teknihall.h"

void teknihallParseCode(void) {
	int i = 0, x = 0;
	int temperature;
	int id;
	int humidity;
	int battery;

	for(i=1;i<teknihall->rawlen-1;i+=2) {
		teknihall->binary[x++] = teknihall->code[i];
	}

	id = binToDecRev(teknihall->binary, 0, 7);
	battery = teknihall->binary[8];
	temperature = binToDecRev(teknihall->binary, 14, 23);
	humidity = binToDecRev(teknihall->binary, 24, 30);

	teknihall->message = json_mkobject();
	json_append_member(teknihall->message, "id", json_mknumber(id));
	json_append_member(teknihall->message, "temperature", json_mknumber(temperature));
	json_append_member(teknihall->message, "humidity", json_mknumber(humidity*10));
	json_append_member(teknihall->message, "battery", json_mknumber(battery));
}

void teknihallInit(void) {

	protocol_register(&teknihall);
	protocol_set_id(teknihall, "teknihall");
	protocol_device_add(teknihall, "teknihall", "Teknihall Weather Stations");
	protocol_plslen_add(teknihall, 266);
	protocol_conflict_add(teknihall, "alecto");
	protocol_conflict_add(teknihall, "threechan");
	teknihall->devtype = WEATHER;
	teknihall->hwtype = RF433;
	teknihall->pulse = 15;
	teknihall->rawlen = 76;

	options_add(&teknihall->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&teknihall->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&teknihall->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&teknihall->options, 'b', "battery", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&teknihall->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&teknihall->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&teknihall->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&teknihall->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&teknihall->options, 0, "gui-show-battery", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	teknihall->parseCode=&teknihallParseCode;
}
