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

#include "kaku_old.h"

void kakuOldParseRaw() {
}

void kakuOldParseCode() {
}

int kakuOldParseBinary() {
	int unit = binToDec(kaku_old.binary,0,4);
	int state = kaku_old.binary[11];
	int check = kaku_old.binary[10];
	int id = binToDec(kaku_old.binary,5,9);

	printf("id: %d, unit: %d, state:",id,unit);
	if(state==0)
		printf(" on\n");
	else
		printf(" off\n");
	return 1;
}

void kakuOldCreateLow(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		kaku_old.raw[i]=kaku_old.high;
		kaku_old.raw[i+1]=kaku_old.low;
		kaku_old.raw[i+2]=kaku_old.low;
		kaku_old.raw[i+3]=kaku_old.high;
	}	
}

void kakuOldCreateHigh(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		kaku_old.raw[i]=kaku_old.low;
		kaku_old.raw[i+1]=kaku_old.high;
		kaku_old.raw[i+2]=kaku_old.low;
		kaku_old.raw[i+3]=kaku_old.high;
	}
}

void kakuOldCreateStart() {
	kaku_old.raw[0]=kaku_old.header[0];
	kaku_old.raw[1]=kaku_old.header[1];
}

void kakuOldClearCode() {
	kakuOldCreateLow(2,49);
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
	if(state == 1) {
		kaku_old.raw[46]=kaku_old.high;
		kaku_old.raw[47]=kaku_old.low;
		kaku_old.raw[48]=kaku_old.low;
		kaku_old.raw[49]=kaku_old.footer;
	} else {
		kaku_old.raw[46]=kaku_old.low;
		kaku_old.raw[47]=kaku_old.high;
		kaku_old.raw[48]=kaku_old.low;
		kaku_old.raw[49]=kaku_old.footer;
	}
}

void kakuOldCreateCode(int id, int unit, int state, int all, int dimlevel) {
	kakuOldCreateStart();
	kakuOldClearCode();
	kakuOldCreateUnit(unit);
	kakuOldCreateId(id);
	kakuOldCreateState(state);
}

void kakuOldInit() {
		
	strcpy(kaku_old.id,"kaku_old");
	strcpy(kaku_old.desc,"Old KlikAanKlikUit Switches");
	kaku_old.header[0] = 300;
	kaku_old.header[1] = 1000;
	kaku_old.low = 300;
	kaku_old.high = 1000;
	kaku_old.footer = 13000;
	kaku_old.multiplier[0] = 0.1;
	kaku_old.multiplier[1] = 0.3;
	kaku_old.rawLength = 50;
	kaku_old.binaryLength = 12;	
	kaku_old.repeats = 2;	

	kaku_old.bit = 0;	
	kaku_old.recording = 0;	
	
	kaku_old.parseRaw=&kakuOldParseRaw;
	kaku_old.parseCode=&kakuOldParseCode;
	kaku_old.parseBinary=&kakuOldParseBinary;
	kaku_old.createCode=&kakuOldCreateCode;
	
	protocol_register(&kaku_old);
}