/*
	Copyright (C) 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"
#include "log.h"
#include "protocol.h"
#include "binary.h"
#include "alecto.h"

void alectoParseCode(void) {
	int i = 0, x = 0, a = 0xf;
	int temperature;
	int negative;
	int humidity;
	int battery;
	int id;

	for(i=1;i<alecto.length-1;i+=2) {
		alecto.binary[x++] = alecto.code[i];
	}

	for(i=0;i<x-4;i+=4) {
		a-=binToDec(alecto.binary, i, i+3);
	}

	alecto.message = NULL;
	if(binToDec(alecto.binary, 32, 35) == (a&0xf)) {
		id = binToDec(alecto.binary, 0, 7);
		if(alecto.binary[11] == 1)
			battery = 1;
		else
			battery = 0;
		temperature = binToDec(alecto.binary, 12, 22);
		if(alecto.binary[23] == 1)
			negative=1;
		else
			negative=0;
		humidity = ((binToDec(alecto.binary, 28, 31)*10)+binToDec(alecto.binary, 24, 27));

		alecto.message = json_mkobject();
		json_append_member(alecto.message, "id", json_mknumber(id));
		json_append_member(alecto.message, "battery", json_mknumber(battery));
		if(negative==1)
			json_append_member(alecto.message, "temperature", json_mknumber(temperature));
		else
			json_append_member(alecto.message, "temperature", json_mknumber(temperature/-1));
		json_append_member(alecto.message, "humidity", json_mknumber(humidity));
	}
}

void alectoInit(void) {

	strcpy(alecto.id, "alecto");
	protocol_add_device(&alecto, "alecto", "Alecto based weather stations");
	alecto.type = WEATHER;
	alecto.header = 14;
	alecto.pulse = 14;
	alecto.footer = 30;
	alecto.length = 74;
	alecto.message = malloc(sizeof(JsonNode));

	alecto.bit = 0;
	alecto.recording = 0;

	options_add(&alecto.options, 'h', "humidity", has_value, config_value, "[0-9]");
	options_add(&alecto.options, 't', "temperature", has_value, config_value, "[0-9]");
	options_add(&alecto.options, 'b', "battery", has_value, config_value, "[0-9]");
	options_add(&alecto.options, 'i', "id", has_value, config_id, "[0-9]");

	alecto.parseCode=&alectoParseCode;

	protocol_register(&alecto);
}
