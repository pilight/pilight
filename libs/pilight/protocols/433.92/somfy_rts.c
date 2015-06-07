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
0.90a	- special version of daemon.c (increased window for footer length variation +/-25 instead +/-5)
	- rollover of rollingcode at 65535
	- rename somfySw... to SomfyScreen..
	- Help Text
	- more meaningful abbreviated letters for options MY (-m) and -PROG (-g)
0.90b	- Footer Bug fixed
0.90c	- Adaption of pulse length in Somfy Documentation, additions to debug log
	- decode of all 16 commands on receive
0.90d	- Bugfixing, Adaption of parameters, additional error checking, more log information
0.90e	- Cleanup of pulse values
	  Handling of all 15 commands using the new optional command "-n ##"
	  ## is the decimal command equivalent (range 0 to 15)
	  Hooks for handling of new generation data frame
0.91	- modified Rollingkey and Footer handling
0.91a	- more debug info
0.92a	- 150121 adaption to nightlies
0.92c	- 150316 focus on rts protocol
0.93	- 150430 port to pilight 7.0
0.94  - Bugfixing
0.94b - Bugfixing GAP Length
0.99a21a - 9ab0e4f - Step a21a - Add Bufferlength checks (pMaxBin, pMaxRaw)
0.99a21b - 9ab0e4f - Step a21b - Fix the buffer overflow loop bug
0.99a21c - 9ab0e4f - Step a21c - modify the wakeup pulses
0.99a21d - 9ab0e4f - Step a21d - Add 2nd wakeup pulse.  0-10750/17750  1-9450/89565
0.99a21e - 9ab0e4f - Step a21e - Change # of repetitive pulsestreams from default 10 to 4
0.99a21e - 9ab0e4f - Step a21f - Add 2nd Footer pulse 32500
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
// #include "../../core/hardware.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "somfy_rts.h"

#define PULSE_SOMFY_SHORT	640	// Somfy Docu Clock and short pulse duration
//#define	PULSE_SOMFY_SHORT	807	// Clock and short pulse duration
#define PULSE_SOMFY_SYNC	PULSE_SOMFY_SHORT*4	// 2 pairs 1st-, 7 pairs Repeated-Pulses
#define PULSE_SOMFY_SYNC_L	PULSE_SOMFY_SYNC-256
#define PULSE_SOMFY_SYNC_H	PULSE_SOMFY_SYNC+256
#define PULSE_SOMFY_SHORT_L	PULSE_SOMFY_SHORT-192
#define PULSE_SOMFY_SHORT_H	PULSE_SOMFY_SHORT+192
#define PULSE_SOMFY_LONG	1280	// Somfy Docu long pulse duration
// #define PULSE_SOMFY_LONG	1614	// long pulse duration
#define PULSE_SOMFY_LONG_L	PULSE_SOMFY_LONG-384
#define PULSE_SOMFY_LONG_H	PULSE_SOMFY_LONG+384
#define PULSE_SOMFY_START	4760	// Start of payload (140*34)
#define PULSE_SOMFY_START_L	PULSE_SOMFY_START-768
#define PULSE_SOMFY_START_H	5100	// Max data pulse handled by pilight is 5100
// The footer detection also depends on pilight-daemon, the standard footer variation is -/+ 170 (5*34)
// The value in daemon.c packaged with this distribution is currently set to -/+ 850 (25*34)
// #define PULSE_SOMFY_FOOTER	1498	// 50932/PULSE_DIV, if last symbol is low plus _SHORT
// #define PULSE_SOMFY_FOOTER	895	// 30430/PULSE_DIV, if last symbol is low plus _SHORT
#define PULSE_SOMFY_FOOTER	809	// 27506/PULSE_DIV, if last symbol is low plus _SHORT
#define PULSE_SOMFY_FOOTER_1	955	// 32500/PULSE_DIV, if last symbol is low plus _SHORT
// #define PULSE_SOMFY_FOOTER	800	// 27200/PULSE_DIV, if last symbol is low plus _SHORT
// #define PULSE_SOMFY_FOOTER	221	// 7531/PULSE_DIV, if last symbol is low plus _SHORT
#define PULSE_SOMFY_FOOTER_L	(PULSE_SOMFY_FOOTER-25)*PULSE_DIV
#define PULSE_SOMFY_FOOTER_H	(PULSE_SOMFY_FOOTER+25)*PULSE_DIV+PULSE_SOMFY_SHORT_H
#define PULSE_SOMFY_FOOTER_L_1	(PULSE_SOMFY_FOOTER_1-5)*PULSE_DIV
#define PULSE_SOMFY_FOOTER_H_1	(PULSE_SOMFY_FOOTER_1+5)*PULSE_DIV+PULSE_SOMFY_SHORT_H
// Bin / Rawlength definitions
// binlen Classic protocol: 56 - new_gen protocol: 56+24=80
// rawlen Classic protocol: 73 .. 129  - new_gen protocol: 97 .. 177
#define BIN_ARRAY_CLASSIC	14
#define BIN_ARRAY_SOMFY_PROT	14
#define BINLEN_SOMFY_CLASSIC	BIN_ARRAY_CLASSIC*8
#define BINLEN_SOMFY_PROT	BIN_ARRAY_SOMFY_PROT*8
#define RAWLEN_SOMFY_PROT	242
#define MINRAWLEN_SOMFY_PROT	63	// all Data bit toogle classic: 4+1+56+2
#define MAXRAWLEN_SOMFY_PROT	241	// all Data bit equal new genration: 14+1+2*56+2*56+2=241 241-4-4=233
//
#define VALUE_LEN_WAKEUP	2	// Wakeup sequence length
#define PULSE_SOMFY_WAKEUP	10750	// Wakeup pulse followed by _WAIT
#define PULSE_SOMFY_WAKEUP_WAIT	17750
#define PULSE_SOMFY_WAKEUP_1	9415	// Wakeup pulse followed by _WAIT
#define PULSE_SOMFY_WAKEUP_WAIT_1	89565
#define RAW_LENGTH RAWLEN_SOMFY_PROT
#define BIN_LENGTH BINLEN_SOMFY_PROT
#define LEARN_REPEATS 4
#define NORMAL_REPEATS 4

