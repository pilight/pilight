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
#include "kaku_old.h"

void kakuOldParseBinary() {
	memset(kaku_old.message,'\0',sizeof(kaku_old.message));
	
	int unit = binToDec(kaku_old.binary,0,4);
	int state = kaku_old.binary[11];
	int id = binToDec(kaku_old.binary,5,9);

	sprintf(kaku_old.message,"id %d unit %d",id,unit);
	if(state==0)
		strcat(kaku_old.message," on");
	else
		strcat(kaku_old.message," off");
}

void kakuOldCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		kaku_old.raw[i]=(PULSE_LENGTH);
		kaku_old.raw[i+1]=(kaku_old.pulse*PULSE_LENGTH);
		kaku_old.raw[i+2]=(kaku_old.pulse*PULSE_LENGTH);
		kaku_old.raw[i+3]=(PULSE_LENGTH);
	}
}

void kakuOldCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		kaku_old.raw[i]=(PULSE_LENGTH);
		kaku_old.raw[i+1]=(kaku_old.pulse*PULSE_LENGTH);
		kaku_old.raw[i+2]=(PULSE_LENGTH);
		kaku_old.raw[i+3]=(kaku_old.pulse*PULSE_LENGTH);
	}
}

void kakuOldClearCode() {
	kakuOldCreateLow(0,49);
}

void kakuOldCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			kakuOldCreateHigh(1+(x-3),1+x);
		}
	}
}

void kakuOldCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			kakuOldCreateHigh(21+(x-3),21+x);
		}
	}
}

void kakuOldCreateState(int state) {
	if(state == 0) {
		kakuOldCreateHigh(44,47);
	}
}

void kakuOldCreateFooter() {
	kaku_old.raw[48]=(PULSE_LENGTH);
	kaku_old.raw[49]=(kaku_old.footer*PULSE_LENGTH);
}


void kakuOldCreateCode(struct options_t *options) {
	int id = -1;
	int unit = -1;
	int state = -1;

	if(getOptionValById(&options,'i') != NULL)
		id=atoi(getOptionValById(&options,'i'));
	if(getOptionValById(&options,'f') != NULL)
		state=0;
	else if(getOptionValById(&options,'t') != NULL)
		state=1;
	if(getOptionValById(&options,'u') != NULL)
		unit = atoi(getOptionValById(&options,'u'));

	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "kaku_old: insufficient number of arguments");
		exit(EXIT_FAILURE);
	} else if(id > 32 || id < 0) {
		logprintf(LOG_ERR, "kaku_old: invalid id range");
		exit(EXIT_FAILURE);
	} else if(unit > 32 || unit < 0) {
		logprintf(LOG_ERR, "kaku_old: invalid unit range");
		exit(EXIT_FAILURE);
	} else {
		kakuOldClearCode();
		kakuOldCreateUnit(unit);
		kakuOldCreateId(id);
		kakuOldCreateState(state);
		kakuOldCreateFooter();
	}
}

void kakuOldPrintHelp() {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -t --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void kakuOldInit() {

	strcpy(kaku_old.id,"kaku_old");
	strcpy(kaku_old.desc,"Old KlikAanKlikUit Switches");
	kaku_old.type = SWITCH;
	kaku_old.header = 4;
	kaku_old.pulse = 4;
	kaku_old.footer = 45;
	kaku_old.multiplier[0] = 0.1;
	kaku_old.multiplier[1] = 0.3;
	kaku_old.rawLength = 50;
	kaku_old.binaryLength = 12;
	kaku_old.repeats = 2;
	kaku_old.message = malloc((50*sizeof(char))+1);

	kaku_old.bit = 0;
	kaku_old.recording = 0;

	kaku_old.options = malloc(4*sizeof(struct options_t));
	addOption(&kaku_old.options, 't', "on", no_argument, 0, 1, NULL);	
	addOption(&kaku_old.options, 'f', "off", no_argument, 0, 1, NULL);	
	addOption(&kaku_old.options, 'u', "unit", required_argument, config_required, 1, "[0-9]");
	addOption(&kaku_old.options, 'i', "id", required_argument, config_required, 1, "[0-9]");

	kaku_old.parseBinary=kakuOldParseBinary;
	kaku_old.createCode=&kakuOldCreateCode;
	kaku_old.printHelp=&kakuOldPrintHelp;

	protocol_register(&kaku_old);
}
