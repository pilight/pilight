/*
	Copyright (C) 2014 Radoslaw Ejsmont

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "chacon_al.h"

#define CHACON_AL_PULSE_DIV 10

/**
 * Creates as System message informing the daemon about a received or created message
 *
 * unitcode : unit being adressed, integer number
 * state : either 4 (off) or 2 (on) or 1 (panic)
 * group : if 1 this affects a whole group of devices
 * 
 */
static void chaconALCreateMessage(int unitcode, int state, int checksum, int on_cmd, int off_cmd) {
	char buffer[32];
	chacon_al->message = json_mkobject();
	json_append_member(chacon_al->message, "unitcode", json_mknumber(unitcode, 0));
	if(state == 1) {
		json_append_member(chacon_al->message, "state", json_mkstring("panic"));
	}
	else if(state == on_cmd) {
		json_append_member(chacon_al->message, "state", json_mkstring("on"));
	}
	else if(state == off_cmd) {
		json_append_member(chacon_al->message, "state", json_mkstring("off"));
	} else {
		sprintf(buffer, "%d", state);
		json_append_member(chacon_al->message, "state", json_mkstring(buffer));
	}
	json_append_member(chacon_al->message, "checksum", json_mknumber(checksum, 0));
}

/**
 * This is the main method when reading a received code
 * Decodes the received stream
 *
 */
static void chaconALParseCode(void) {
	int i = 0;
	//utilize the "code" field
	//at this point the code field holds translated "0" and "1" codes from the received pulses
	//this means that we have to combine these ourselves into meaningful values in groups of 2
	
	for(i = 0; i < chacon_al->rawlen/2; i+=1) {
		if(chacon_al->code[i*2] == chacon_al->code[i*2+1]) {
			//these should always be different - this is not a valid code
			return;
		}
		chacon_al->binary[i] = chacon_al->code[(i*2)];
	}
	
	//chunked code now contains "groups of 2" codes for us to handle.
	int unitcode = binToDec(chacon_al->binary, 0, 15);
	int state = binToDec(chacon_al->binary, 16, 23);
	int checksum = binToDec(chacon_al->binary, 24, 31);
	
	if(state < 1) {
		return;
	} else {
		chaconALCreateMessage(unitcode, state, checksum, 2, 4);
	}
}

/**
 * Creates a number of "low" entries (500 1500). Note that each entry requires 2 raw positions
 * so e-s should be a multiple of 2
 * s : start position in the raw code (inclusive)
 * e : end position in the raw code (inclusive)
 */
static void chaconALCreateLow(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=2) {
		chacon_al->raw[i]=(chacon_al->plslen->length);
		chacon_al->raw[i+1]=(chacon_al->pulse*chacon_al->plslen->length);
	}
}

/**
 * Creates a number of "high" entries (1500 500). Note that each entry requires 2 raw positions
 * so e-s should be a multiple of 2
 * s : start position in the raw code (inclusive)
 * e : end position in the raw code (inclusive)
 */
static void chaconALCreateHigh(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=2) {
		chacon_al->raw[i]=(chacon_al->pulse*chacon_al->plslen->length);
		chacon_al->raw[i+1]=(chacon_al->plslen->length);
	}
}

/**
 * This simply clears the full length of the code to be all "zeroes" (LOW entries)
 */
static void chaconALClearCode(void) {
	chaconALCreateLow(0,chacon_al->rawlen);
}

/**
 * Takes the passed number converts it into raw and inserts it into the raw code at the appropriate position
 *
 * checksum : integer number, checksum of the message
 * 
 */
static void chaconALCreateChecksum(int checksum) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(checksum, binary) + 1;
	for(i=0;i<length;i++) {
		if(binary[i]==1) {
			x=i*2;
			chaconALCreateHigh((64-length*2)+x, (64-length*2)+x+1);
		}
	}
}

/**
 * Takes the passed number converts it into raw and inserts it into the raw code at the appropriate position
 *
 * unitcode : integer number, id of the unit to control
 * 
 */
static void chaconALCreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unitcode, binary) + 1;
	for(i=0;i<length;i++) {
		if(binary[i]==1) {
			x=i*2;
			chaconALCreateHigh((32-length*2)+x, (32-length*2)+x+1);
		}
	}
}

/**
 * Takes the passed number converts it into raw and inserts it into the raw code at the appropriate position
 *
 * state : integer number, state value to set. can be either 1 (on) or 2 (off)
 * 
 */
static void chaconALCreateState(int state) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(state, binary) + 1;
	for(i=0;i<length;i++) {
		if(binary[i]==1) {
			x=i*2;
			chaconALCreateHigh((48-length*2)+x, (48-length*2)+x+1);
		}
	}
}