int sDataTime = 0;
int sDataLow = 0;
int preAmb_wakeup [VALUE_LEN_WAKEUP+1] = {VALUE_LEN_WAKEUP, PULSE_SOMFY_WAKEUP, PULSE_SOMFY_WAKEUP_WAIT};
int preAmb_wakeup_1 [VALUE_LEN_WAKEUP+1] = {VALUE_LEN_WAKEUP, PULSE_SOMFY_WAKEUP_1, PULSE_SOMFY_WAKEUP_WAIT_1};
int wakeup_type = 0;
int pMaxbin = 0;
int pMaxraw = 0;
// 01 - MY, 02 - UP, 04 - DOWN, 08 - PROG

typedef struct settings_t {
	double address;
	double command;
	double rollingcode;
	double rollingkey;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

// Check for expected length, Footer value and SYNC
static int validate(void) {
	int i;
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	logprintf(LOG_DEBUG, "somfy_rts validate: rawlen: %d footer: %d SYNC: %d", somfy_rts->rawlen, somfy_rts->raw[somfy_rts->rawlen-1], somfy_rts->raw[2]);
	if(log_level_get() >= LOG_DEBUG) {
		for (i=0; i < somfy_rts->rawlen; i++) {
			printf("%d ",somfy_rts->raw[i]);
		}
		printf("-#: %d\n",somfy_rts->rawlen);
	}

	if((somfy_rts->rawlen > MINRAWLEN_SOMFY_PROT) &&
		(somfy_rts->rawlen < MAXRAWLEN_SOMFY_PROT)) {
		if(((somfy_rts->raw[somfy_rts->rawlen-1] > PULSE_SOMFY_FOOTER_L) &&
				(somfy_rts->raw[somfy_rts->rawlen-1] < PULSE_SOMFY_FOOTER_H)) ||
			((somfy_rts->raw[somfy_rts->rawlen-1] > PULSE_SOMFY_FOOTER_L_1) &&
				(somfy_rts->raw[somfy_rts->rawlen-1] < PULSE_SOMFY_FOOTER_H_1))) {
			if((somfy_rts->raw[2] > PULSE_SOMFY_SYNC_L) &&
				(somfy_rts->raw[2] < PULSE_SOMFY_SYNC_H)) {
					return 0;
			}
		}
	}
	return -1;
}

static uint8_t codeChkSum (uint8_t *p_frame) {
	uint8_t cksum, i;
	cksum = 0;
	for (i=0; i < BIN_ARRAY_CLASSIC; i++) {
		cksum = cksum ^ *(p_frame+i) ^ (*(p_frame+i) >> 4);
	}
	cksum = cksum & 0xf;
	return cksum;
}

static void createMessage(int address, int command, int rollingcode, int rollingkey) {
	char str [9];
	somfy_rts->message = json_mkobject();
	json_append_member(somfy_rts->message, "address", json_mknumber(address,0));
	if(command==1) {
		json_append_member(somfy_rts->message, "state", json_mkstring("my"));
	} else if (command==2) {
		json_append_member(somfy_rts->message, "state", json_mkstring("up"));
	} else if (command==3) {
		json_append_member(somfy_rts->message, "state", json_mkstring("my+up"));
	} else if (command==4) {
		json_append_member(somfy_rts->message, "state", json_mkstring("down"));
	} else if (command==5) {
		json_append_member(somfy_rts->message, "state", json_mkstring("my+down"));
	} else if (command==6) {
		json_append_member(somfy_rts->message, "state", json_mkstring("up+down"));
	} else if (command==8) {
		json_append_member(somfy_rts->message, "state", json_mkstring("prog"));
	} else if (command==9) {
		json_append_member(somfy_rts->message, "state", json_mkstring("sun+flag"));
	} else if (command==10) {
		json_append_member(somfy_rts->message, "state", json_mkstring("flag"));
	} else {
		sprintf(str, "%d", command);
		json_append_member(somfy_rts->message, "state", json_mkstring(str));
	}
	if (rollingcode != -1) {
		json_append_member(somfy_rts->message, "rollingcode", json_mknumber(rollingcode,0));
	}
	if (rollingkey != -1) {
		json_append_member(somfy_rts->message, "rollingkey", json_mknumber(rollingkey,0));
	}
	somfy_rts->txrpt = NORMAL_REPEATS;
}

static void parseCode(void) {
	int i, x;
	int pBin = 0, pRaw = 0;
	int protocol_sync = 0, rDataLow = 0, rDataTime = 0;
	int rollingcode = 0, rollingkey = 0, binary[MAXPULSESTREAMLENGTH];
	uint8_t dec_frame[BIN_ARRAY_SOMFY_PROT] = { 0 };
	uint8_t frame[BIN_ARRAY_SOMFY_PROT] = { 0 };

	int cksum = 0, key_left = 0, command = 0, address = 0;

	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);
	// Decode Manchester pulse stream into binary
	pRaw = 0;
	for  (i=0;i<BINLEN_SOMFY_PROT;i++) {
		binary[i]=0;
	}

	while (pRaw<=somfy_rts->rawlen) {
		switch (protocol_sync) {
			case 0: // Wait for the first SW Sync pulse
			if( (somfy_rts->raw[pRaw] > PULSE_SOMFY_START_L) && (somfy_rts->raw[pRaw] < PULSE_SOMFY_START_H) ) {
				protocol_sync=1;
				rDataLow = 1;
				rDataTime=0;
				pBin=0;
			}
			break;
			case 1:
				// We found the Start Sync pulse, this pulse is defined as High
				// Now we got the next pulse, the level is low.
				// We expect either a 604µS sync or a 1208mS pulse
				// A 604 pulse indicating a falling edge for a logical 0, or
				// A 1208 pulse indicating a rising edge for a logical 1
			rDataLow = -rDataLow;
			rDataTime= rDataTime+somfy_rts->raw[pRaw];
			protocol_sync=2;
			if( rDataTime > PULSE_SOMFY_SHORT_L && rDataTime < PULSE_SOMFY_SHORT_H) {
			// No Data pulse, Sync only
				rDataTime=0;
				logprintf(LOG_DEBUG, "somfy_rts: First pulse: Sync detected");
			} else if  (rDataTime > PULSE_SOMFY_LONG_L && rDataTime < PULSE_SOMFY_LONG_H) {
			// if it is 1st pulse, we are now in the middile of the pulse windows
				logprintf(LOG_DEBUG, "somfy_rts: First pulse: Data detected");
				rDataTime = PULSE_SOMFY_SHORT;
				binary[pBin]=1;
				pBin=pBin+1;
				if(pBin>pMaxbin)pMaxbin = pBin;
			} else {
			// if it is neither a short nor a long pulse, we try to find another SYNC pulse
				protocol_sync=0;
				logprintf(LOG_DEBUG, "somfy_rts: First pulse: Not followed by valid data pulse");
			}
			break;
			case 2:
			// Determine if we have a rising/falling edge in the middle
			rDataLow = -rDataLow;
			if( somfy_rts->raw[pRaw] > PULSE_SOMFY_FOOTER_L && somfy_rts->raw[pRaw] < PULSE_SOMFY_FOOTER_H) {
				if (pBin==56) {
					protocol_sync=4;	// We received all 56 classic frame bits
					logprintf(LOG_DEBUG, "somfy_rts: prot 2a");
				} else {
					if (pBin==56) {
						protocol_sync=3;	// We received all 56 classic frame bits
						logprintf(LOG_DEBUG, "somfy_rts: prot 2b");
					} else {
						protocol_sync=96; // We should never end up here as binary bits are missing
						logprintf(LOG_DEBUG, "somfy_rts: Err 21. Payload incomplete, bin:%d raw:%d",pBin,pRaw);
					}
				}
				break;
			}
			rDataTime=rDataTime+somfy_rts->raw[pRaw];
			// Pulse changes at the pulse duration are neutral
			if ((rDataTime > PULSE_SOMFY_LONG_L) && (rDataTime < PULSE_SOMFY_LONG_H)) {
			rDataTime=0;
			} else {
				if (rDataLow==1) {
					binary[pBin]=0;
				} else {
					binary[pBin]=1;
				}
				pBin=pBin+1;
				if(pBin>pMaxbin)pMaxbin=pBin;
				rDataTime = PULSE_SOMFY_SHORT;
				if (pBin >= BINLEN_SOMFY_CLASSIC) {
					protocol_sync = 3;	// We got all bits for classic data frame
					logprintf(LOG_DEBUG, "somfy_rts: prot 3");
				}
																				}
			break;
			case 3:
			// Determine if we have a rising/falling edge in the middle
			rDataLow = -rDataLow;
			if(somfy_rts->raw[pRaw] > PULSE_SOMFY_FOOTER_L && somfy_rts->raw[pRaw] < PULSE_SOMFY_FOOTER_H) {
				if (pBin==112) {
					protocol_sync=4;	// We received (56 classic) + (56 extended) frame bits
					logprintf(LOG_DEBUG, "somfy_rts: prot 4");
				} else {
					protocol_sync=97;	// We have to check why we end up here
					logprintf(LOG_DEBUG, "somfy_rts: Err 31. Payload analysis required.");
				}
				break;
			}
			rDataTime= rDataTime+somfy_rts->raw[pRaw];
			// Pulse changes at the pulse duration are neutral
			if ( (rDataTime > PULSE_SOMFY_LONG_L) && (rDataTime < PULSE_SOMFY_LONG_H) ) {
			rDataTime = 0;
			} else {
				if (rDataLow == 1) {
					binary[pBin]=0;
				} else {
					binary[pBin]=1;
				}
				pBin=pBin+1;
				if(pBin>pMaxbin)pMaxbin=pBin;
				rDataTime = PULSE_SOMFY_SHORT;
				if (pBin >= BINLEN_SOMFY_CLASSIC) {
					protocol_sync = 4;	// We got all bits for classic data frame
				}
			}
			break;
			case 4:
			// We decoded the number of bis for classic data and new generation, check for footer pulse
			logprintf(LOG_DEBUG, "somfy_rts: End of new generation data. pulse: %d - pRaw: %d - bin: %d", somfy_rts->raw[pRaw], pRaw, pBin);
			if (((somfy_rts->raw[pRaw] > PULSE_SOMFY_FOOTER_L) && (somfy_rts->raw[pRaw] < PULSE_SOMFY_FOOTER_H)) ||
				 ((somfy_rts->raw[pRaw] > PULSE_SOMFY_FOOTER_L_1) && (somfy_rts->raw[pRaw] < PULSE_SOMFY_FOOTER_H_1)) ){
				protocol_sync = 98;
			} else {
				protocol_sync = 5;
				logprintf(LOG_DEBUG, "somfy_rts: prot 5");
			}
//			break;
			case 5:
			// to be implemented: extended protocol
			case 95:
			// Reserved: handle new Generation Frame decoding 24 Bit
			logprintf(LOG_DEBUG, "somfy_rts: Skip new generation data processing for now. pulse: %d pRaw: %d - bin: %d", somfy_rts->raw[pRaw], pRaw, pBin);
			protocol_sync = 99;
			logprintf(LOG_DEBUG, "somfy_rts: End of data. pulse: %d - pRaw: %d - bin: %d", somfy_rts->raw[pRaw], pRaw, pBin);
			break;
			case 96:
			logprintf(LOG_DEBUG, "somfy_rts: Err 96 unexpected EOF.");
			case 97:
			// We decoded a footer pulse without decoding the correct number of binary bits
			logprintf(LOG_DEBUG, "somfy_rts: Err 97 unexpected EOF.");
			case 98:
			logprintf(LOG_DEBUG, "somfy_rts: End of data. pulse: %d - pRaw: %d - bin: %d", somfy_rts->raw[pRaw], pRaw, pBin);
			// We have reached the end of processing raw data
			case 99:
			default:
			break;
		}
		pRaw++;
		if (pRaw>pMaxraw)pMaxraw=pRaw; // Monitor maximum value
		if (protocol_sync > 95) {
			logprintf(LOG_DEBUG, "**** somfy_rts RAW CODE ****");
			if(log_level_get() >= LOG_DEBUG) {
				for(x=0;x<pRaw;x++) {
					printf("%d ", somfy_rts->raw[x]);
				}
				printf(" -#: %d\n",pRaw);
			}
			break;
		}
	}
	if (protocol_sync > 97) {
		frame[0] = (uint8_t) (binToDecRev(binary, 0, 3) << 4);		// Key
		frame[0] = frame[0] | (uint8_t) binToDecRev(binary, 4, 7);	// Key increment counter
		frame[1] = (uint8_t) (binToDecRev(binary, 8,11) << 4);		// command
		frame[1] = frame[1] | (uint8_t) binToDecRev(binary, 12, 15);	// cks;
		frame[2] = (uint8_t) binToDecRev(binary, 16,23);
		frame[3] = (uint8_t) binToDecRev(binary, 24,31);
		frame[4] = (uint8_t) binToDecRev(binary, 32,39);
		frame[5] = (uint8_t) binToDecRev(binary, 40,47);
		frame[6] = (uint8_t) binToDecRev(binary, 48,55);

		dec_frame[0] = frame[0];
		for (i=1; i < BIN_ARRAY_SOMFY_PROT; i++) {
			dec_frame[i] = frame[i] ^ frame[i-1];
		}

		cksum = codeChkSum (dec_frame);

		key_left = dec_frame[0] >> 4;
		rollingkey = dec_frame[0] & 0xf;
		command = dec_frame[1] >> 4;

		rollingcode = dec_frame[2]*256 + dec_frame[3];
		address = dec_frame[4]+dec_frame[5]*256+dec_frame[6]*65536;
		struct settings_t *tmp = settings;
		while(tmp) {
			if(fabs(tmp->address-address) < EPSILON ) {
				tmp->command = (double) command;
				tmp->rollingcode = (double) rollingcode;
				tmp->rollingkey = (double) rollingkey;
				break;
			}
		tmp = tmp->next;
		}

		logprintf(LOG_DEBUG, "**** somfy_rts BIN CODE ****");
		if(log_level_get() >= LOG_DEBUG) {
			for(i=0;i<BIN_ARRAY_SOMFY_PROT;i++) {
				printf("%d ", dec_frame[i]);
			}
			printf(" - addr: %d - rk: %d - rc: %d", address, rollingkey, rollingcode);
			printf("\n");
		}

		if ( ( key_left == 10 && 0 == cksum)) {
			createMessage(address, command, rollingcode, rollingkey);
		} else {
			if(log_level_get() >= LOG_DEBUG) {
				logprintf(LOG_DEBUG, "somfy_rts parseCode Error: Header or Checksum Error");
			}
		}
	} else {
		logprintf(LOG_DEBUG, "somfy_rts parseCode Error");
	}
}

