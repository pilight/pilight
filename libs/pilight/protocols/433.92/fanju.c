/*
  Copyright (C) 2021 CurlyMo
  Copyright (C) 2021 Johan van Oostrum

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/* *********************************************************************
	Protocol characteristics:
	4x Sync: 936/936, 1x Head: 468/7956, Logical 0: 468/1872, Logical 1: 468/3744-4212, 1x Tail: 468/15912

	Fanju sensor message format:
	____ ____ ____ 1___ 1___ 2___ 2___ 2___ 3___ 3___
	0... 4... 8... 2... 6... 0... 4... 8... 2... 6...

	0010 0001 1000 1100 0110 0101 1000 0011 0101 0010
	AAAA AAAA BBBB xyzz CCCC CCCC CCCC DDDD DDDD EEFF

	A = ID
	B = checksum?
	B = x 0=scheduled, 1=requested transmission
	    y 0=ok, 1=battery low
	    zz ?
	C = temperature in Fahrenheit (binary)
	D = humidity (BCD format)
	E = .. ?
	F = channel

	ID   21
	CS   8
	Tr   1 => requested
	Bat  1 => low
	..   00 (?)
	Temp 0x658 => (((0x658 - 0x4C4)=0x194 * 5) =0x7E4 /9) = 0xE0 = 22.4°
	Hum  35%
	..   00 (?)
	Ch   2
   ********************************************************************* */

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
#include "fanju.h"

#define MIN_PULSE_LENGTH 460
#define MAX_PULSE_LENGTH 470
#define ZERO_PULSE       1872
#define ONE_PULSE        3744
#define AVG_PULSE        (ZERO_PULSE + ONE_PULSE) / 2
#define RAW_LENGTH       92
#define MSG_LENGTH       RAW_LENGTH / 2
#define OFFSET           5

typedef struct settings_t {
	double id;
	double temp;
	double humi;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(fanju->rawlen == RAW_LENGTH) {
		// TAIL
		if(fanju->raw[fanju->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   fanju->raw[fanju->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}
	return -1;
}

static void parseCode(void) {
	int i=0, x=0, binary[MSG_LENGTH];
	int binary_cpy[MSG_LENGTH], mask=0, checksum_calc=0, bit=0;
	int header=0, id=0, channel=0, battery=0, checksum=0;
	double temp_offset=0.0, temperature=0.0, temp_fahrenheit=0.0, temp_celsius=0.0;
	double humi_offset=0.0, humidity=0.0;
	int humidity_10;

	if(fanju->rawlen > RAW_LENGTH) {
		logprintf(LOG_ERR, "fanju: parsecode - invalid parameter passed %d", fanju->rawlen);
		return;
	}

	x = 0;
	for(i=1; i < fanju->rawlen - 2; i+=2) {
		if(fanju->raw[i] > AVG_PULSE) {
			binary[x++] = 1;
		} else {
			binary[x++] = 0;
		}
	}

	// 4x SYNC '0' + 1x HEAD '1'
	header = binToDecRev(binary, 0, 4);
	if(header != 1) {
		logprintf(LOG_ERR, "fanju: parsecode - invalid header %d", header);
		return;
	}

	id = binToDecRev(binary, OFFSET, OFFSET + 7);
	checksum = binToDecRev(binary, OFFSET + 8, OFFSET + 11);
	battery = binary[13];
	temp_fahrenheit = (double) binToDecRevUl(binary, OFFSET + 16, OFFSET + 27);
	temp_celsius = ((temp_fahrenheit - 0x4C4) * 5) / 9;
	temperature = temp_celsius / 10;
	humidity_10 = binToDecRev(binary, OFFSET + 28, OFFSET + 31);
	humidity = (double) binToDecRev(binary, OFFSET + 32, OFFSET + 35);
	humidity += humidity_10 * 10;
	channel = binToDecRev(binary, OFFSET + 38, OFFSET + 39);

	// move channel to the checksum position
	x = 0;
	for(i=OFFSET + 0; i < OFFSET + 8; i++) {
		binary_cpy[x++] = binary[i];
	}
	for(i=OFFSET + 36; i < OFFSET + 40; i++) {
		binary_cpy[x++] = binary[i];
	}
	for(i=OFFSET + 12; i < OFFSET + 36; i++) {
		binary_cpy[x++] = binary[i];
	}
	// verify checksum
	mask = 0xC;
	checksum_calc = 0x0;
	for(i=0; i < 36; i++) {
		bit = mask & 0x1;
		mask >>= 1;
		if(bit == 0x1) {
			mask ^= 0x9;
		}
		if(binary_cpy[i] == 1) {
			checksum_calc ^= mask;
		}
	}
	if(checksum != checksum_calc) {
		logprintf(LOG_ERR, "fanju: parsecode - invalid checksum: %d calc: %d", checksum, checksum_calc);
		return;
	}

	struct settings_t *tmp = settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON) {
			humi_offset = tmp->humi;
			temp_offset = tmp->temp;
			break;
		}
		tmp = tmp->next;
	}
	temperature += temp_offset;
	humidity += humi_offset;

	if(channel != 4) {
		fanju->message = json_mkobject();
		json_append_member(fanju->message, "id", json_mknumber(id, 0));
		json_append_member(fanju->message, "temperature", json_mknumber(temperature, 1));
		json_append_member(fanju->message, "humidity", json_mknumber(humidity, 1));
		json_append_member(fanju->message, "battery", json_mknumber(battery, 0));
		json_append_member(fanju->message, "channel", json_mknumber(channel, 0));
	}
}

static int checkValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct settings_t *snode = NULL;
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;
		double id = -1;
		int match = 0;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "id") == 0) {
					id = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}

		struct settings_t *tmp = settings;
		while(tmp) {
			if(fabs(tmp->id - id) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(match == 0) {
			if((snode = MALLOC(sizeof(struct settings_t))) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			snode->id = id;
			snode->humi = 0;
			snode->temp = 0;
			json_find_number(jvalues, "humidity-offset", &snode->humi);
			json_find_number(jvalues, "temperature-offset", &snode->temp);
			snode->next = settings;
			settings = snode;
		}
	}
	return 0;
}

static void gc(void) {
	struct settings_t *tmp = NULL;
	while(settings) {
		tmp = settings;
		settings = settings->next;
		FREE(tmp);
	}
	if(settings != NULL) {
		FREE(settings);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void fanjuInit(void) {

	protocol_register(&fanju);
	protocol_set_id(fanju, "fanju");
	protocol_device_add(fanju, "fanju", "Fanju 3378 Weather Stations");

	fanju->devtype = WEATHER;
	fanju->hwtype = RF433;
	fanju->minrawlen = RAW_LENGTH;
	fanju->maxrawlen = RAW_LENGTH;
	fanju->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	fanju->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&fanju->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&fanju->options, "c", "channel", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[1-3]");
	options_add(&fanju->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&fanju->options, "h", "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,2}$");
	options_add(&fanju->options, "b", "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");

	options_add(&fanju->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&fanju->options, "0", "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&fanju->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&fanju->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");
	options_add(&fanju->options, "0", "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");
	options_add(&fanju->options, "0", "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");

	fanju->parseCode=&parseCode;
	fanju->checkValues=&checkValues;
	fanju->validate=&validate;
	fanju->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "fanju";
	module->version = "0.1";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	fanjuInit();
}
#endif
