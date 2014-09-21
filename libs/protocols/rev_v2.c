/*
	Copyright (C) 2014 CurlyMo & neevedr

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
#include "rev_v2.h"

static void rev2CreateMessage(char *id, int unit, int state) {
	rev2_switch->message = json_mkobject();
	json_append_member(rev2_switch->message, "id", json_mkstring(id));
	json_append_member(rev2_switch->message, "unit", json_mknumber(unit));
	if(state == 2)
		json_append_member(rev2_switch->message, "state", json_mkstring("on"));
	else
		json_append_member(rev2_switch->message, "state", json_mkstring("off"));
}

static void rev2ParseCode(void) {
	int x = 0;
	int z = 65;
	char id[3] = {'\0'};

	/* Convert the one's and zero's into binary */
	for(x=0; x<rev2_switch->rawlen; x+=4) {
		if(rev2_switch->code[x+3] == 1) {
			rev2_switch->binary[x/4]=1;
		} else if(rev2_switch->code[x+0] == 1) {
			rev2_switch->binary[x/4]=2;
		} else {
			rev2_switch->binary[x/4]=0;
		}
	}

	for(x=9;x>=5;--x) {
		if(rev2_switch->binary[x] == 2) {
			break;
		}
		z++;
	}

	int unit = binToDecRev(rev2_switch->binary, 0, 5);
	int state = rev2_switch->binary[11];
	int y = binToDecRev(rev2_switch->binary, 6, 9);
	sprintf(&id[0], "%c%d", z, y);

	rev2CreateMessage(id, unit, state);
}

static void rev2CreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		rev2_switch->raw[i]=(rev2_switch->plslen->length);
		rev2_switch->raw[i+1]=(rev2_switch->pulse*rev2_switch->plslen->length);
		rev2_switch->raw[i+2]=(rev2_switch->pulse*rev2_switch->plslen->length);
		rev2_switch->raw[i+3]=(rev2_switch->plslen->length);
	}
}

static void rev2CreateMed(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		rev2_switch->raw[i]=(rev2_switch->pulse*rev2_switch->plslen->length);
		rev2_switch->raw[i+1]=(rev2_switch->plslen->length);
		rev2_switch->raw[i+2]=(rev2_switch->pulse*rev2_switch->plslen->length);
		rev2_switch->raw[i+3]=(rev2_switch->plslen->length);
	}
}

static void rev2CreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		rev2_switch->raw[i]=(rev2_switch->plslen->length);
		rev2_switch->raw[i+1]=(rev2_switch->pulse*rev2_switch->plslen->length);
		rev2_switch->raw[i+2]=(rev2_switch->plslen->length);
		rev2_switch->raw[i+3]=(rev2_switch->pulse*rev2_switch->plslen->length);
	}
}

static void rev2ClearCode(void) {
	rev2CreateMed(0,4);
	rev2CreateLow(4,47);
}

static void rev2CreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=i*4;
			rev2CreateHigh(23-(x+3), 23-x);
		}
	}
}

static void rev2CreateId(char *id) {
	int l = ((int)(id[0]))-65;
	int y = atoi(&id[1]);
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBinRev(y, binary);
	for(i=0;i<=length;i++) {
		x=i*4;
		if(binary[i]==1) {
			rev2CreateHigh(39-(x+3), 39-x);
		}
	}
	x=(l*4);
	rev2CreateMed(39-(x+3), 39-x);
}

static void rev2CreateState(int state) {
	if(state == 0) {
		rev2CreateMed(40,43);
		rev2CreateHigh(44,47);
	} else {
		rev2CreateHigh(40,43);
		rev2CreateMed(44,47);
	}
}

static void rev2CreateFooter(void) {
	rev2_switch->raw[48]=(rev2_switch->plslen->length);
	rev2_switch->raw[49]=(PULSE_DIV*rev2_switch->plslen->length);
}

static int rev2CreateCode(JsonNode *code) {
	char id[3] = {'\0'};
	int unit = -1;
	int state = -1;
	double itmp = -1;
	char *stmp;

	strcpy(id, "-1");

	if(json_find_string(code, "id", &stmp) == 0)
		strcpy(id, stmp);

	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);

	if(strcmp(id, "-1") == 0 || unit == -1 || state == -1) {
		logprintf(LOG_ERR, "rev2_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if((int)(id[0]) < 65 || (int)(id[0]) > 70) {
		logprintf(LOG_ERR, "rev2_switch: invalid id range");
		return EXIT_FAILURE;
	} else if(atoi(&id[1]) < 0 || atoi(&id[1]) > 31) {
		logprintf(LOG_ERR, "rev2_switch: invalid id range");
		return EXIT_FAILURE;
	} else if(unit > 63 || unit < 0) {
		logprintf(LOG_ERR, "rev2_switch: invalid unit range");
		return EXIT_FAILURE;
	} else {
		rev2CreateMessage(id, unit, ((state == 2 || state == 1) ? 2 : 0));
		rev2ClearCode();
		rev2CreateUnit(unit);
		rev2CreateId(id);
		rev2CreateState(state);
		rev2CreateFooter();
	}
	return EXIT_SUCCESS;
}

static void rev2PrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void rev2Init(void) {

	protocol_register(&rev2_switch);
	protocol_set_id(rev2_switch, "rev2_switch");
	protocol_device_add(rev2_switch, "rev2_switch", "Rev Switches v2");
	protocol_plslen_add(rev2_switch, 258);
	protocol_plslen_add(rev2_switch, 186);
	protocol_plslen_add(rev2_switch, 172);
	rev2_switch->devtype = SWITCH;
	rev2_switch->hwtype = RF433;
	rev2_switch->pulse = 3;
	rev2_switch->rawlen = 50;
	rev2_switch->binlen = 12;

	options_add(&rev2_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&rev2_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&rev2_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]|[1-5][0-9]|6[0-3])$");
	options_add(&rev2_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, "^[ABCDEF](3[012]?|[012][0-9]|[0-9]{1})$");

	options_add(&rev2_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	rev2_switch->parseCode=&rev2ParseCode;
	rev2_switch->createCode=&rev2CreateCode;
	rev2_switch->printHelp=&rev2PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "rev2_switch";
	module->version = "0.8";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	rev2Init();
}
#endif
