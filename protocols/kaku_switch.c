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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "kaku_switch.h"

void kakuSwParseRaw() {
}

void kakuSwParseCode() {
}

char *kakuSwParseBinary() {
	char *message = malloc((50*sizeof(char))+1);
	memset(message,'0',sizeof(message));

	int unit = binToDecRev(kaku_switch.binary,28,31);
	int state = kaku_switch.binary[27];
	int group = kaku_switch.binary[26];
	int id = binToDecRev(kaku_switch.binary,0,25);

	sprintf(message,"id %d unit %d state",id,unit);
	if(group == 1)
		strcat(message," all");
	if(state == 1)
		strcat(message," on");
	else
		strcat(message," off");

	return message;
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
	kakuSwCreateLow(2,132);
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
			kakuSwCreateHigh(106-x,106-(x-3));
		}
	}
}

void kakuSwCreateAll(int all) {
	if(all == 1) {
		kakuSwCreateHigh(106,109);
	}
}

void kakuSwCreateState(int state) {
	if(state == 1) {
		kakuSwCreateHigh(110,113);
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
			kakuSwCreateHigh(130-x,130-(x-3));
		}
	}
}

void kakuSwCreateFooter() {
	kaku_switch.raw[131]=(kaku_switch.footer*PULSE_LENGTH);
}

void kakuSwCreateCode(struct options *options) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;

	if(atoi(getOption(options,'i')) > 0)
		id=atoi(getOption(options,'i'));
	if(atoi(getOption(options,'f')) == 1)
		state=0;
	else if(atoi(getOption(options,'t')) == 1)
		state=1;
	if(atoi(getOption(options,'u')) > -1)
		unit = atoi(getOption(options,'u'));
	if(atoi(getOption(options,'a')) == 1)
		all = 1;

	if(id == -1 || unit == -1 || state == -1) {
		fprintf(stderr, "kaku_switch: insufficient number of arguments\n");
	} else {
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

	strcpy(kaku_switch.id,"kaku_switch");
	strcpy(kaku_switch.desc,"KlikAanKlikUit Switches");
	kaku_switch.header = 10;
	kaku_switch.pulse = 5;
	kaku_switch.footer = 38;
	kaku_switch.multiplier[0] = 0.1;
	kaku_switch.multiplier[1] = 0.3;
	kaku_switch.rawLength = 132;
	kaku_switch.binaryLength = 33;
	kaku_switch.repeats = 2;

	kaku_switch.bit = 0;
	kaku_switch.recording = 0;

	struct option kakuSwOptions[] = {
		{"id", required_argument, NULL, 'i'},
		{"unit", required_argument, NULL, 'u'},
		{"all", no_argument, NULL, 'a'},
		{"on", no_argument, NULL, 't'},
		{"off", no_argument, NULL, 'f'},
		{0,0,0,0}
	};

	kaku_switch.options=setOptions(kakuSwOptions);
	kaku_switch.parseRaw=&kakuSwParseRaw;
	kaku_switch.parseCode=&kakuSwParseCode;
	kaku_switch.parseBinary=kakuSwParseBinary;
	kaku_switch.createCode=&kakuSwCreateCode;
	kaku_switch.printHelp=&kakuSwPrintHelp;

	protocol_register(&kaku_switch);
}
