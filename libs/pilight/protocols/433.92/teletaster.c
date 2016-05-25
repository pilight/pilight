/*
	Copyright (C) 2016 - 2017 ramyres

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
#include "teletaster.h"

#define PULSE_MULTIPLIER	4
#define MIN_PULSE_LENGTH	450
#define MAX_PULSE_LENGTH	480
#define AVG_PULSE_LENGTH	465
#define RAW_LENGTH			36

#define ID_LENGTH 9

static int validate(void) {
	if(teletaster->rawlen == RAW_LENGTH) {
		if(teletaster->raw[teletaster->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   teletaster->raw[teletaster->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}
	return -1;
}

static void createMessage(char* id) {
	teletaster->message = json_mkobject();
	json_append_member(teletaster->message, "id", json_mkstring(id));
}

static void parseCode(void) {
	int x = 0, i = 0;
	int len = (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2));
	char id[ID_LENGTH] = "abcdefghi";

	for(x=0;x<teletaster->rawlen-2;x+=4) {
		if(teletaster->raw[x] > len) {
			if(teletaster->raw[x+2] > len) {
				id[i++] = '1';
			} else {
				id[i++] = 'X';
			}
		} else {
			id[i++] = '0';
		}
	}
	createMessage(id);
}

static void createZero(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		teletaster->raw[i]=(AVG_PULSE_LENGTH);
		teletaster->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		teletaster->raw[i+2]=(AVG_PULSE_LENGTH);
		teletaster->raw[i+3]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void createOne(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		teletaster->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		teletaster->raw[i+1]=(AVG_PULSE_LENGTH);
		teletaster->raw[i+2]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		teletaster->raw[i+3]=(AVG_PULSE_LENGTH);
	}
}

static void createX(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		teletaster->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		teletaster->raw[i+1]=(AVG_PULSE_LENGTH);
		teletaster->raw[i+2]=(AVG_PULSE_LENGTH);
		teletaster->raw[i+3]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createX(0,35);
}

static void createId(char* id) {
	int length = 9;
	int i=0, x=0;

	for(i=0;i<=length;i++) {
		if(id[i]=='1') {
			x=i*4;
			createOne(x, x+3);
		}
		else if (id[i]=='0') {
			x=i*4;
			createZero(x, x+3);
		}
	}
}

static void createFooter(void) {
	teletaster->raw[35]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code) {
	char *id = NULL;
	int i = 0;

	if(json_find_string(code, "id", &id) != 0) {
		logprintf(LOG_ERR, "teletaster: insufficient number of arguments");
		return EXIT_FAILURE;
	}

	if(strlen(id) != ID_LENGTH) {
		logprintf(LOG_ERR, "teletaster: invalid id size");
		return EXIT_FAILURE;
	} else {
		for(i=0; i<ID_LENGTH; i++) {
			if(*(id+i) != '1' && *(id+i) != '0' && *(id+i) != 'x' && *(id+i) != 'X') {
				logprintf(LOG_ERR, "teletaster: invalid id range");
				return EXIT_FAILURE;
			}
		}
		createMessage(id);
		clearCode();
		createId(id);
		createFooter();
		teletaster->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -i --id=id\t\t\tsend trigger signal to a device with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void teletasterInit(void) {

	protocol_register(&teletaster);
	protocol_set_id(teletaster, "teletaster");
	protocol_device_add(teletaster, "teletaster", "Tedsen Teletaster Switches");
	protocol_device_add(teletaster, "siral_switch", "Siral Switches");
	teletaster->devtype = SWITCH;
	teletaster->hwtype = RF433;
	teletaster->minrawlen = RAW_LENGTH;
	teletaster->maxrawlen = RAW_LENGTH;
	teletaster->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	teletaster->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&teletaster->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&teletaster->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&teletaster->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^([01xX]{9})$");

	options_add(&teletaster->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&teletaster->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	teletaster->parseCode=&parseCode;
	teletaster->createCode=&createCode;
	teletaster->printHelp=&printHelp;
	teletaster->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "teletaster";
	module->version = "0.1";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	teletasterInit();
}
#endif