static int *preAmbCode (void) {
// Wakeup sequence called by daemon.c
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if (wakeup_type == 1) return preAmb_wakeup_1;

	return preAmb_wakeup;
}


static int checkValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct settings_t *snode = NULL;
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

		struct settings_t *tmp = settings;
		while(tmp) {
			if(fabs(tmp->address-address) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}
		if(!match) {
			if(!(snode = MALLOC(sizeof(struct settings_t)))) {
				logprintf(LOG_ERR, "somfy_rts: out of memory");
				exit(EXIT_FAILURE);
			}
			snode->address = address;

			json_find_number(jvalues, "command", &snode->command);
			json_find_number(jvalues, "rollingcode", &snode->rollingcode);
			json_find_number(jvalues, "rollingkey", &snode->rollingkey);
			snode->rollingcode = (double) ((int)(snode->rollingcode) & 0xffff);
			snode->rollingkey = (double) ((int)(snode->rollingkey) & 0xf);
			snode->next = settings;
			settings = snode;
		}
	}
	return 0;
}

static void gc(void) {
	struct settings_t *tmp = NULL;
	logprintf(LOG_DEBUG, "somfy_rts: pMaxbin: %d pMaxraw: %d",pMaxbin, pMaxraw);
	while(settings) {
		tmp = settings;
		settings = settings->next;
		FREE(tmp);
	}
	FREE(settings);
}