/**
 * Inserts the message trailer (one HIGH) into the raw message
 * 
 */
static void chaconALCreateFooter(void) {
	chacon_al->raw[64]=(chacon_al->plslen->length);
	chacon_al->raw[65]=(CHACON_AL_PULSE_DIV*chacon_al->plslen->length);
}


/**
 * Main method for creating a message based on daemon-passed values in the chacon_al protocol.
 * code : JSON Message containing the received parameters to use for message creation
 *
 * returns : EXIT_SUCCESS or EXIT_FAILURE on obvious occasions
 */
static int chaconALCreateCode(JsonNode *code) {
	int unitcode = -1;
	int state = -1;
	int checksum = -1;
	int on_cmd = 2;
	int off_cmd = 4;
	double itmp;
	double jtmp;
	
	if(json_find_number(code, "unitcode", &itmp) == 0) {
		unitcode = (int)round(itmp);
	}
	if(json_find_number(code, "checksum", &itmp) == 0) {
		checksum = (int)round(itmp);
	}
	if(json_find_number(code, "on-command", &itmp) == 0) {
		on_cmd = (int)round(itmp);
	}
	if(json_find_number(code, "off-command", &itmp) == 0) {
		off_cmd = (int)round(itmp);
	}
	
	if(json_find_number(code, "off", &itmp) == 0) {
		if((checksum == -1)&&(json_find_number(code, "off-checksum", &jtmp) == 0)) {
			checksum = (int)round(jtmp);
		}
		state = off_cmd;
	}
	else if(json_find_number(code, "on", &itmp) == 0) {
		if((checksum == -1)&&(json_find_number(code, "on-checksum", &jtmp) == 0)) {
			checksum = (int)round(jtmp);
		}
		state = on_cmd;
	}
	else if(json_find_number(code, "panic", &itmp) == 0) {
		state=1;
	}
	
	if(unitcode == -1 || state == -1 || checksum == -1) {
		logprintf(LOG_ERR, "chacon_al: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		chaconALCreateMessage(unitcode, state, checksum, on_cmd, off_cmd);
		chaconALClearCode();
		chaconALCreateUnitCode(unitcode);
		chaconALCreateState(state);
		chaconALCreateChecksum(checksum);
		chaconALCreateFooter();
	}
	return EXIT_SUCCESS;
}

/**
 * Outputs help messages directly to the current output target (probably the console)
 */
static void chaconALPrintHelp(void) {
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -c --checksum=checksum\t\tuse the specified checksum\n");
	printf("\t -t --on\t\t\tsend an on (arm) signal\n");
	printf("\t -f --off\t\t\tsend an off (disarm) signal\n");
	printf("\t -p --panic\t\t\tsend a panic signal\n");    
}

/**
 * Main Init method called to init the protocol and register its functions with pilight
 */
#ifndef MODULE
__attribute__((weak))
#endif
void chaconALInit(void) {

	protocol_register(&chacon_al);
	protocol_set_id(chacon_al, "chacon_al");
	protocol_device_add(chacon_al, "chacon_al", "Chacon Alarm System");
	protocol_plslen_add(chacon_al, 500);
	chacon_al->devtype = SWITCH;
	chacon_al->hwtype = RF433;
	chacon_al->pulse = 3;
	chacon_al->rawlen = 66;

	options_add(&chacon_al->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5]|[01]{16})$");
	options_add(&chacon_al->options, 'c', "checksum", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(-1|[0-9]{1,2}|1[0-9]{2}|2[0-4][0-9]|25[0-5]|[01]{8})$");
	options_add(&chacon_al->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&chacon_al->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&chacon_al->options, 'a', "panic", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&chacon_al->options, 0, "on-checksum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)-1, "^([0-9]{1,2}|1[0-9]{2}|2[0-4][0-9]|25[0-5]|[01]{8})$");
	options_add(&chacon_al->options, 0, "off-checksum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)-1, "^([0-9]{1,2}|1[0-9]{2}|2[0-4][0-9]|25[0-5]|[01]{8})$");
	options_add(&chacon_al->options, 0, "on-command", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)2, "^([0-9]{1,2}|1[0-9]{2}|2[0-4][0-9]|25[0-5]|[01]{8})$");
	options_add(&chacon_al->options, 0, "off-command", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)4, "^([0-9]{1,2}|1[0-9]{2}|2[0-4][0-9]|25[0-5]|[01]{8})$");
	options_add(&chacon_al->options, 0, "gui-readonly", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	chacon_al->parseCode=&chaconALParseCode;
	chacon_al->createCode=&chaconALCreateCode;
	chacon_al->printHelp=&chaconALPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "chacon_al";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = 84;
}

void init(void) {
	chaconALInit();
}
#endif
