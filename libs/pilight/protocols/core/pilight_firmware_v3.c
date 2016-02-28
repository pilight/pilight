/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "pilight_firmware_v3.h"

#define PULSE_MULTIPLIER	4
#define MIN_PULSE_LENGTH	205
#define MAX_PULSE_LENGTH	245
#define AVG_PULSE_LENGTH	225
#define RAW_LENGTH				212

static int validate(void) {
	if(pilight_firmware_v3->rawlen == RAW_LENGTH) {
		if(pilight_firmware_v3->raw[pilight_firmware_v3->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   pilight_firmware_v3->raw[pilight_firmware_v3->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV) &&
			 pilight_firmware_v3->raw[1] > AVG_PULSE_LENGTH*PULSE_MULTIPLIER) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(char *message) {
	int i = 0, x = 0, binary[RAW_LENGTH/4];

	for(i=0;i<pilight_firmware_v3->rawlen;i+=4) {
		if(pilight_firmware_v3->raw[i+3] < 100) {
			pilight_firmware_v3->raw[i+3]*=10;
		}
		if(pilight_firmware_v3->raw[i+3] > AVG_PULSE_LENGTH*(PULSE_MULTIPLIER/2)) {
			binary[x++] = 1;
		} else {
			binary[x++] = 0;
		}
	}

	int version = binToDec(binary, 0, 15);
	int high = binToDec(binary, 16, 31);
	int low = binToDec(binary, 32, 47);
	int chk = binToDec(binary, 48, 51);
	int lpf = low;
	int hpf = high;
	int ver = version;

	while(hpf > 10) {
		hpf /= 10;
	}
	while(lpf > 10) {
		lpf /= 10;
	}
	while(ver > 10) {
		ver /= 10;
	}

	if((((ver&0xf)+(lpf&0xf)+(hpf&0xf))&0xf) == chk) {
		snprintf(message, 255, "{\"version\":%d,\"lpf\":%d,\"hpf\":%d}", version, high*10, low*10);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void pilightFirmwareV3Init(void) {

  protocol_register(&pilight_firmware_v3);
  protocol_set_id(pilight_firmware_v3, "pilight_firmware");
  protocol_device_add(pilight_firmware_v3, "pilight_firmware", "pilight filter firmware");
  pilight_firmware_v3->devtype = FIRMWARE;
  pilight_firmware_v3->hwtype = HWINTERNAL;
	pilight_firmware_v3->minrawlen = RAW_LENGTH;
	pilight_firmware_v3->maxrawlen = RAW_LENGTH;
	pilight_firmware_v3->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	pilight_firmware_v3->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

  options_add(&pilight_firmware_v3->options, 'v', "version", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]+$");
  options_add(&pilight_firmware_v3->options, 'l', "lpf", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]+$");
  options_add(&pilight_firmware_v3->options, 'h', "hpf", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]+$");

  pilight_firmware_v3->parseCode=&parseCode;
  pilight_firmware_v3->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "pilight_firmware";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	pilightFirmwareV3Init();
}
#endif
