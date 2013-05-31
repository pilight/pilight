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

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdio.h>
#include <string.h>

typedef struct {
	char id[25];
	char desc[50];
	int header[2];
	int high;
	int low;
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
	int (*parseBinary)();
	void (*createCode)(int id, int unit, int state, int all, int dimlevel);
} protocol;

typedef struct {
	int nr;
	protocol *listeners[255];
} protocols;

protocols protos;

void protocol_register(protocol *proto);
void protocol_unregister(protocol *proto);

#endif