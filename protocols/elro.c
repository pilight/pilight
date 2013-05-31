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

#include "elro.h"

void elroParseRaw() {
}

void elroParseCode() {
}

int elroParseBinary() {
	int unit = binToDec(elro.binary,0,4);
	int state = elro.binary[10];
	int check = elro.binary[11];
	int id = binToDec(elro.binary,5,9);

	if(check != state) {
		printf("id: %d, unit: %d, state:",id,unit);
		if(state==1)
			printf(" on\n");
		else
			printf(" off\n");
		return 1;
	} else {
		return 0;
	}
}

void elroCreateLow(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		elro.raw[i]=elro.high;
		elro.raw[i+1]=elro.low;
		elro.raw[i+2]=elro.low;
		elro.raw[i+3]=elro.high;
	}	
}

void elroCreateHigh(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		elro.raw[i]=elro.low;
		elro.raw[i+1]=elro.high;
		elro.raw[i+2]=elro.low;
		elro.raw[i+3]=elro.high;
	}
}

void elroCreateStart() {
	elro.raw[0]=elro.header[0];
	elro.raw[1]=elro.header[1];
}

void elroClearCode() {
	elroCreateLow(2,49);
}

void elroCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;
	
	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			elroCreateHigh(1+(x-3),1+x);
		}
	}
}

void elroCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;
	
	length = decToBinRev(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=(i+1)*4;
			elroCreateHigh(21+(x-3),21+x);
		}
	}
}

void elroCreateState(int state) {
	if(state == 1) {
		elroCreateHigh(42,45);
		elro.raw[46]=elro.high;
		elro.raw[47]=elro.low;
		elro.raw[48]=elro.low;
		elro.raw[49]=elro.footer;
	} else {
		elro.raw[46]=elro.low;
		elro.raw[47]=elro.high;
		elro.raw[48]=elro.low;
		elro.raw[49]=elro.footer;
	}
}

void elroCreateCode(int id, int unit, int state, int all, int dimlevel) {
	elroCreateStart();
	elroClearCode();
	elroCreateUnit(unit);
	elroCreateId(id);
	elroCreateState(state);
}

void elroInit() {
		
	strcpy(elro.id,"elro");
	strcpy(elro.desc,"Elro Switches");
	elro.header[0] = 300;
	elro.header[1] = 1000;
	elro.low = 300;
	elro.high = 1000;
	elro.footer = 13000;
	elro.multiplier[0] = 0.1;
	elro.multiplier[1] = 0.3;
	elro.rawLength = 50;
	elro.binaryLength = 12;	
	elro.repeats = 2;	

	elro.bit = 0;	
	elro.recording = 0;	
	
	elro.parseRaw=&elroParseRaw;
	elro.parseCode=&elroParseCode;
	elro.parseBinary=&elroParseBinary;
	elro.createCode=&elroCreateCode;
	
	protocol_register(&elro);
}