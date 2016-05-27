/*
	Copyright (C) 2016 Puuu

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
#include "maxitronic.h"

#define PULSE_MULTIPLIER	3
#define MIN_PULSE_LENGTH	274
#define MAX_PULSE_LENGTH	320
#define AVG_PULSE_LENGTH	300
#define RAW_LENGTH		50

/*
	Maxi-Tronic switch

	logical representations of pulses (50 pulses):
	Low	300, 900, 900, 300
	Med	900, 300, 900, 300
	High	300, 900, 300, 900
	Footer	300, 10000

	message format (12 bit):
	01234 56789 01
	iiiii uuuuu sc

	i: id     binary representation with High and Low
		  additionally somewhere one Med
	u: unit   binary representation with High and Low
	s: state  On: High, Off: Low
	c: check  inverse of state
*/

static int validate(void) {
	if(maxitronic->rawlen == RAW_LENGTH) {
		if(maxitronic->raw[maxitronic->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   maxitronic->raw[maxitronic->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(const char *id, int unit, int state) {
	maxitronic->message = json_mkobject();
	json_append_member(maxitronic->message, "id", json_mkstring(id));
	json_append_member(maxitronic->message, "unit", json_mknumber(unit, 0));
	if(state != 0)
		json_append_member(maxitronic->message, "state", json_mkstring("on"));
	else
		json_append_member(maxitronic->message, "state", json_mkstring("off"));
}

static void parseCode(void) {
	int x = 0, z = 'A', binary[RAW_LENGTH/4];
	char id[4];

	if(maxitronic->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "maxitronic: parsecode - invalid parameter passed %d", maxitronic->rawlen);
		return;
	}

	/* Convert the one's and zero's into binary */
	for(x=0;x<maxitronic->rawlen-2;x+=4) {
		if(maxitronic->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[x/4]=1;
		} else if(maxitronic->raw[x+0] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[x/4]=2;
		} else {
			binary[x/4]=0;
		}
	}

	for(x=0;x<=4;x++) {
		if(binary[x] == 2) {
			break;
		}
		z++;
	}

	int unit = binToDec(binary, 5, 9);
	int state = binary[10];
	int check = binary[11];
	int y = binToDec(binary, 0, 4);
	sprintf(&id[0], "%c%d", z, y);

	if(check != state) {
		createMessage(id, unit, state);
	}
}

static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		maxitronic->raw[i]=(AVG_PULSE_LENGTH);
		maxitronic->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		maxitronic->raw[i+2]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		maxitronic->raw[i+3]=(AVG_PULSE_LENGTH);
	}
}

static void createMed(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		maxitronic->raw[i]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		maxitronic->raw[i+1]=(AVG_PULSE_LENGTH);
		maxitronic->raw[i+2]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		maxitronic->raw[i+3]=(AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		maxitronic->raw[i]=(AVG_PULSE_LENGTH);
		maxitronic->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		maxitronic->raw[i+2]=(AVG_PULSE_LENGTH);
		maxitronic->raw[i+3]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createLow(0,47);
}

static void createId(const char *id) {
	int l = ((int)(id[0]-'A'));
	int y = atoi(&id[1]);
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(y, binary);
	for(i=0;i<=length;i++) {
		x=i*4;
		if(binary[i]==1) {
			createHigh(x, x+3);
		}
	}
	x=(l*4);
	createMed(x, x+3);
}

static void createUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			createHigh(20+x, 20+x+3);
		}
	}
}

static void createState(int state) {
	if(state == 0) {
		createHigh(44,47);
	} else {
		createHigh(40,43);
	}
}

static void createFooter(void) {
	maxitronic->raw[48]=(AVG_PULSE_LENGTH);
	maxitronic->raw[49]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code) {
	const char *id = NULL;
	int unit = -1;
	int state = -1;
	double itmp;
	char *stmp = NULL;

	if(json_find_string(code, "id", &stmp) == 0)
		id = stmp;
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);

	if(id == NULL || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "maxitronic: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id[0] < 'A' || id[0] > 'E') {
		logprintf(LOG_ERR, "maxitronic: invalid id range");
		return EXIT_FAILURE;
	} else if(atoi(&id[1]) < 0 || atoi(&id[1]) > 31) {
		logprintf(LOG_ERR, "maxitronic: invalid id range");
		return EXIT_FAILURE;
	} else if(unit < 0 || unit > 31) {
		logprintf(LOG_ERR, "maxitronic: invalid unit range");
		return EXIT_FAILURE;
	} else {
		createMessage(id, unit, state);
		clearCode();
		createId(id);
		createUnit(unit);
		createState(state);
		createFooter();
		maxitronic->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void maxitronicSwitchInit(void) {

	protocol_register(&maxitronic);
	protocol_set_id(maxitronic, "maxitronic");
	protocol_device_add(maxitronic, "maxitronic", "Maxitronic Switches");
	maxitronic->devtype = SWITCH;
	maxitronic->hwtype = RF433;
	maxitronic->minrawlen = RAW_LENGTH;
	maxitronic->maxrawlen = RAW_LENGTH;
	maxitronic->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	maxitronic->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&maxitronic->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&maxitronic->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&maxitronic->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&maxitronic->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[ABCDEF](3[012]?|[012][0-9]|[0-9]{1})$");

	options_add(&maxitronic->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&maxitronic->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	maxitronic->parseCode=&parseCode;
	maxitronic->createCode=&createCode;
	maxitronic->printHelp=&printHelp;
	maxitronic->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "maxitronic";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	maxitronicSwitchInit();
}
#endif
