#include "kaku_dimmer.h"

void kakuDimParseRaw() {
}

void kakuDimParseCode() {
}

void kakuDimParseBinary() {
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
}

void kakuDimInit() {
		
	strcpy(kaku_dimmer.id,"kaku_dimmer");
	kaku_dimmer.header[0] = 275;
	kaku_dimmer.header[1] = 2800;
	kaku_dimmer.crossing = 1000;
	kaku_dimmer.footer = 11000;
	kaku_dimmer.multiplier[0] = 0.1;
	kaku_dimmer.multiplier[1] = 0.3;
	kaku_dimmer.rawLength = 149;
	kaku_dimmer.binaryLength = 37;	
	kaku_dimmer.repeats = 2;
	
	kaku_dimmer.bit = 0;	
	kaku_dimmer.recording = 0;	
	
	kaku_dimmer.parseRaw=&kakuDimParseRaw;
	kaku_dimmer.parseCode=&kakuDimParseCode;
	kaku_dimmer.parseBinary=&kakuDimParseBinary;
	
	protocol_register(&kaku_dimmer);
}