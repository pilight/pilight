/*
	Copyright (C) 2014 - 2016 CurlyMo

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
#include "secudo_smoke.h"

#define PULSE_MULTIPLIER	2
#define MIN_PULSE_LENGTH	292
#define MAX_PULSE_LENGTH	332
#define AVG_PULSE_LENGTH	312
#define RAW_LENGTH			26

static int validate(void) {
	if(secudo_smoke->rawlen == RAW_LENGTH) {
		if(secudo_smoke->raw[secudo_smoke->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   secudo_smoke->raw[secudo_smoke->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(char **message) {
	int binary[RAW_LENGTH/2];
	int id = 0;
	int x = 0, i = 0;

	int len = (AVG_PULSE_LENGTH*(PULSE_MULTIPLIER+1)) / 2;

	if(secudo_smoke->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "secudo_smoke: parsecode - invalid parameter passed %d", secudo_smoke->rawlen);
		return;
	}

	for(x=1;x<secudo_smoke->rawlen-2;x+=2) {
		if(secudo_smoke->raw[x] > len) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	id = binToDec(binary, 0, 9);
	id = (~id) & 1023;

	x = snprintf((*message), 255, "{\"id\":%d,", id);
	x += snprintf(&(*message)[x], 255-x, "\"state\":\"alarm\"");
	x += snprintf(&(*message)[x], 255-x, "}");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void secudoSmokeInit(void) {
	protocol_register(&secudo_smoke);
	protocol_set_id(secudo_smoke, "secudo_smoke_sensor");
	protocol_device_add(secudo_smoke, "secudo_smoke_sensor", "Secudo/FlammEx smoke sensor");
	secudo_smoke->devtype = ALARM;
	secudo_smoke->hwtype = RF433;
	secudo_smoke->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	secudo_smoke->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;
	secudo_smoke->minrawlen = 26;
	secudo_smoke->maxrawlen = 26;

	options_add(&secudo_smoke->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]{1-4}$");
	options_add(&secudo_smoke->options, "t", "alarm", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	secudo_smoke->parseCode=&parseCode;
	secudo_smoke->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "secudo_smoke_sensor";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	secudoSmokeInit();
}
#endif
