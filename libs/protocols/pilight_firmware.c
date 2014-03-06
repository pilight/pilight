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
    along with pilight. If not, see <http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "pilight_firmware.h"

void pilightFirmwareCreateMessage(int version, int high, int low) {
	pilight_firmware->message = json_mkobject();
	json_append_member(pilight_firmware->message, "version", json_mknumber(version));
	json_append_member(pilight_firmware->message, "lpf", json_mknumber(high*10));
	json_append_member(pilight_firmware->message, "hpf", json_mknumber(low*10));
}

void pilightFirmwareParseBinary(void) {
	int version = binToDec(pilight_firmware->binary, 0, 15);
	int high = binToDec(pilight_firmware->binary, 16, 31);
	int low = binToDec(pilight_firmware->binary, 32, 47);
	pilightFirmwareCreateMessage(version, high, low);
}

void pilightFirmwareInit(void) {

  protocol_register(&pilight_firmware);
  protocol_set_id(pilight_firmware, "pilight_firmware");
  protocol_device_add(pilight_firmware, "pilight_firmware", "pilight filter firmware");
  protocol_plslen_add(pilight_firmware, 232);

  pilight_firmware->devtype = INTERNAL;
  pilight_firmware->hwtype = HWINTERNAL;
  pilight_firmware->pulse = 3;
  pilight_firmware->rawlen = 196;
  pilight_firmware->lsb = 3;

  options_add(&pilight_firmware->options, 'v', "version", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[0-9]+$");
  options_add(&pilight_firmware->options, 'l', "lpf", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[0-9]+$");
  options_add(&pilight_firmware->options, 'h', "hpf", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^[0-9]+$");

  pilight_firmware->parseBinary=&pilightFirmwareParseBinary;
}
