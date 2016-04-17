/*
	Copyright (C) 2016 yablacky

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

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "unitec.h"

#define MIN_PULSE_LENGTH	(250)
#define MAX_PULSE_LENGTH	(820-MIN_PULSE_LENGTH)
#define AVG_PULSE_LENGTH	((MAX_PULSE_LENGTH + MIN_PULSE_LENGTH) / 2)

#define MIN_PULSE_DIV		(180*PULSE_DIV)
#define OUR_PULSE_DIV		(190*PULSE_DIV)
#define MAX_PULSE_DIV		(200*PULSE_DIV)

#define RAW_LENGTH		50

static int validate(void) {
	if(unitec_switch->rawlen == RAW_LENGTH) {
		if(unitec_switch->raw[RAW_LENGTH-1] >= MIN_PULSE_DIV &&
		   unitec_switch->raw[RAW_LENGTH-1] <= MAX_PULSE_DIV &&
		   unitec_switch->raw[RAW_LENGTH-2] < AVG_PULSE_LENGTH) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(const char *id, int unit, int state) {
	unitec_switch->message = json_mkobject();
	json_append_member(unitec_switch->message, "id", json_mkstring(id));
	json_append_member(unitec_switch->message, "unit", json_mknumber(unit, 0));
	json_append_member(unitec_switch->message, "state", json_mkstring(state ? "on" : "off"));
}

static void parseCode(void) {
	int frame[RAW_LENGTH/2], x = 0;

	if(unitec_switch->rawlen != RAW_LENGTH)
	    return;

	/*
	 * I reverse-engineered it for transmitter 48113, Model EIM-827 and
	 * corresponding switch.
	 * It seems that other unitec transmitter/switch share the same
	 * protocol. See here:
	 * https://sites.google.com/site/arduinohomeautomation/home/unitec_eim826
	 * (there 0 and 1 is vice versa, frame bit positions are counted reverse).
	 */

	for(x=0; x < countof(frame); x++) {
		// High "1" if first > second, Low "0" else
		frame[x] = unitec_switch->raw[x*2] > unitec_switch->raw[x*2+1];
	}

        int idn = binToDec(frame, 0, 19);
	int unit = binToDec(frame, 20 + 0, 20 + 2) ^ 7;
	int state = frame[20 + 3];

	unit = unit == 4 ? 0 : (unit + 1);

	char id[16];
	sprintf(id, "X%X", idn);

	createMessage(id, unit, state);
}

static void createLow(int s, int c) {

	while(--c >= 0) {
		unitec_switch->raw[    2*s  ]=MIN_PULSE_LENGTH;
		unitec_switch->raw[1 + 2*s++]=MAX_PULSE_LENGTH;
	}
}

static void createHigh(int s, int c) {

	while(--c >= 0) {
		unitec_switch->raw[    2*s  ]=MAX_PULSE_LENGTH;
		unitec_switch->raw[1 + 2*s++]=MIN_PULSE_LENGTH;
	}
}

static void clearCode(void) {
	createLow(0, 20 + 3 + 1);
}

static void createId(int idn) {

	int pos = 0, len = 20;
	for(;--len >= 0; idn >>= 1) {
	    ((idn & 1) ? createHigh : createLow)(pos++, 1);
	}
}

static void createUnit(int unit) {

	unit = unit == 0 ? 4 : (unit - 1);

	int pos = 20, len = 3;
	for(; --len >= 0; unit >>= 1) {
	    // inverted:
	    ((unit & 1) ? createLow : createHigh)(pos++, 1);
	}
}

static void createState(int state) {
	// pos = 23, len = 1
	(state == 0 ? createLow : createHigh)(23, 1);
}

static void createFooter(void) {
	unitec_switch->raw[RAW_LENGTH - 2] = MIN_PULSE_LENGTH;
	unitec_switch->raw[RAW_LENGTH - 1] = OUR_PULSE_DIV;
	unitec_switch->rawlen = RAW_LENGTH;
}

static int createCode(const struct JsonNode *code) {
	const char *id = NULL;
	int unit = -1;
	int state = -1;
	double itmp;

	if(json_find_string(code, "id", &id) != 0) {
		logprintf(LOG_ERR, "unitec_switch: missing --id argument");
		return EXIT_FAILURE;
	}

	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;
	else {
		logprintf(LOG_ERR, "unitec_switch: missing state argument (--on or --off)");
		return EXIT_FAILURE;
	}

	if(json_find_number(code, "unit", &itmp) == 0) {
		unit = (int)itmp;
	} else {
		logprintf(LOG_ERR, "unitec_switch: missing --unit argument (1..4 or 0 for all)");
		return EXIT_FAILURE;
	}

	if(id[0] != 'X' && id[0] != 'x') {
		logprintf(LOG_ERR, "unitec_switch: id must start with 'X' followed by hex-digits (0..9 and A-F).");
		return EXIT_FAILURE;
	}
	char *end = NULL;
	int idn = strtol(id+1, &end, 16);
	if(!end || end==id+1 || *end) {
		logprintf(LOG_ERR, "unitec_switch: invalid char in id: %s ", end ? end : "?");
		return EXIT_FAILURE;
	}
	if (end -(id+1) > 5) {
		logprintf(LOG_ERR, "unitec_switch: too long id. Max 5 digits allowed.");
		return EXIT_FAILURE;
	}

	if(unit < 0 || unit > 4) {
		logprintf(LOG_ERR, "unitec_switch: unit not in (1..4 or 0 for all)");
		return EXIT_FAILURE;
	}

	createMessage(id, unit, state);

	clearCode();
	createId(idn);
	createUnit(unit);
	createState(state);
	createFooter();

	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code (1..4, 0=all)\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id (Xhexdigits, e.g. 'X0')\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void unitecSwitchInit(void) {

	protocol_register(&unitec_switch);
	protocol_set_id(unitec_switch, "unitec_switch");
	protocol_device_add(unitec_switch, "unitec_switch", "Unitec Switches");
	unitec_switch->devtype = SWITCH;
	unitec_switch->hwtype = RF433;
	unitec_switch->minrawlen = RAW_LENGTH;
	unitec_switch->maxrawlen = RAW_LENGTH;
	unitec_switch->maxgaplen = MAX_PULSE_DIV;
	unitec_switch->mingaplen = MIN_PULSE_DIV;

	options_add(&unitec_switch->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&unitec_switch->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&unitec_switch->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-5]$");
	options_add(&unitec_switch->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^X[0-9A-Fa-f]+$");

	options_add(&unitec_switch->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&unitec_switch->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	unitec_switch->parseCode=&parseCode;
	unitec_switch->createCode=&createCode;
	unitec_switch->printHelp=&printHelp;
	unitec_switch->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "unitec_switch";
	module->version = "0.9";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}
#endif
