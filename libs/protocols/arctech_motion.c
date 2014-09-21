/*
	Copyright (C) 2014 CurlyMo & lvdp

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
#include "arctech_motion.h"

static void arctechMotionCreateMessage(int id, int unit, int state, int all) {
	arctech_motion->message = json_mkobject();
	json_append_member(arctech_motion->message, "id", json_mknumber(id));
	if(all == 1) {
		json_append_member(arctech_motion->message, "all", json_mknumber(all));
	} else {
		json_append_member(arctech_motion->message, "unit", json_mknumber(unit));
	}

	if(state == 1) {
		json_append_member(arctech_motion->message, "state", json_mkstring("on"));
	} else {
		json_append_member(arctech_motion->message, "state", json_mkstring("off"));
	}
}

static void arctechMotionParseBinary(void) {
	int unit = binToDecRev(arctech_motion->binary, 28, 31);
	int state = arctech_motion->binary[27];
	int all = arctech_motion->binary[26];
	int id = binToDecRev(arctech_motion->binary, 0, 25);

	arctechMotionCreateMessage(id, unit, state, all);
}

#ifndef MODULE
__attribute__((weak))
#endif
void arctechMotionInit(void) {

	protocol_register(&arctech_motion);
	protocol_set_id(arctech_motion, "arctech_motion");
	protocol_device_add(arctech_motion, "kaku_motion", "KlikAanKlikUit Motion Sensor");
	protocol_plslen_add(arctech_motion, 279);

	arctech_motion->devtype = SWITCH;
	arctech_motion->hwtype = RF433;
	arctech_motion->pulse = 4;
	arctech_motion->rawlen = 132;
	arctech_motion->lsb = 3;

	options_add(&arctech_motion->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_motion->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
	options_add(&arctech_motion->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_motion->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&arctech_motion->options, 'a', "all", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&arctech_motion->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	arctech_motion->parseBinary=&arctechMotionParseBinary;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "arctech_motion";
	module->version = "0.1";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	arctechMotionInit();
}
#endif
