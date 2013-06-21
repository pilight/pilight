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
#include "kaku_dimmer.h"

void kakuDimCreateMessage(int id, int unit, int state, int all, int dimlevel) {
	int i = 0;

	memset(kaku_dimmer.message, '\0', sizeof(kaku_dimmer.message));

	i = sprintf(kaku_dimmer.message, "id %d unit %d", id, unit);
	if(dimlevel > 0) {
		sprintf(kaku_dimmer.message+i, " dim %d", dimlevel);
	} else {
		if(all == 1)
			strcat(kaku_dimmer.message, " all");	
		if(state == 1)
			strcat(kaku_dimmer.message, " on");
		else
			strcat(kaku_dimmer.message, " off");
	}
}

void kakuDimParseBinary() {
	int dimlevel = binToDecRev(kaku_dimmer.binary, 32, 35);
	int unit = binToDecRev(kaku_dimmer.binary, 28, 31);
	int state = kaku_dimmer.binary[27];
	int all = kaku_dimmer.binary[26];
	int id = binToDecRev(kaku_dimmer.binary, 0, 25);
	kakuDimCreateMessage(id, unit, state, all, dimlevel);
}

void kakuDimCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		kaku_dimmer.raw[i]=(PULSE_LENGTH);
		kaku_dimmer.raw[i+1]=(PULSE_LENGTH);
		kaku_dimmer.raw[i+2]=(PULSE_LENGTH);
		kaku_dimmer.raw[i+3]=(kaku_dimmer.pulse*PULSE_LENGTH);
	}
}

void kakuDimCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		kaku_dimmer.raw[i]=(PULSE_LENGTH);
		kaku_dimmer.raw[i+1]=(kaku_dimmer.pulse*PULSE_LENGTH);
		kaku_dimmer.raw[i+2]=(PULSE_LENGTH);
		kaku_dimmer.raw[i+3]=(PULSE_LENGTH);
	}
}

void kakuDimClearCode() {
	kakuDimCreateLow(2,147);
}

void kakuDimCreateStart() {
	kaku_dimmer.raw[0]=PULSE_LENGTH;
	kaku_dimmer.raw[1]=(kaku_dimmer.header*PULSE_LENGTH);
}

void kakuDimCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			kakuDimCreateHigh(106-x, 106-(x-3));
		}
	}
}

void kakuDimCreateAll(int all) {
	if(all == 1) {
		kakuDimCreateHigh(106, 109);
	}
}

void kakuDimCreateState(int state) {
	kaku_dimmer.raw[110]=(PULSE_LENGTH);
	kaku_dimmer.raw[111]=(PULSE_LENGTH);
	kaku_dimmer.raw[112]=(PULSE_LENGTH);
	kaku_dimmer.raw[113]=(PULSE_LENGTH);
}

void kakuDimCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			kakuDimCreateHigh(130-x, 130-(x-3));
		}
	}
}

void kakuDimCreateDimlevel(int dimlevel) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(dimlevel, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			kakuDimCreateHigh(146-x, 146-(x-3));
		}
	}
}

void kakuDimCreateFooter() {
	kaku_dimmer.raw[147]=(kaku_dimmer.footer*PULSE_LENGTH);
}

void kakuDimCreateCode(struct options_t *options) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	int dimlevel = -1;

	if(getOptionValById(&options, 'i') != NULL)
		id=atoi(getOptionValById(&options, 'i'));
	if(getOptionValById(&options, 'f') != NULL)
		state=0;
	else if(getOptionValById(&options, 't') != NULL)
		state=1;
	if(getOptionValById(&options, 'd') != NULL)
		dimlevel=atoi(getOptionValById(&options, 'd'));
	if(getOptionValById(&options, 'u') != NULL)
		unit = atoi(getOptionValById(&options, 'u'));
	if(getOptionValById(&options, 'a') != NULL)
		all = 1;

	if(id == -1 || unit == -1 || dimlevel == -1) {
		logprintf(LOG_ERR, "kaku_dimmer: insufficient number of arguments");
		exit(EXIT_FAILURE);
	} else if(id > 67108863 || id < 1) {
		logprintf(LOG_ERR, "kaku_dimmer: invalid id range");
		exit(EXIT_FAILURE);
	} else if(unit > 16 || unit < 0) {
		logprintf(LOG_ERR, "kaku_dimmer: invalid unit range");
		exit(EXIT_FAILURE);
	} else if(dimlevel > 16 || dimlevel < 0) {
		logprintf(LOG_ERR, "kaku_dimmer: invalid dimlevel range");
		exit(EXIT_FAILURE);
	} else {
		kakuDimCreateMessage(id, unit, state, all, dimlevel);
		kakuDimCreateStart();
		kakuDimClearCode();
		kakuDimCreateId(id);
		kakuDimCreateAll(all);
		kakuDimCreateState(state);
		kakuDimCreateUnit(unit);
		kakuDimCreateDimlevel(dimlevel);
		kakuDimCreateFooter();
	}
}

void kakuDimPrintHelp() {
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
	printf("\t -d --dimlevel=dimlevel\t\tsend a specific dimlevel\n");
}


void kakuDimInit() {

	strcpy(kaku_dimmer.id, "kaku_dimmer");
	strcpy(kaku_dimmer.desc, "KlikAanKlikUit Dimmers");
	kaku_dimmer.type = DIMMER;
	kaku_dimmer.header = 10;
	kaku_dimmer.pulse = 5;
	kaku_dimmer.footer = 38;
	kaku_dimmer.multiplier[0] = 0.1;
	kaku_dimmer.multiplier[1] = 0.3;
	kaku_dimmer.rawLength = 148;
	kaku_dimmer.binaryLength = 37;
	kaku_dimmer.repeats = 2;
	kaku_dimmer.message = malloc((50*sizeof(char))+1);

	kaku_dimmer.bit = 0;
	kaku_dimmer.recording = 0;

	addOption(&kaku_dimmer.options, 'd', "dimlevel", required_argument, 0, "[0-9]");
	addOption(&kaku_dimmer.options, 'a', "all", no_argument, 0, NULL);
	addOption(&kaku_dimmer.options, 'u', "unit", required_argument, config_required, "[0-9]");
	addOption(&kaku_dimmer.options, 'i', "id", required_argument, config_required, "[0-9]");

	kaku_dimmer.parseBinary=&kakuDimParseBinary;
	kaku_dimmer.createCode=&kakuDimCreateCode;
	kaku_dimmer.printHelp=&kakuDimPrintHelp;

	protocol_register(&kaku_dimmer);
}
