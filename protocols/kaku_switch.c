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

#include "kaku_switch.h"

void kakuSwParseRaw() {
}

void kakuSwParseCode() {
}

void kakuSwParseBinary() {
	int unit = binToDecRev(kaku_switch.binary,28,31);
	int state = kaku_switch.binary[27];
	int group = kaku_switch.binary[26];
	int id = binToDecRev(kaku_switch.binary,0,25);
	
	printf("id: %d, unit: %d, state:",id,unit);
	if(group == 1)
		printf(" all");
	if(state == 1)
		printf(" on");
	else
		printf(" off");
	
	printf("\n");
}

void kakuSwCreateLow(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		kaku_switch.raw[i]=kaku_switch.low;
		kaku_switch.raw[i+1]=kaku_switch.low;
		kaku_switch.raw[i+2]=kaku_switch.low;
		kaku_switch.raw[i+3]=kaku_switch.high;
	}	
}

void kakuSwCreateHigh(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		kaku_switch.raw[i]=kaku_switch.low;
		kaku_switch.raw[i+1]=kaku_switch.high;
		kaku_switch.raw[i+2]=kaku_switch.low;
		kaku_switch.raw[i+3]=kaku_switch.low;
	}
}

void kakuSwClearCode() {
	kakuSwCreateLow(2,132);
}

void kakuSwCreateStart() {
	kaku_switch.raw[0]=kaku_switch.header[0];
	kaku_switch.raw[1]=kaku_switch.header[1];
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
	kaku_switch.raw[131]=kaku_switch.footer;
}

void kakuSwCreateCode(int id, int unit, int state, int all, int dimlevel) {
	kakuSwCreateStart();
	kakuSwClearCode();
	kakuSwCreateId(id);
	kakuSwCreateAll(all);
	kakuSwCreateState(state);
	kakuSwCreateUnit(unit);
	kakuSwCreateFooter();
}

void kakuSwInit() {
		
	strcpy(kaku_switch.id,"kaku_switch");
	strcpy(kaku_switch.desc,"KlikAanKlikUit Switches");
	kaku_switch.header[0] = 265;
	kaku_switch.header[1] = 2660;
	kaku_switch.low = 265;
	kaku_switch.high = 1300;
	kaku_switch.footer = 10200;
	kaku_switch.multiplier[0] = 0.1;
	kaku_switch.multiplier[1] = 0.3;
	kaku_switch.rawLength = 132;
	kaku_switch.binaryLength = 33;	
	kaku_switch.repeats = 2;
	
	kaku_switch.bit = 0;	
	kaku_switch.recording = 0;	
	
	kaku_switch.parseRaw=&kakuSwParseRaw;
	kaku_switch.parseCode=&kakuSwParseCode;
	kaku_switch.parseBinary=&kakuSwParseBinary;
	kaku_switch.createCode=kakuSwCreateCode;
	
	protocol_register(&kaku_switch);
}