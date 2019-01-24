/*
	Copyright (C) 2019 CurlyMo & S. Seegel

	This file is part of pilight.

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "fs20.h"

#define RAW_LENGTH			118
#define SHORT_PULSE_LENGTH	400
#define LONG_PULSE_LENGTH	600
#define PULSE_HYST			100
#define PAUSE_LENGTH		9300

static uint8_t sum = 0;

static int validate(void) {
	int i = 0;
	
	if(fs20->rawlen != RAW_LENGTH)
		return -1;
	
	for(i=0; i<(fs20->rawlen-1); i++) {
		if((fs20->raw[i] < (SHORT_PULSE_LENGTH - PULSE_HYST)) ||
			(fs20->raw[i] > (LONG_PULSE_LENGTH + PULSE_HYST)))
			return -1;
	}
	
	if ((fs20->raw[i] > (PAUSE_LENGTH + 5000)) || (fs20->raw[i] < (PAUSE_LENGTH - 5000)))
		return -1;

	return 0;
}

static void createMessage(int housecode, int address, int command) {
	fs20->message = json_mkobject();
	json_append_member(fs20->message, "housecode", json_mknumber(housecode, 0));
	json_append_member(fs20->message, "address", json_mknumber(address, 0));
	if(command != 0)
		json_append_member(fs20->message, "state", json_mkstring("on"));
	else
		json_append_member(fs20->message, "state", json_mkstring("off"));
}

static int readBits(int pos, int len) {
	int result = 0;
	int i = 0;
	
	for(i=pos; i<pos+len*2; i+=2) {
		uint8_t bits = 0;
		
		if(fs20->raw[i] > ((SHORT_PULSE_LENGTH + LONG_PULSE_LENGTH) / 2))
			bits |= 0x01;
		
		if(fs20->raw[i+1] > ((SHORT_PULSE_LENGTH + LONG_PULSE_LENGTH) / 2))
			bits |= 0x02;
		
		result <<= 1;
		switch (bits) {
		case 0x00:
			break;
		case 0x03:
			result |= 1;
			break;
		default:
			return -1;
		}
	}
	
	return result;
}

static int readByte(int pos) {
	int byte = 0;
	int par = 0;
	int temp = 0;
	
	byte = readBits(pos, 9);
	if (byte == -1)
		return -1;
	
	temp = byte;
	while (temp != 0) {
		temp &= temp - 1;
		par = !par;
	}
	
	if (par != 0)
		return -1;
	
	return byte >> 1;
}

static void parseCode(void) {
	int temp = 0;
	uint8_t sum = 6;
	int sync = 0;
	int housecode = 0;
	int address = 0;
	int command = 0;
	
	sync = readBits(0, 13);
	if (sync != 1)
		return;
	
	temp = readByte(26);
	if (temp == -1)
		return;
	sum += temp;
	housecode = temp << 8;
	
	temp = readByte(44);
	if (temp == -1)
		return;
	sum += temp;
	housecode |= temp;
	
	address = readByte(62);
	if (address == -1)
		return;
	sum += address;
	
	command = readByte(80);
	if (command == -1)
		return;
	sum += command;
	
	if (readByte(98) != (int) sum)
		return;
	
	createMessage(housecode, address, command);
}

static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		fs20->raw[i]=SHORT_PULSE_LENGTH;
		fs20->raw[i+1]=SHORT_PULSE_LENGTH;
	}
}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		fs20->raw[i]=LONG_PULSE_LENGTH;
		fs20->raw[i+1]=LONG_PULSE_LENGTH;
	}
}

static int createSync(void) {
	createLow(0, 24);
	createHigh(24, 26);
	return 26;
}

static int createByte(int byte, int p) {
	uint8_t mask = 0x80;
	int par = 0;
	
	while(mask != 0) {
		if((byte & mask) != 0) {
			createHigh(p, p + 2);
			par = !par;
		}
		else {
			createLow(p, p + 2);
		}
		
		mask >>= 1;
		p += 2;
	}
	
	if(par != 0) {
		createHigh(p, p + 2);
	}
	else {
		createLow(p, p + 2);
	}
	
	sum += byte;
	return p + 2;
}

static int createHouseCode(int housecode, int p) {
	p = createByte(housecode >> 8, p);
	p = createByte(housecode & 0xFF, p);
	return p;
}

static int createFooter(int p) {
	fs20->raw[p++]=SHORT_PULSE_LENGTH;
	fs20->raw[p++]=PAUSE_LENGTH;
	return p;
}

static int createCode(struct JsonNode *code) {
	int housecode = -1;
	int address = -1;
	int command = -1;
	double itmp = -1;
	int p = 0;

	if(json_find_number(code, "housecode", &itmp) == 0)
		housecode = (int)round(itmp);
	if(json_find_number(code, "address", &itmp) == 0)
		address = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		command=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		command=17;

	if(housecode == -1 || address == -1 || command == -1) {
		logprintf(LOG_ERR, "fs20: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(housecode > 65353 || housecode < 0) {
		logprintf(LOG_ERR, "fs20: invalid housecode range");
		return EXIT_FAILURE;
	} else if(address > 255 || address < 0) {
		logprintf(LOG_ERR, "fs20: invalid address range");
		return EXIT_FAILURE;
	} else {
		createMessage(housecode, address, command);
		
		sum = 6;
		p = createSync();
		p = createHouseCode(housecode, p);
		p = createByte(address, p);
		p = createByte(command, p); //command
		p = createByte(sum, p); //checksum
		p = createFooter(p); //EOT
		
		fs20->rawlen = p;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -c --housecode=housecode\t\t\tcontrol a device with this housecode\n");
	printf("\t -a --address=address\t\t\tcontrol a device with this address\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void fs20Init(void) {
	protocol_register(&fs20);
	protocol_set_id(fs20, "fs20");
	protocol_device_add(fs20, "fs20", "fs20");
	fs20->devtype = SWITCH;
	fs20->hwtype = RF868;
	fs20->minrawlen = 100;
	fs20->maxrawlen = 150;
	fs20->maxgaplen = 15000;
	fs20->mingaplen = 5000;

	options_add(&fs20->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&fs20->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&fs20->options, "c", "housecode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]+$");
	options_add(&fs20->options, "a", "address", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]+$");

	fs20->parseCode=&parseCode;
	fs20->createCode=&createCode;
	fs20->printHelp=&printHelp;
	fs20->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "fs20";
	module->version = "1.0";
	module->reqversion = "8.0";
	module->reqcommit = "1";
}

void init(void) {
	fs20Init();
}
#endif
