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

void kakuSwInit() {
		
	strcpy(kaku_switch.id,"kaku_switch");
	kaku_switch.header[0] = 275;
	kaku_switch.header[1] = 2800;
	kaku_switch.crossing = 1000;
	kaku_switch.footer = 11000;
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
	
	protocol_register(&kaku_switch);
}