static void createPulse(int e, int pulse) {
	int i,k;
	k = somfy_rts->rawlen+e;
	if (k<=somfy_rts->maxrawlen) {
		for(i=somfy_rts->rawlen;i<k;i++) {
			somfy_rts->raw[i] = pulse;
		}
		somfy_rts->rawlen=k;
	}
}

static void createFooter(void) {
	if (sDataLow) {
		createPulse(1,PULSE_SOMFY_SHORT);
		createPulse(1,PULSE_SOMFY_FOOTER*PULSE_DIV);
	} else {
		createPulse(1,PULSE_SOMFY_FOOTER*PULSE_DIV+PULSE_SOMFY_SHORT);
	}
}

static void createHeader(void) {
	somfy_rts->rawlen=0;				// There is plenty of space in the structure:
	sDataTime = 0;
	sDataLow = 1;
	createPulse(14,PULSE_SOMFY_SYNC);	// seven SYNC pulses
	createPulse(1,PULSE_SOMFY_START);
	sDataTime = -PULSE_SOMFY_SHORT;	// Mark 1st pulse to be transmitted
}

static void createZero(void) {
	if (sDataTime < 0) {
		somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_SHORT; // the 1st pulse is different
		somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_SHORT;
		sDataTime = PULSE_SOMFY_SHORT;
		sDataLow = 0;
	} else {
		if (sDataLow == 0) {
			somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_SHORT;
			somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_SHORT;
		} else {
			somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_LONG;
			sDataLow = 0;
		}
	}
}

