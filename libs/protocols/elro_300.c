/*
	Copyright (C) 2014 CurlyMo

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
#include "elro_300.h"

/**
 * Creates as System message informing the daemon about a received or created message
 *
 * systemcode : integer number, the 32 bit system code
 * unitcode : unit being adressed, integer number
 * state : either 2 (off) or 1 (on)
 * group : if 1 this affects a whole group of devices
 */
static void elro300CreateMessage(unsigned long long systemcode, int unitcode, int state, int group) {
	elro_300->message = json_mkobject();
	//aka address
	json_append_member(elro_300->message, "systemcode", json_mknumber((double)systemcode, 0));
	//toggle all or just one unit
	if(group == 1) {
	    json_append_member(elro_300->message, "all", json_mknumber(group, 0));
	} else {
	    json_append_member(elro_300->message, "unitcode", json_mknumber(unitcode, 0));
	}
	//aka command
	if(state == 1) {
		json_append_member(elro_300->message, "state", json_mkstring("on"));
	}
	else if(state == 2) {
		json_append_member(elro_300->message, "state", json_mkstring("off"));
	}
}

/**
 * This is the main method when reading a received code
 * Decodes the received stream
 *
 */
static void elro300ParseCode(void) {
	int i = 0;
	//utilize the "code" field
	//at this point the code field holds translated "0" and "1" codes from the received pulses
	//this means that we have to combine these ourselves into meaningful values in groups of 2

	for(i = 0; i < elro_300->rawlen/2; i +=1) {
		if(elro_300->code[i*2] != 0) {
			//these are always zero - this is not a valid code
			return;
		}
		elro_300->binary[i] = elro_300->code[(i*2)+1];
	}

	//chunked code now contains "groups of 2" codes for us to handle.
	unsigned long long systemcode = binToDecRevUl(elro_300->binary, 11, 42);
	int groupcode = binToDec(elro_300->binary, 43, 46);
	int groupcode2 = binToDec(elro_300->binary, 49, 50);
	int unitcode = binToDec(elro_300->binary, 51, 56);
	int state = binToDec(elro_300->binary, 47, 48);
	int groupRes = 0;

	if(groupcode == 13 && groupcode2 == 2) {
	    groupRes = 0;
	} else if(groupcode == 3 && groupcode2 == 3) {
	    groupRes = 1;
	} else {
	    return;
	}
	if(state < 1 || state > 2) {
		return;
	} else {
		elro300CreateMessage(systemcode, unitcode, state, groupRes);
	}
}

/**
 * Creates a number of "low" entries (302 302). Note that each entry requires 2 raw positions
 * so e-s should be a multiple of 2
 * s : start position in the raw code (inclusive)
 * e : end position in the raw code (inclusive)
 */
static void elro300CreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		elro_300->raw[i]=(elro_300->plslen->length);
		elro_300->raw[i+1]=(elro_300->plslen->length);
	}
}

/**
 * Creates a number of "high" entries (302 1028). Note that each entry requires 2 raw positions
 * so e-s should be a multiple of 2
 * s : start position in the raw code (inclusive)
 * e : end position in the raw code (inclusive)
 */
static void elro300CreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		elro_300->raw[i]=(elro_300->plslen->length);
		elro_300->raw[i+1]=(elro_300->pulse*elro_300->plslen->length);
	}
}

/**
 * This simply clears the full length of the code to be all "zeroes" (LOW entries)
 */
static void elro300ClearCode(void) {
	elro300CreateLow(0,116);
}

/**
 * Takes the passed number
 * converts it into raw and inserts them into the raw code at the appropriate position
 *
 * systemcode : unsigned integer number, the 32 bit system code
 */
static void elro300CreateSystemCode(unsigned long long systemcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;
	length = decToBinRevUl(systemcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[(length)-i]==1) {
			x=i*2;
			elro300CreateHigh(22+x, 22+x+1);
		}
	}
}

/**
 * Takes the passed number converts it into raw and inserts it into the raw code at the appropriate position
 *
 * unitcode : integer number, id of the unit to control
 */
static void elro300CreateUnitCode(int unitcode) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unitcode, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*2;
			elro300CreateHigh(102+x, 102+x+1);
		}
	}
}

/**
 * Takes the passed number converts it into raw and inserts it into the raw code at the appropriate position
 *
 * state : integer number, state value to set. can be either 1 (on) or 2 (off)
 */
static void elro300CreateState(int state) {
	if(state == 1) {
		elro300CreateHigh(94, 95);
		elro300CreateLow(96, 97);
	}
	else {
    	elro300CreateLow(94, 95);
		elro300CreateHigh(96, 97);
	}
}

