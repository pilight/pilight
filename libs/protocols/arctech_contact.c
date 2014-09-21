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
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "arctech_contact.h"

static void arctechContactCreateMessage(int id, int unit, int state, int all) {
	arctech_contact->message = json_mkobject();
	json_append_member(arctech_contact->message, "id", json_mknumber(id));
	if(all == 1) {
		json_append_member(arctech_contact->message, "all", json_mknumber(all));
	} else {
		json_append_member(arctech_contact->message, "unit", json_mknumber(unit));
	}

	if(state == 1) {
		json_append_member(arctech_contact->message, "state", json_mkstring("opened"));
	} else {
		json_append_member(arctech_contact->message, "state", json_mkstring("closed"));
	}
}

static void arctechContactParseBinary(void) {
	int unit = binToDecRev(arctech_contact->binary, 28, 31);
	int state = arctech_contact->binary[27];
	int all = arctech_contact->binary[26];
	int id = binToDecRev(arctech_contact->binary, 0, 25);

	arctechContactCreateMessage(id, unit, state, all);
}

#ifndef MODULE
__attribute__((weak))
#endif
void arctechContactInit(void) {

	protocol_register(&arctech_contact);
	protocol_set_id(arctech_contact, "arctech_contact");
	protocol_device_add(arctech_contact, "kaku_contact", "KlikAanKlikUit Contact Sensor");
	protocol_device_add(arctech_contact, "dio_contact", "D-IO Contact Sensor");
	protocol_plslen_add(arctech_contact, 294);
	protocol_plslen_add(arctech_contact, 305);
	protocol_plslen_add(arctech_contact, 294);

	arctech_contact->devtype = SWITCH;
	arctech_contact->hwtype = RF433;
	arctech_contact->pulse = 4;
	arctech_contact->rawlen = 140;
	arctech_contact->minrawlen = 132;
	arctech_contact->maxrawlen = 148;
	arctech_contact->lsb = 3;

	options_add(&arctech_contact->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_contact->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
	options_add(&arctech_contact->options, 't', "opened", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_contact->options, 'f', "closed", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&arctech_contact->options, 'a', "all", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&arctech_contact->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	arctech_contact->parseBinary=&arctechContactParseBinary;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "arctech_contact";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = "38";
}

void init(void) {
	arctechContactInit();
}
#endif