static void createOne(void) {
	if (sDataTime < 0) {
		somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_LONG; // the 1st pulse is different
		sDataTime = PULSE_SOMFY_SHORT;
		sDataLow = 1;
	} else {
		if ( sDataLow == 0 ) {
			somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_LONG;
			sDataLow = 1;
		} else {
			somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_SHORT;
			somfy_rts->raw[somfy_rts->rawlen++] = PULSE_SOMFY_SHORT;
		}
	}
}

static void createData(int iParam, int iLength) {
	int binary[255];
	int i, length, emptylength;

	length = decToBin(iParam, binary);

	// Create leading empty zero pulses and data pulses
	emptylength=iLength-length-1;
	for(i=0;i<emptylength;i++) {
		createZero();
	}
	for(i=0;i<=length;i++) {
		if(binary[i] == 1) {
			createOne();
		} else {
			createZero();
		}
	}
}

static int createCode(JsonNode *code) {
	int key_left = 160;
	int rollingkey = -1;
	int command = -1, command_code = -1;
	int rollingcode = -1;
	int address = -1;
	double itmp = -1;
	uint8_t dec_frame[BIN_ARRAY_SOMFY_PROT], frame[BIN_ARRAY_SOMFY_PROT], i;

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
	if(json_find_number(code, "command_code", &itmp) == 0)	command_code = (int)round(itmp);
	if(json_find_number(code, "wakeup", &itmp) == 0)	wakeup_type = (int)round(itmp);

	if(command != -1 && command_code != -1) {
		logprintf(LOG_ERR, "too many arguments: either command or command_code");
		return EXIT_FAILURE;
	}
	if (command_code != -1) {
		command=command_code;
	}

	if(address==-1 || command==-1) {
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
			struct settings_t *tmp = settings;
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
			struct settings_t *tmp = settings;
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
		createMessage(address, command, rollingcode, rollingkey);
		createHeader();

		dec_frame[0] = (uint8_t) (key_left | (rollingkey & 0xf));
		dec_frame[1] = (uint8_t) (command << 4);
		dec_frame[2] = (uint8_t) ((rollingcode >> 8) & 0xff);
		dec_frame[3] = (uint8_t) (rollingcode & 0xff);
		dec_frame[4] = (uint8_t) (address & 0xff);
		dec_frame[5] = (uint8_t) ((address >> 8) & 0xff);
		dec_frame[6] = (uint8_t) ((address >> 16) & 0xff);

		int cksum = codeChkSum (dec_frame);
		dec_frame[1] = dec_frame[1] | (uint8_t) cksum;

		frame[0] = dec_frame[0];
		createData (frame[0], 8);
		for (i=1; i < 7; i++) {
			frame[i] = dec_frame[i] ^ frame[i-1];
			createData (frame[i], 8);
		}

		createFooter();
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -a --address=address\t\t\tAddress of Remote Controller\n");
	printf("\t -t --up\t\t\tCommand UP\n");
	printf("\t -f --down\t\t\tCommand DOWN\n");
	printf("\t -m --my\t\t\tCommand MY\n");
	printf("\t -g --prog\t\t\tCommand PROG\n");
	printf("\t -c --rollingcode=rollingcode\t\t\tset rollingcode\n");
	printf("\t -k --rollingkey=rollingkey\t\t\tset rollingkey\n");
	printf("\t -n --command_code=command\t\t\tNumeric Command Code\n");
	printf("\t -w --wakeup=command\t\t\tType of Wakeup Pulse\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void somfy_rtsInit(void) {
	protocol_register(&somfy_rts);
	protocol_set_id(somfy_rts, "somfy_rts");
	protocol_device_add(somfy_rts, "somfy_rts", "Telis-1");
	somfy_rts->devtype = SCREEN;
	somfy_rts->hwtype = RF433;
	somfy_rts->minrawlen = MINRAWLEN_SOMFY_PROT;
	somfy_rts->maxrawlen = MAXRAWLEN_SOMFY_PROT;
	somfy_rts->mingaplen = PULSE_SOMFY_FOOTER_L;
	somfy_rts->maxgaplen = PULSE_SOMFY_FOOTER_H;
	// Wakeup sequence
	// Hardware Sync: 2416.2416 or 2416.2416.2416.2416.2416.2416.2416: 1st short Sync not yet implemented at send time
	// START 4550.604.604 1st Data bit=0 or 4550.1208 1st Data bit=1
	// Classic 56 data bits: Byte structure command/rollingkey, ctrl/cks, rolling code[2] bE, address[3]lE
	// 30415 footer or
	// New Generation 28 Bits; Not implemented
	// 27500 Footer

	options_add(&somfy_rts->options, 'a', "address", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,6}|[1-5][0-9]{6}|16[0-6][0-9]{5}|167[0-6][0-9]{4}|1677[0-6][0-9]{3}| 16777[0-1][0-9]{2}|1677720[0-9]|1677721[0-6])$");
	options_add(&somfy_rts->options, 'c', "rollingcode", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
	options_add(&somfy_rts->options, 'k', "rollingkey", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^([0-9]|1[0-5])$");
	options_add(&somfy_rts->options, 'n', "command_code", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL,  "^([0-9]|1[0-5])$");
	options_add(&somfy_rts->options, 't', "up", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 'f', "down", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 'm', "my", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 'g', "prog", OPTION_NO_VALUE,DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 'w', "wakeup", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, (void *)0, "^([01])$");
	options_add(&somfy_rts->options, 0, "my+up", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 0, "my+down", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 0, "up+down", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 0, "sun+flag", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 0, "flag", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&somfy_rts->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	somfy_rts->parseCode=&parseCode;
	somfy_rts->createCode=&createCode;
	somfy_rts->validate=&validate;
	somfy_rts->preAmbCode=preAmbCode;
	somfy_rts->checkValues=&checkValues;
	somfy_rts->printHelp=&printHelp;
	somfy_rts->gc=&gc;

}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name =  "somfy_rts";
	module->version =  "0.99";
	module->reqversion =  "6.0";
	module->reqcommit =  NULL;
}

void init(void) {
	somfy_rtsInit();
}
#endif
