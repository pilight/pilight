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
#include "kaku_switch.h"

void kakuSwCreateMessage(int id, int unit, int state, int all) {
	memset(kaku_switch.message, '\0', sizeof(kaku_switch.message));

	sprintf(kaku_switch.message, "id %d unit %d", id, unit);
	if(all == 1)
		strcat(kaku_switch.message, " all");
	if(state == 1)
		strcat(kaku_switch.message, " on");
	else
		strcat(kaku_switch.message, " off");
}

void kakuSwParseBinary() {
	int unit = binToDecRev(kaku_switch.binary, 28, 31);
	int state = kaku_switch.binary[27];
	int all = kaku_switch.binary[26];
	int id = binToDecRev(kaku_switch.binary, 0, 25);
	kakuSwCreateMessage(id, unit, state, all);
}

void kakuSwCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		kaku_switch.raw[i]=(PULSE_LENGTH);
		kaku_switch.raw[i+1]=(PULSE_LENGTH);
		kaku_switch.raw[i+2]=(PULSE_LENGTH);
		kaku_switch.raw[i+3]=(kaku_switch.pulse*PULSE_LENGTH);
	}
}

void kakuSwCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		kaku_switch.raw[i]=(PULSE_LENGTH);
		kaku_switch.raw[i+1]=(kaku_switch.pulse*PULSE_LENGTH);
		kaku_switch.raw[i+2]=(PULSE_LENGTH);
		kaku_switch.raw[i+3]=(PULSE_LENGTH);
	}
}

void kakuSwClearCode() {
	kakuSwCreateLow(2, 132);
}

void kakuSwCreateStart() {
	kaku_switch.raw[0]=(PULSE_LENGTH);
	kaku_switch.raw[1]=(kaku_switch.header*PULSE_LENGTH);
}

void kakuSwCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			kakuSwCreateHigh(106-x, 106-(x-3));
		}
	}
}

void kakuSwCreateAll(int all) {
	if(all == 1) {
		kakuSwCreateHigh(106, 109);
	}
}

void kakuSwCreateState(int state) {
	if(state == 1) {
		kakuSwCreateHigh(110, 113);
	}
}

void kakuSwCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			kakuSwCreateHigh(130-x, 130-(x-3));
		}
	}
}

void kakuSwCreateFooter() {
	kaku_switch.raw[131]=(kaku_switch.footer*PULSE_LENGTH);
}

void kakuSwCreateCode(struct options_t *options) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;

	if(getOptionValById(&options, 'i') != NULL)
		id=atoi(getOptionValById(&options, 'i'));
	if(getOptionValById(&options, 'f') != NULL)
		state=0;
	else if(getOptionValById(&options, 't') != NULL)
		state=1;
	if(getOptionValById(&options, 'u') != NULL)
		unit = atoi(getOptionValById(&options, 'u'));
	if(getOptionValById(&options, 'a') != NULL)
		all = 1;

	if(id == -1 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "kaku_switch: insufficient number of arguments");
		exit(EXIT_FAILURE);
	} else if(id > 67108863 || id < 1) {
		logprintf(LOG_ERR, "kaku_switch: invalid id range");
		exit(EXIT_FAILURE);
	} else if(unit > 16 || unit < 0) {
		logprintf(LOG_ERR, "kaku_switch: invalid unit range");
		exit(EXIT_FAILURE);
	} else {
		kakuSwCreateMessage(id, unit, state, all);	
		kakuSwCreateStart();
		kakuSwClearCode();
		kakuSwCreateId(id);
		kakuSwCreateAll(all);
		kakuSwCreateState(state);
		kakuSwCreateUnit(unit);
		kakuSwCreateFooter();
	}
}

void kakuSwPrintHelp() {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

void kakuSwInit() {

	strcpy(kaku_switch.id, "kaku_switch");
	strcpy(kaku_switch.desc, "KlikAanKlikUit Switches");
	kaku_switch.type = SWITCH;
	kaku_switch.header = 10;
	kaku_switch.pulse = 5;
	kaku_switch.footer = 38;
	kaku_switch.multiplier[0] = 0.1;
	kaku_switch.multiplier[1] = 0.3;
	kaku_switch.rawLength = 132;
	kaku_switch.binaryLength = 33;
	kaku_switch.repeats = 2;
	kaku_switch.message = malloc((50*sizeof(char))+1);
	
	kaku_switch.bit = 0;
	kaku_switch.recording = 0;

	addOption(&kaku_switch.options, 'a', "all", no_argument, 0, NULL);
	addOption(&kaku_switch.options, 't', "on", no_argument, 0, NULL);
	addOption(&kaku_switch.options, 'f', "off", no_argument, 0, NULL);
	addOption(&kaku_switch.options, 'u', "unit", required_argument, config_required, "[0-9]");
	addOption(&kaku_switch.options, 'i', "id", required_argument, config_required, "[0-9]");

	kaku_switch.parseBinary=&kakuSwParseBinary;
	kaku_switch.createCode=&kakuSwCreateCode;
	kaku_switch.printHelp=&kakuSwPrintHelp;

	protocol_register(&kaku_switch);
}
