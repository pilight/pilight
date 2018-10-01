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
#include "sc2262.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	427
#define MAX_PULSE_LENGTH	444
#define AVG_PULSE_LENGTH	432
#define RAW_LENGTH				50

static int validate(void) {
	if(sc2262->rawlen == RAW_LENGTH) {
		if(sc2262->raw[sc2262->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   sc2262->raw[sc2262->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void parseCode(char **message) {
	int binary[RAW_LENGTH/4], x = 0, i = 0;

	if(sc2262->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "sc2262: parsecode - invalid parameter passed %d", sc2262->rawlen);
		return;
	}

	for(x=0;x<sc2262->rawlen-2;x+=4) {
		if(sc2262->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	int systemcode = binToDec(binary, 0, 4);
	int unitcode = binToDec(binary, 5, 9);
	int state = binary[11];
	x = snprintf((*message), 255, "{\"systemcode\":%d,", systemcode);
	x += snprintf(&(*message)[x], 255-x, "\"unitcode\":%d,", unitcode);

	if(state == 0) {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"opened\"");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"closed\"");
	}
	x += snprintf(&(*message)[x], 255-x, "}");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void sc2262Init(void) {

	protocol_register(&sc2262);
	protocol_set_id(sc2262, "sc2262");
	protocol_device_add(sc2262, "sc2262", "sc2262 contact sensor");
	sc2262->devtype = CONTACT;
	sc2262->hwtype = RF433;
	sc2262->minrawlen = RAW_LENGTH;
	sc2262->maxrawlen = RAW_LENGTH;
	sc2262->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	sc2262->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&sc2262->options, "s", "systemcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&sc2262->options, "u", "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&sc2262->options, "t", "opened", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&sc2262->options, "f", "closed", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	sc2262->parseCode=&parseCode;
	sc2262->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "sc2262";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	sc2262Init();
}
#endif