/**
 * sets the first group code portions to the appropriate raw values.
 * Fro grouped mode this is the equivalent to 1100 and 11, for non-grouped mode 1011 and 01
 *
 * group : integer value, 1 means grouped enabled, 0 means disabled
 */
static void elro300CreateGroupCode(int group) {
    if(group == 1) {
		elro300CreateHigh(86, 89);
		elro300CreateLow(90, 93);
		elro300CreateHigh(98, 101);
    } else {
		elro300CreateHigh(86, 87);
		elro300CreateLow(88, 89);
		elro300CreateHigh(90, 93);
		elro300CreateLow(98, 99);
		elro300CreateHigh(100, 101);
    }
}

/**
 * Inserts the (as far as is known) fixed message preamble
 * First eleven words are the preamble
 */
static void elro300CreatePreamble(void) {
	elro300CreateHigh(0,3);
	elro300CreateLow(4,9);
	elro300CreateHigh(10,17);
	elro300CreateLow(18,21);
}

/**
 * Inserts the message trailer (one HIGH) into the raw message
 */
static void elro300CreateFooter(void) {
	elro_300->raw[114]=(elro_300->plslen->length);
	elro_300->raw[115]=(PULSE_DIV*elro_300->plslen->length);
}


/**
 * Main method for creating a message based on daemon-passed values in the elro_300 protocol.
 * code : JSON Message containing the received parameters to use for message creation
 *
 * returns : EXIT_SUCCESS or EXIT_FAILURE on obvious occasions
 */
static int elro300CreateCode(JsonNode *code) {
	unsigned long long systemcode = 0;
	int unitcode = -1;
	int group = 0;
	int state = -1;
	double itmp;

	if(json_find_number(code, "systemcode", &itmp) == 0) {
		systemcode = (unsigned long long)itmp;
	}
	if(json_find_number(code, "unitcode", &itmp) == 0) {
		unitcode = (int)round(itmp);
	}
	if(json_find_number(code, "all", &itmp) == 0) {
	    group = 1;
	    //on the reference remote, group toggles always used a unit code of 56.
    	    //for that reason we are enforcing that here
	    unitcode = 56;
	}

	if(json_find_number(code, "off", &itmp) == 0) {
		state=2;
	}
	else if(json_find_number(code, "on", &itmp) == 0) {
		state=1;
	}

	if(systemcode == 0 || unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "elro_300: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 4294967295u || unitcode > 99 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_300: values out of valid range");
	} else {
		elro300CreateMessage(systemcode, unitcode, state, group);
		elro300ClearCode();
		elro300CreatePreamble();
		elro300CreateSystemCode(systemcode);
		elro300CreateGroupCode(group);
		elro300CreateState(state);
		elro300CreateUnitCode(unitcode);
		elro300CreateFooter();
	}
	return EXIT_SUCCESS;
}

/**
 * Outputs help messages directly to the current output target (probably the console)
 */
static void elro300PrintHelp(void) {
	printf("\t -s --systemcode=systemcode\tcontrol a device with this systemcode\n");
	printf("\t -a --all\t\t\ttoggle switching all devices on or off\n");
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
}

/**
 * Main Init method called to init the protocol and register its functions with pilight
 */
#ifndef MODULE
__attribute__((weak))
#endif
void elro300Init(void) {

	protocol_register(&elro_300);
	protocol_set_id(elro_300, "elro_300");
	protocol_device_add(elro_300, "elro_300", "Elro 300 Series Switches");
	protocol_plslen_add(elro_300, 302);
	elro_300->devtype = SWITCH;
	elro_300->hwtype = RF433;
	elro_300->pulse = 4;
	elro_300->rawlen = 116;

	options_add(&elro_300->options, 's', "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,9}|[1-3][0-9]{9}|4([01][0-9]{8}|2([0-8][0-9]{7}|9([0-3][0-9]{6}|4([0-8][0-9]{5}|9([0-5][0-9]{4}|6([0-6][0-9]{3}|7([01][0-9]{2}|2([0-8][0-9]|9[0-4])))))))))$");
	options_add(&elro_300->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]{1,2}$");
	options_add(&elro_300->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_300->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&elro_300->options, 'a', "all", OPTION_OPT_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&elro_300->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");


	elro_300->parseCode=&elro300ParseCode;
	elro_300->createCode=&elro300CreateCode;
	elro_300->printHelp=&elro300PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "elro_300";
	module->version = "1.2";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	elro300Init();
}
#endif
