/*
  Copyright (C) 2022 CurlyMo

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
#include "hs2241pt.h"

#define PULSE_MULTIPLIER	4
#define MIN_PULSE_LENGTH	251
#define MAX_PULSE_LENGTH	800
#define AVG_PULSE_LENGTH	400
#define RAW_LENGTH				50

static int validate(void) {
	if(hs2241pt->rawlen == RAW_LENGTH) {
		if(hs2241pt->raw[hs2241pt->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   hs2241pt->raw[hs2241pt->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int unitcode, int state, const char *tx_data) {
	hs2241pt->message = json_mkobject();
	json_append_member(hs2241pt->message, "id", json_mknumber(unitcode, 0));
	json_append_member(hs2241pt->message, "state", json_mknumber(state, 0));
	json_append_member(hs2241pt->message, "txdata", json_mkstring(tx_data));
}

static void parseCode(void) {

	int binary[RAW_LENGTH/2], x = 0, i = 0;

	if(hs2241pt->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "hs2241pt: parsecode - invalid parameter passed %d", hs2241pt->rawlen);
		return;
	}

	for(x=1;x<hs2241pt->rawlen-2;x+=2) {
		if(hs2241pt->raw[x] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 0;
		} else {
			binary[i++] = 1;
		}
	}

	int unitcode = binToDecRev(binary, 0, 19);
	int state = binToDecRev(binary, 20,23);
	char tx_data[7];
	sprintf(tx_data, "%06X", binToDecRev(binary, 0, 23));
	createMessage(unitcode, state, tx_data);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void hs2241ptInit(void) {

	protocol_register(&hs2241pt);
	protocol_set_id(hs2241pt, "hs2241pt");
	protocol_device_add(hs2241pt, "hs2241pt", "hs2241pt device");
	hs2241pt->devtype = CONTACT;
	hs2241pt->hwtype = RF433;
	hs2241pt->minrawlen = RAW_LENGTH;
	hs2241pt->maxrawlen = RAW_LENGTH;
	hs2241pt->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	hs2241pt->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&hs2241pt->options, "id", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]{1,6}");
	options_add(&hs2241pt->options, "s", "state", OPTION_NO_VALUE, DEVICES_STATE, JSON_NUMBER, NULL, "^([0-9]|1[0-5])$");

	hs2241pt->parseCode=&parseCode;
	hs2241pt->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "hs2241pt";
	module->version = "1.2";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	hs2241ptInit();
}
#endif
