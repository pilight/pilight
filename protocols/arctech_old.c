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
#include "arctech_old.h"

void arctechOldCreateMessage(int id, int unit, int state) {
	memset(arctech_old.message, '\0', sizeof(arctech_old.message));

	sprintf(arctech_old.message, "id %d unit %d", id, unit);
	if(state==0)
		strcat(arctech_old.message," on");
	else
		strcat(arctech_old.message," off");
}

void arctechOldParseBinary() {
	int unit = binToDec(arctech_old.binary, 0, 4);
	int state = arctech_old.binary[11];
	int id = binToDec(arctech_old.binary, 5, 9);
	arctechOldCreateMessage(id, unit, state);
}

void arctechOldCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_old.raw[i]=(PULSE_LENGTH);
		arctech_old.raw[i+1]=(arctech_old.pulse*PULSE_LENGTH);
		arctech_old.raw[i+2]=(arctech_old.pulse*PULSE_LENGTH);
		arctech_old.raw[i+3]=(PULSE_LENGTH);
	}
}

void arctechOldCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_old.raw[i]=(PULSE_LENGTH);
		arctech_old.raw[i+1]=(arctech_old.pulse*PULSE_LENGTH);
		arctech_old.raw[i+2]=(PULSE_LENGTH);
		arctech_old.raw[i+3]=(arctech_old.pulse*PULSE_LENGTH);
	}
}

void arctechOldClearCode() {
	arctechOldCreateLow(0,49);
}

void arctechOldCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			arctechOldCreateHigh(1+(x-3),1+x);
		}
	}
}

void arctechOldCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			arctechOldCreateHigh(21+(x-3), 21+x);
		}
	}
}

void arctechOldCreateState(int state) {
	if(state == 0) {
		arctechOldCreateHigh(44,47);
	}
}

void arctechOldCreateFooter() {
	arctech_old.raw[48]=(PULSE_LENGTH);
	arctech_old.raw[49]=(arctech_old.footer*PULSE_LENGTH);
}


void arctechOldCreateCode(struct options_t *options) {
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
		logprintf(LOG_ERR, "arctech_old: insufficient number of arguments");
		exit(EXIT_FAILURE);
	} else if(id > 32 || id < 0) {
		logprintf(LOG_ERR, "arctech_old: invalid id range");
		exit(EXIT_FAILURE);
	} else if(unit > 32 || unit < 0) {
		logprintf(LOG_ERR, "arctech_old: invalid unit range");
		exit(EXIT_FAILURE);
	} else {
		arctechOldCreateMessage(id, unit, state);
		arctechOldClearCode();
		arctechOldCreateUnit(unit);
		arctechOldCreateId(id);
		arctechOldCreateState(state);
		arctechOldCreateFooter();
	}
}

void arctechOldPrintHelp() {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -t --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void arctechOldInit() {

	strcpy(arctech_old.id, "archtech_old");
	addDevice(&arctech_old, "kaku_old", "Old KlikAanKlikUit Switches");
	arctech_old.type = SWITCH;
	arctech_old.header = 4;
	arctech_old.pulse = 4;
	arctech_old.footer = 45;
	arctech_old.multiplier[0] = 0.1;
	arctech_old.multiplier[1] = 0.3;
	arctech_old.rawLength = 50;
	arctech_old.binaryLength = 12;
	arctech_old.repeats = 2;
	arctech_old.message = malloc((50*sizeof(char))+1);

	arctech_old.bit = 0;
	arctech_old.recording = 0;

	addOption(&arctech_old.options, 't', "on", no_argument, 0, NULL);	
	addOption(&arctech_old.options, 'f', "off", no_argument, 0, NULL);	
	addOption(&arctech_old.options, 'u', "unit", required_argument, config_id, "[0-9]");
	addOption(&arctech_old.options, 'i', "id", required_argument, config_id, "[0-9]");

	arctech_old.parseBinary=arctechOldParseBinary;
	arctech_old.createCode=&arctechOldCreateCode;
	arctech_old.printHelp=&arctechOldPrintHelp;

	protocol_register(&arctech_old);
}
