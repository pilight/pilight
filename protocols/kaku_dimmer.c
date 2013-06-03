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

#include "kaku_dimmer.h"

void kakuDimParseRaw() {
}

void kakuDimParseCode() {
}

int kakuDimParseBinary() {
	int dim = binToDecRev(kaku_dimmer.binary,32,35);
	int unit = binToDecRev(kaku_dimmer.binary,28,31);
	int state = kaku_dimmer.binary[27];
	int group = kaku_dimmer.binary[26];
	int id = binToDecRev(kaku_dimmer.binary,0,25);
	
	printf("id: %d, unit: %d,",id,unit);
	if(dim > 0) {
		printf(" dim: %d",dim);
	} else {
		printf(" state:");
		if(group == 1)
			printf(" all");	
		if(state == 1)
			printf(" on");
		else
			printf(" off");
	}
	printf("\n");
	return 1;
}

void kakuDimCreateLow(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		kaku_dimmer.raw[i]=kaku_dimmer.low;
		kaku_dimmer.raw[i+1]=kaku_dimmer.low;
		kaku_dimmer.raw[i+2]=kaku_dimmer.low;
		kaku_dimmer.raw[i+3]=kaku_dimmer.high;
	}	
}

void kakuDimCreateHigh(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=4) {
		kaku_dimmer.raw[i]=kaku_dimmer.low;
		kaku_dimmer.raw[i+1]=kaku_dimmer.high;
		kaku_dimmer.raw[i+2]=kaku_dimmer.low;
		kaku_dimmer.raw[i+3]=kaku_dimmer.low;
	}
}

void kakuDimClearCode() {
	kakuDimCreateLow(2,147);
}

void kakuDimCreateStart() {
	kaku_dimmer.raw[0]=kaku_dimmer.header[0];
	kaku_dimmer.raw[1]=kaku_dimmer.header[1];
}

void kakuDimCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;
	
	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			kakuDimCreateHigh(106-x,106-(x-3));
		}
	}
}

void kakuDimCreateAll(int all) {
	if(all == 1) {
		kakuDimCreateHigh(106,109);
	}
}

void kakuDimCreateState(int state) {
	kaku_dimmer.raw[110]=kaku_dimmer.low;
	kaku_dimmer.raw[111]=kaku_dimmer.low;
	kaku_dimmer.raw[112]=kaku_dimmer.low;
	kaku_dimmer.raw[113]=kaku_dimmer.low;
}

void kakuDimCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;
	
	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			kakuDimCreateHigh(130-x,130-(x-3));
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
			kakuDimCreateHigh(146-x,146-(x-3));
		}
	}
}

void kakuDimCreateFooter() {
	kaku_dimmer.raw[147]=kaku_dimmer.footer;
}

void kakuDimCreateCode(struct options *options) {
	int id = atoi(getOption(options,'i'));
	int unit = atoi(getOption(options,'u'));
	int state = atoi(getOption(options,'f')) || 1;
	int all = atoi(getOption(options,'a'));
	int dimlevel = atoi(getOption(options,'d')) || 15;
	if(id == 0)
		fprintf(stderr, "kaku_dimmer: insufficient number of arguments\n");
		
	kakuDimCreateStart();
	kakuDimClearCode();
	kakuDimCreateId(id);
	kakuDimCreateAll(all);
	kakuDimCreateState(state);
	kakuDimCreateUnit(unit);
	kakuDimCreateDimlevel(dimlevel);
	kakuDimCreateFooter();
}

void kakuDimPrintHelp() {
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
	printf("\t -d --dimlevel=dimlevel\t\tsend a specific dimlevel\n");
}


void kakuDimInit() {
		
	strcpy(kaku_dimmer.id,"kaku_dimmer");
	strcpy(kaku_dimmer.desc,"KlikAanKlikUit Dimmers");
	kaku_dimmer.header[0] = 265;
	kaku_dimmer.header[1] = 2660;
	kaku_dimmer.low = 265;
	kaku_dimmer.high = 1300;
	kaku_dimmer.footer = 10200;
	kaku_dimmer.multiplier[0] = 0.1;
	kaku_dimmer.multiplier[1] = 0.3;
	kaku_dimmer.rawLength = 148;
	kaku_dimmer.binaryLength = 37;	
	kaku_dimmer.repeats = 2;
	
	kaku_dimmer.bit = 0;	
	kaku_dimmer.recording = 0;	
	
	struct option kakuDimOptions[] = {
		{"unit", required_argument, NULL, 'u'},
		{"id", required_argument, NULL, 'i'},
		{"all", required_argument, NULL, 'a'},
		{"dimlevel", required_argument, NULL, 'd'},
		{0,0,0,0}
	};

	kaku_dimmer.options=setOptions(kakuDimOptions);
	kaku_dimmer.parseRaw=&kakuDimParseRaw;
	kaku_dimmer.parseCode=&kakuDimParseCode;
	kaku_dimmer.parseBinary=&kakuDimParseBinary;
	kaku_dimmer.createCode=&kakuDimCreateCode;
	kaku_dimmer.printHelp=&kakuDimPrintHelp;
	
	protocol_register(&kaku_dimmer);
}