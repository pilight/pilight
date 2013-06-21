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
#include "config.h"
#include "log.h"
#include "protocol.h"
#include "binary.h"
#include "alecto.h"

void alectoParseCode() {
	int i = 0, x = 0, a = 0xf;
	int id;
	int temperature;
	int negative;
	int humidity;
	int battery;

	memset(alecto.message, '\0', sizeof(alecto.message));

	for(i=1;i<alecto.rawLength-1;i+=2) {
		alecto.binary[x++] = alecto.code[i];
	}

	for(i=0;i<x-4;i+=4) {
		a-=binToDec(alecto.binary, i, i+3);
	}

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
		i=0;
		i+=sprintf(alecto.message+i, "id %d ", id);
		i+=sprintf(alecto.message+i, "battery %d ", battery);
		if(negative==1)
			i+=sprintf(alecto.message+i, "temperature -%d ", temperature);
		else
			i+=sprintf(alecto.message+i, "temperature %d ", temperature);
		i+=sprintf(alecto.message+i, "humidity %d", humidity);
	}
}

void alectoInit() {

	strcpy(alecto.id, "alecto");
	strcpy(alecto.desc, "Alecto based weather stations");
	alecto.type = WEATHER;
	alecto.header = 14;
	alecto.pulse = 14;
	alecto.footer = 30;
	alecto.multiplier[0] = 0.1;
	alecto.multiplier[1] = 0.3;
	alecto.rawLength = 74;
	alecto.repeats = 1;
	alecto.message = malloc((50*sizeof(char))+1);

	alecto.bit = 0;
	alecto.recording = 0;

	alecto.parseCode=&alectoParseCode;

	protocol_register(&alecto);
}
