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
#include <getopt.h>
#include "config.h"
#include "protocol.h"
#include "binary.h"
#include "alecto.h"

void alectoParseRaw() {
	int i, x = 0, a = 0xf;
	memset(alecto.message,'0',sizeof(alecto.message));
	
	for(i=1;i<alecto.rawLength-1;i++) {
		if(alecto.raw[i]>(PULSE_LENGTH*3)) {
			if(alecto.raw[i]>(PULSE_LENGTH*9))
				alecto.binary[x++] = 1;
			else
				alecto.binary[x++] = 0;
		}
	}
	
	for(i=0;i<x-4;i+=4) {
		a-=binToDec(alecto.binary,i,i+3);
	}
	if(binToDec(alecto.binary,32,35) == (a&0xf)) {	
		sprintf(alecto.message,"id %d battery %d temperature %d humidity %d",binToDecRev(alecto.binary,0,7),alecto.binary[8],binToDecRev(alecto.binary,12,23),((binToDecRev(alecto.binary,24,27)*10)+binToDecRev(alecto.binary,28,31)));
	}
}

void alectoParseCode() {
}

char *alectoParseBinary() {
	if(strlen(alecto.message) > 0)
		return alecto.message;
	else
		return (0);
}

void alectoInit() {

	strcpy(alecto.id,"alecto");
	strcpy(alecto.desc,"Alecto based Weather stations");
	alecto.type = WEATHER;
	alecto.header = 14;
	alecto.pulse = 7;
	alecto.footer = 30;
	alecto.multiplier[0] = 0.1;
	alecto.multiplier[1] = 0.3;
	alecto.rawLength = 74;
	alecto.binaryLength = 18;
	alecto.repeats = 2;
	alecto.message = malloc((50*sizeof(char))+1);
	
	alecto.bit = 0;
	alecto.recording = 0;

	struct option alectoOptions[] = {
		{0,0,0,0}
	};

	alecto.options=setOptions(alectoOptions);
	alecto.parseRaw=&alectoParseRaw;
	alecto.parseCode=&alectoParseCode;
	alecto.parseBinary=alectoParseBinary;

	protocol_register(&alecto);
}
