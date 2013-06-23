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
#include "sartano.h"

void sartanoCreateMessage(int id, int unit, int state) {
	memset(sartano.message, '\0', sizeof(sartano.message));

	sprintf(sartano.message, "id %d unit %d", id, unit);
	if(state==1)
		strcat(sartano.message, " on");
	else
		strcat(sartano.message, " off");
}

void sartanoParseBinary() {
	int unit = binToDec(sartano.binary, 0, 4);
	int state = sartano.binary[10];
	int check = sartano.binary[11];
	int id = binToDec(sartano.binary, 5, 9);
	if(check != state)
		sartanoCreateMessage(id, unit, state);
}

void sartanoCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		sartano.raw[i]=(PULSE_LENGTH);
		sartano.raw[i+1]=(sartano.pulse*PULSE_LENGTH);
		sartano.raw[i+2]=(sartano.pulse*PULSE_LENGTH);
		sartano.raw[i+3]=(PULSE_LENGTH);
	}
}

void sartanoCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		sartano.raw[i]=(PULSE_LENGTH);
		sartano.raw[i+1]=(sartano.pulse*PULSE_LENGTH);

		sartano.raw[i+2]=(PULSE_LENGTH);
		sartano.raw[i+3]=(sartano.pulse*PULSE_LENGTH);
	}
}
void sartanoClearCode() {
	sartanoCreateLow(0,49);
}

void sartanoCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			sartanoCreateHigh(1+(x-3), 1+x);
		}
	}
}

void sartanoCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			sartanoCreateHigh(21+(x-3), 21+x);
		}
	}
}

void sartanoCreateState(int state) {
	if(state == 0) {
		sartanoCreateHigh(44, 47);
	} else {
		sartanoCreateHigh(40, 43);
	}
}

void sartanoCreateFooter() {
	sartano.raw[48]=(PULSE_LENGTH);
	sartano.raw[49]=(sartano.footer*PULSE_LENGTH);
}

void sartanoCreateCode(struct options_t *options) {
	int id = -1;
	int unit = -1;
	int state = -1;

	if(getOptionValById(&options, 'i') != NULL)
		id=atoi(getOptionValById(&options, 'i'));
	if(getOptionValById(&options, 'f') != NULL)
		state=0;
	else if(getOptionValById(&options, 't') != NULL)
		state=1;
	if(getOptionValById(&options, 'u') != NULL)
		unit = atoi(getOptionValById(&options, 'u'));

	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "sartano: insufficient number of arguments");
		exit(EXIT_FAILURE);
	} else if(id > 32 || id < 0) {
		logprintf(LOG_ERR, "sartano: invalid id range");
		exit(EXIT_FAILURE);
	} else if(unit > 32 || unit < 0) {
		logprintf(LOG_ERR, "sartano: invalid unit range");
		exit(EXIT_FAILURE);
	} else {
		sartanoCreateMessage(id, unit, state);	
		sartanoClearCode();
		sartanoCreateUnit(unit);
		sartanoCreateId(id);
		sartanoCreateState(state);
		sartanoCreateFooter(state);
	}
}

void sartanoPrintHelp() {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void sartanoInit() {

	strcpy(sartano.id, "sartano");
	addDevice(&sartano, "elro", "Elro Switches");
	sartano.type = SWITCH;
	sartano.header = 4;
	sartano.pulse = 4;
	sartano.footer = 45;
	sartano.multiplier[0] = 0.1;
	sartano.multiplier[1] = 0.3;
	sartano.rawLength = 50;
	sartano.binaryLength = 12;
	sartano.repeats = 2;
	sartano.message = malloc((50*sizeof(char))+1);

	sartano.bit = 0;
	sartano.recording = 0;

	addOption(&sartano.options, 't', "on", no_argument, 0, NULL);	
	addOption(&sartano.options, 'f', "off", no_argument, 0, NULL);
	addOption(&sartano.options, 'u', "unit", required_argument, config_id, "[0-9]");
	addOption(&sartano.options, 'i', "id", required_argument, config_id, "[0-9]");

	sartano.parseBinary=&sartanoParseBinary;
	sartano.createCode=&sartanoCreateCode;
	sartano.printHelp=&sartanoPrintHelp;

	protocol_register(&sartano);
}
