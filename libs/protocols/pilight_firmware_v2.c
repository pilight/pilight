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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "pilight_firmware_v2.h"

static void pilightFirmwareV2CreateMessage(int version, int high, int low) {
	pilight_firmware_v2->message = json_mkobject();
	json_append_member(pilight_firmware_v2->message, "version", json_mknumber(version));
	json_append_member(pilight_firmware_v2->message, "lpf", json_mknumber(high*10));
	json_append_member(pilight_firmware_v2->message, "hpf", json_mknumber(low*10));
}

static void pilightFirmwareV2ParseRaw(void) {
	int i = 0;
	for(i=0;i<pilight_firmware_v2->rawlen;i++) {
		if(pilight_firmware_v2->raw[i] < 100) {
			pilight_firmware_v2->raw[i]*=10;
		}
	}
}

static void pilightFirmwareV2ParseBinary(void) {
	int version = binToDec(pilight_firmware_v2->binary, 0, 15);
	int high = binToDec(pilight_firmware_v2->binary, 16, 31);
	int low = binToDec(pilight_firmware_v2->binary, 32, 47);
	pilightFirmwareV2CreateMessage(version, high, low);
}

#ifndef MODULE
__attribute__((weak))
#endif
void pilightFirmwareV2Init(void) {

  protocol_register(&pilight_firmware_v2);
  protocol_set_id(pilight_firmware_v2, "pilight_firmware");
  protocol_device_add(pilight_firmware_v2, "pilight_firmware", "pilight filter firmware");
  protocol_plslen_add(pilight_firmware_v2, 230);
  protocol_plslen_add(pilight_firmware_v2, 220);

  pilight_firmware_v2->devtype = INTERNAL;
  pilight_firmware_v2->hwtype = HWINTERNAL;
  pilight_firmware_v2->pulse = 3;
  pilight_firmware_v2->rawlen = 196;
  pilight_firmware_v2->lsb = 3;

  options_add(&pilight_firmware_v2->options, 'v', "version", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[0-9]+$");
  options_add(&pilight_firmware_v2->options, 'l', "lpf", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[0-9]+$");
  options_add(&pilight_firmware_v2->options, 'h', "hpf", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[0-9]+$");

  pilight_firmware_v2->parseBinary=&pilightFirmwareV2ParseBinary;
  pilight_firmware_v2->parseRaw=&pilightFirmwareV2ParseRaw;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "pilight_firmware";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	pilightFirmwareV2Init();
}
#endif
