/*
	Copyright (C) 2014 wo_rasp & CurlyMo

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
/*
Change Log:
0.90a	- rollover of rollingcode at 65535
	- rename somfySw... to SomfyScreen..
	- Help Text
	- more meaningful abbreviated letters for options MY (-m) and -PROG (-g)
0.90b	- Footer Bug fixed
0.90c	- Adaption to pulse length in Somfy Documentation
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "somfy.h"

// #define	PULSE_SOMFY_SHORT	640	// Somfy Docu Clock and short pulse duration
#define	PULSE_SOMFY_SHORT	807	// Clock and short pulse duration
#define PULSE_SOMFY_SHORT_L	PULSE_SOMFY_SHORT-154
#define PULSE_SOMFY_SHORT_H	PULSE_SOMFY_SHORT+236
// #define PULSE_SOMFY_LONG	1280	// Somfy Docu long pulse duration
#define PULSE_SOMFY_LONG	1614	// long pulse duration
#define PULSE_SOMFY_LONG_L	PULSE_SOMFY_LONG-158
#define PULSE_SOMFY_LONG_H	PULSE_SOMFY_LONG+492
#define PULSE_SOMFY_START	4650	// Start of payload
#define PULSE_SOMFY_START_L	PULSE_SOMFY_START-400
#define PULSE_SOMFY_START_H	PULSE_SOMFY_START+400
// #define PULSE_SOMFY_FOOTER	1498	// 50932/PULSE_DIV, if last symbol is low plus _SHORT
// #define PULSE_SOMFY_FOOTER	895	// 30415/PULSE_DIV, if last symbol is low plus _SHORT
// #define PULSE_SOMFY_FOOTER	809	// 27500/PULSE_DIV, if last symbol is low plus _SHORT
#define PULSE_SOMFY_FOOTER	800	// 27200/PULSE_DIV, if last symbol is low plus _SHORT
#define PULSE_SOMFY_FOOTER_L	PULSE_SOMFY_FOOTER*PULSE_DIV-215
#define PULSE_SOMFY_FOOTER_H	PULSE_SOMFY_FOOTER*PULSE_DIV+185+PULSE_SOMFY_SHORT
#define PULSE_SOMFY_SYNC	2416	// 4 or 7
// Rawlength definitions
// binlen original protocol: 56
#define BINLEN_SOMFY_PROT	56
#define	MINRAWLEN_SOMFY_PROT	73
#define MAXRAWLEN_SOMFY_PROT	129
// binlen new protocol: 56+24=80
// #define BINLEN_SOMFY_PROT	80
// #define MINRAWLEN_SOMFY_PROT	97
// #define MAXRAWLEN_SOMFY_PROT	175

// to be implemented
#define PULSE_SOMFY_WAKEUP	9415	// Wakeup pulse followed by _WAIT
#define PULSE_SOMFY_WAKEUP_WAIT	89565

int sDataTime = 0;
int sDataLow = 0;

// 01 - MY, 02 - UP, 04 - DOWN, 08 - PROG

typedef struct somfy_settings_t {
	double address;
	double command;
	double rollingcode;
	double rollingkey;
	struct somfy_settings_t *next;
} somfy_settings_t;

static struct somfy_settings_t *somfy_settings = NULL;

static uint8_t somfyScreenCodeChkSum (uint8_t *p_frame) {
	uint8_t cksum, i;
	cksum = 0;
	for (i=0; i < 7; i++) {
 		cksum = cksum ^ *(p_frame+i) ^ (*(p_frame+i) >> 4);
	}
	cksum = cksum & 0xf;
	return cksum;
}

static void somfyScreenCreateMessage(int address, int command, int rollingcode, int rollingkey) {
	somfy->message = json_mkobject();
	json_append_member(somfy->message, "address", json_mknumber(address));
        if(command==1) {
		json_append_member(somfy->message, "state", json_mkstring("my"));
        } else if (command==2) {
		json_append_member(somfy->message, "state", json_mkstring("up"));
	} else if (command==4) {
		json_append_member(somfy->message, "state", json_mkstring("down"));
	} else if (command==8) { 
		json_append_member(somfy->message, "state", json_mkstring("prog"));
	}
	if (rollingcode != -1) {
		json_append_member(somfy->message, "rollingcode", json_mknumber(rollingcode));
	}
	if (rollingkey != -1) {
		json_append_member(somfy->message, "rollingkey", json_mknumber(rollingkey));
	}
}

static void somfyScreenParseCode(void) {
	int i;
	int x = 0, pRaw = 0;
	int protocol_sync = 0;
	int rDataLow = 0, rDataTime = 0;
	int rollingcode = 0, rollingkey = 0;
	uint8_t	dec_frame[7];
	uint8_t frame[7];

	int cksum = 0;
	int key_left = 0, command = 0, address = 0;

// Decode Manchester pulse stream into binary
	pRaw = 0;
	while (pRaw<=somfy->rawlen) {
		switch (protocol_sync) {
			case 0: // Wait for the first SW Sync pulse
			if( (somfy->raw[pRaw] > PULSE_SOMFY_START_L) && (somfy->raw[pRaw] < PULSE_SOMFY_START_H) ) protocol_sync=1;
			rDataLow = 1;
			rDataTime=0;
			x=0;
			break;
			case 1: // We found the Start Sync pulse, this pulse is defined as High
				// Now we got the next pulse, the level is low.
				// We expect either a 604µS sync, than the next 604 pulse indicates a falling edge for 0,
				// or we expect a 1208 pulse, this indicates a rising edge for a logical 1
			rDataLow = -rDataLow;
			rDataTime= rDataTime+somfy->raw[pRaw];
			protocol_sync=2;
			if( rDataTime > PULSE_SOMFY_SHORT_L && rDataTime < PULSE_SOMFY_SHORT_H) {
			// No Data pulse, Sync only
				rDataTime=0;
			} else {
			// 1st pulse, we are now in the middile of the pulse windows
				logprintf(LOG_ERR, "somfy: First pulse detected");
				rDataTime = PULSE_SOMFY_SHORT;
				somfy->binary[x]=1;
				x=x+1;
			}
			break;
			case 2:
			// Determine if we have a rising/falling edge in the middle
			rDataLow = -rDataLow;
			if( somfy->raw[pRaw] > PULSE_SOMFY_FOOTER_L && somfy->raw[pRaw] < PULSE_SOMFY_FOOTER_H) {
				protocol_sync=98; // We should not end up here as binary bits are missing
				logprintf(LOG_ERR, "somfy: Err 21. Incomplete payload");
				break;
			}
			rDataTime= rDataTime+somfy->raw[pRaw];
			// Pulse changes at the pulse duration are neutral
			if ( (rDataTime > PULSE_SOMFY_LONG_L) && (rDataTime < PULSE_SOMFY_LONG_H) ) {
			rDataTime = 0;
			} else {
				if ( rDataLow == 1 ) {
					somfy->binary[x]=0;
				} else {
					somfy->binary[x]=1;
				}
				x=x+1;
				rDataTime = PULSE_SOMFY_SHORT;
				if ( x >= somfy->binlen) protocol_sync = 3; // We got all bits
			}
			break;
			case 3:
			// We got all bits, immediately followed by the footer pulse
			if ( (rDataTime > PULSE_SOMFY_LONG_L) && (rDataTime < PULSE_SOMFY_LONG_H) ) protocol_sync=99;
			case 98:
			// We decoded a footer pulse without decoding the correct number of binary bits
			logprintf(LOG_ERR, "somfy: Err 98. EOF");
			case 99:
			// We have reached the end of processing raw data
			default:
			break;
		}
	pRaw++;
	if (protocol_sync > 97) break;
	}

	frame[0] = (uint8_t) (binToDecRev(somfy->binary, 0, 3) << 4);		// Key
	frame[0] = frame[0] | (uint8_t) binToDecRev(somfy->binary, 4, 7);	// Key increment counter
	frame[1] = (uint8_t) (binToDecRev(somfy->binary, 8,11) << 4);		// command
	frame[1] = frame[1] | (uint8_t) binToDecRev(somfy->binary, 12, 15);	// cks;
	frame[2] = (uint8_t) binToDecRev(somfy->binary, 16,23);
	frame[3] = (uint8_t) binToDecRev(somfy->binary, 24,31);
	frame[4] = (uint8_t) binToDecRev(somfy->binary, 32,39);
	frame[5] = (uint8_t) binToDecRev(somfy->binary, 40,47);
	frame[6] = (uint8_t) binToDecRev(somfy->binary, 48,55);

	dec_frame[0]=frame[0];
	for (i=1; i < 7; i++) {
		dec_frame[i] = frame[i] ^ frame[i-1];
	}

	cksum = somfyScreenCodeChkSum (dec_frame);

	key_left = dec_frame[0] >> 4;
	rollingkey = dec_frame[0] & 0xf;
	command = dec_frame[1] >> 4;

	rollingcode = dec_frame[2]*256 + dec_frame[3];
	address = dec_frame[4]+dec_frame[5]*256+dec_frame[6]*65536;
	struct somfy_settings_t *tmp = somfy_settings;
	while(tmp) {
		if(fabs(tmp->address-address) < EPSILON ) {
			tmp->command = (double) command;
			tmp->rollingcode = (double) rollingcode;
			tmp->rollingkey = (double) rollingkey;
			break;
		}
	tmp = tmp->next;
	}

	if ( ( key_left == 10 && 0 == cksum)) {
		somfyScreenCreateMessage(address, command, rollingcode, rollingkey);
	} else {
		if(log_level_get() >= LOG_DEBUG) {
			logprintf(LOG_ERR, "somfyScreen Parsecode Error: Header or Checksum Error");
		}
	}
}

static int somfyCheckValues(struct JsonNode *jvalues) {
struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct somfy_settings_t *snode = NULL;
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;
		double address = -1;
		int match = 0;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "address") == 0) {
					address = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}

		struct somfy_settings_t *tmp = somfy_settings;
		while(tmp) {
			if(fabs(tmp->address-address) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}
		if(!match) {
			if(!(snode = malloc(sizeof(struct somfy_settings_t)))) {
				logprintf(LOG_ERR, "somfy: out of memory");
				exit(EXIT_FAILURE);
			}
			snode->address = address;

			json_find_number(jvalues, "command", &snode->command);
			json_find_number(jvalues, "rollingcode", &snode->rollingcode);
			json_find_number(jvalues, "rollingkey", &snode->rollingkey);
			snode->rollingcode = (double) ((int)(snode->rollingcode) & 0xffff);
			snode->rollingkey = (double) ((int)(snode->rollingkey) & 0xf);
			snode->next = somfy_settings;
			somfy_settings = snode;
		}
	}
	return 0;
}

static void somfyScreenGC(void) {
	struct somfy_settings_t *tmp = NULL;
	while(somfy_settings) {
		tmp = somfy_settings;
		somfy_settings = somfy_settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&somfy_settings);
}


static void somfyScreenCreatePulse(int e, int pulse) {
	int i,k;
	k = somfy->rawlen+e;
	if (k<=somfy->maxrawlen) {
		for(i=somfy->rawlen;i<k;i++) {
			somfy->raw[i] = pulse;
		}
		somfy->rawlen=k;
	}
}

static void somfyScreenCreateFooter(void) {
	if (sDataLow) {
		somfyScreenCreatePulse(1,PULSE_SOMFY_SHORT);
		somfyScreenCreatePulse(1,PULSE_SOMFY_FOOTER*PULSE_DIV);
	} else {
		somfyScreenCreatePulse(1,PULSE_SOMFY_FOOTER*PULSE_DIV+PULSE_SOMFY_SHORT);
	}
}

static void somfyScreenCreateHeader(void) {
	somfy->rawlen=0;				// There is plenty of space in the structure:
	sDataTime = 0;
	sDataLow = 1;
	somfyScreenCreatePulse(14,PULSE_SOMFY_SYNC);	// seven SYNC pulses
	somfyScreenCreatePulse(1,PULSE_SOMFY_START);
	sDataTime = -PULSE_SOMFY_SHORT;			// Mark 1st pulse to be transmitted
}

static void somfyScreenCreateZero(void) {
	if (sDataTime < 0) {
		somfy->raw[somfy->rawlen++] = PULSE_SOMFY_SHORT; // the 1st pulse is different
		somfy->raw[somfy->rawlen++] = PULSE_SOMFY_SHORT;
		sDataTime = PULSE_SOMFY_SHORT;
		sDataLow = 0;
	} else {
		if ( sDataLow == 0 ) {
			somfy->raw[somfy->rawlen++] = PULSE_SOMFY_SHORT;
			somfy->raw[somfy->rawlen++] = PULSE_SOMFY_SHORT;
		} else {
			somfy->raw[somfy->rawlen++] = PULSE_SOMFY_LONG;
			sDataLow = 0;
		}
	}
}

static void somfyScreenCreateOne(void) {
	if (sDataTime < 0) {
		somfy->raw[somfy->rawlen++] = PULSE_SOMFY_LONG; // the 1st pulse is different
		sDataTime = PULSE_SOMFY_SHORT;
		sDataLow = 1;
	} else {
		if ( sDataLow == 0 ) {
			somfy->raw[somfy->rawlen++] = PULSE_SOMFY_LONG;
			sDataLow = 1;
		} else {
			somfy->raw[somfy->rawlen++] = PULSE_SOMFY_SHORT;
			somfy->raw[somfy->rawlen++] = PULSE_SOMFY_SHORT;
		}
	}
}

static void somfyScreenCreateData(int iParam, int iLength) {
	int binary[255];
	int i, length, emptylength;

	length = decToBin(iParam, binary);
// Create leading empty zero pulses and data pulses
	emptylength=iLength-length-1;
	for(i=0;i<emptylength;i++) {
		somfyScreenCreateZero();
	}
	for(i=0;i<=length;i++) {
		if(binary[i] == 1) {
			somfyScreenCreateOne();
		} else {
			somfyScreenCreateZero();
		}
	}
}

static int somfyScreenCreateCode(JsonNode *code) {
	int key_left = 160;
	int rollingkey = -1;
	int command = -1;
	int rollingcode = -1;
	int address = -1;
	double itmp = -1;
	uint8_t	dec_frame[7], frame[7], i;

	if(json_find_number(code, "address", &itmp) == 0)	address = (int)round(itmp);
	if(json_find_number(code, "rollingcode", &itmp) == 0)	rollingcode = (int)round(itmp);
	if(json_find_number(code, "rollingkey", &itmp) == 0)	rollingkey = (int)round(itmp);

	if(json_find_number(code, "my", &itmp) == 0) {
		command=1;
	} else if (json_find_number(code, "up", &itmp) == 0) {
		command=2;
	} else if (json_find_number(code, "down", &itmp) == 0) {
		command=4;
	} else if (json_find_number(code, "prog", &itmp) == 0) {
		command=8;
	}

	if( address==-1 || command==-1 ) {
		logprintf(LOG_ERR, "insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(address < 0 || address > 16777216) {
		logprintf(LOG_ERR, "invalid address range");
		return EXIT_FAILURE;
	} else if(command > 15 || command < 0) {
		logprintf(LOG_ERR, "invalid command code");
		return EXIT_FAILURE;
	} else {
		if(rollingcode == -1) {
			struct somfy_settings_t *tmp = somfy_settings;
			while(tmp) {
				if(fabs(tmp->address-address) < EPSILON ) {
					tmp->command = (double) command;
					tmp->rollingcode = (double) (((int)(tmp->rollingcode) + 1) & 0xffff);
					rollingcode = (int) tmp->rollingcode;
			break;
				}
			tmp = tmp->next;
			}
		}
		if(rollingkey == -1) {
			struct somfy_settings_t *tmp = somfy_settings;
			while(tmp) {
				if(fabs(tmp->address-address) < EPSILON ) {
					tmp->command = (double) command;
					tmp->rollingkey = (double) (((int)(tmp->rollingkey) + 1) & 0xf);
					rollingkey = (int) tmp->rollingkey;
				break;
				}
			tmp = tmp->next;
			}
		}
		somfyScreenCreateMessage(address, command, rollingcode, rollingkey);
		somfyScreenCreateHeader();

		dec_frame[0] = (uint8_t) (key_left | (rollingkey & 0xf));
		dec_frame[1] = (uint8_t) (command << 4);
		dec_frame[2] = (uint8_t) ((rollingcode >> 8) & 0xff);
		dec_frame[3] = (uint8_t) (rollingcode & 0xff);
		dec_frame[4] = (uint8_t) (address & 0xff);
		dec_frame[5] = (uint8_t) ((address >> 8) & 0xff);
		dec_frame[6] = (uint8_t) ((address >> 16) & 0xff);

		int cksum = somfyScreenCodeChkSum (dec_frame);
		dec_frame[1] = dec_frame[1] | (uint8_t) cksum;

		frame[0] = dec_frame[0];
		somfyScreenCreateData (frame[0], 8);
		for (i=1; i < 7; i++) {
			frame[i] = dec_frame[i] ^ frame[i-1];
			somfyScreenCreateData (frame[i], 8);
		}

		somfyScreenCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void somfyScreenPrintHelp(void) {
	printf("\t -a --address=address\t\t\tAddress of Remote Controller\n");
	printf("\t -t --up\t\t\tCommand UP\n");
	printf("\t -f --down\t\t\tCommand DOWN\n");
	printf("\t -m --my\t\t\tCommand MY\n");
	printf("\t -g --prog\t\t\tCommand PROG\n");
	printf("\t -c --rollingcode=rollingcode\t\t\tset rollingcode\n");
	printf("\t -k --rollingkey=rollingkey\t\t\tset rollingkey\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void somfyScreenInit(void) {
	protocol_register(&somfy);
	protocol_set_id(somfy, "somfy");
	protocol_device_add(somfy, "somfy", "Telis-1");
	protocol_plslen_add(somfy, PULSE_SOMFY_FOOTER); // Footer length ratio: (30415/PULSE_DIV)
	somfy->devtype = SCREEN;
	somfy->hwtype = RF433;
	somfy->pulse = 2;	// LONG pulse
	somfy->rawlen = 128;	// dynamically depending on 1st or repeated frame and binary value of 1st bit
	somfy->minrawlen = MINRAWLEN_SOMFY_PROT;
	somfy->maxrawlen = MAXRAWLEN_SOMFY_PROT;
	// Hardware Sync: 2416.2416 or 2416.2416.2416.2416.2416.2416.2416
	// 4550.604.604 1st Data bit=0 or 4550.1208 1st Data bit=1
	// 55 data bits: Byte structure command/rollingkey, ctrl/cks, rolling code[2] bE, address[3]lE
	// 30415 footer
	somfy->binlen = BINLEN_SOMFY_PROT;
	options_add(&somfy->options, 'a', "address", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,6}|[1-5][0-9]{6}|16[0-6][0-9]{5}|167[0-6][0-9]{4}|1677[0-6][0-9]{3}| 16777[0-1][0-9]{2}|1677720[0-9]|1677721[0-6])$");
	options_add(&somfy->options, 'c', "rollingcode", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
	options_add(&somfy->options, 'k', "rollingkey", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^([0-9]|1[0-5])$");
	options_add(&somfy->options, 't', "up", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy->options, 'f', "down", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy->options, 'm', "my", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy->options, 'g', "prog", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&somfy->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	somfy->parseCode=&somfyScreenParseCode;
	somfy->createCode=&somfyScreenCreateCode;
        somfy->checkValues=&somfyCheckValues;
	somfy->printHelp=&somfyScreenPrintHelp;
        somfy->gc=&somfyScreenGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name =  "somfy";
	module->version =  "0.90c";
	module->reqversion =  "6.0";
	module->reqcommit =  NULL;
}

void init(void) {
	somfyScreenInit();
}
#endif
