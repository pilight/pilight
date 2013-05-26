#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdio.h>
#include <string.h>

typedef struct {
	char id[25];
	int header[2];
	int crossing;
	int footer;
	float multiplier[2];
	int rawLength;
	int binaryLength;
	int repeats;
	
	int bit;
	int recording;
	int raw[255];
	int code[255];
	int pCode[255];
	int binary[255];
	
	void (*parseRaw)();
	void (*parseCode)();
	void (*parseBinary)();
} protocol;

typedef struct {
	int nr;
	protocol *listeners[255];
} protocols;

protocols protos;

void protocol_register(protocol *proto);
void protocol_unregister(protocol *proto);

#endif