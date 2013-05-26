#include "elro.h"

void elroParseRaw() {
}

void elroParseCode() {
}

void elroParseBinary() {
	int unit = binToDec(elro.binary,0,4);
	int state = elro.binary[10];
	int id = binToDec(elro.binary,5,9);
	
	printf("id: %d, unit: %d, state: ",id,unit);
	if(state==1)
		printf("on\n");
	else
		printf("off\n");
}

void elroInit() {
		
	strcpy(elro.id,"elro");
	elro.header[0] = 300;
	elro.header[1] = 1000;
	elro.crossing = 750;
	elro.footer = 13000;
	elro.multiplier[0] = 0.1;
	elro.multiplier[1] = 0.3;
	elro.rawLength = 51;
	elro.binaryLength = 13;	
	elro.repeats = 2;	

	elro.bit = 0;	
	elro.recording = 0;	
	
	elro.parseRaw=&elroParseRaw;
	elro.parseCode=&elroParseCode;
	elro.parseBinary=&elroParseBinary;
	
	protocol_register(&elro);
}