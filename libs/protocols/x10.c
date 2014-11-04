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
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "x10.h"

static char x10letters[18] = {"MNOPCDABEFGHKL IJ"};

static void x10CreateMessage(char *id, int state) {
	x10->message = json_mkobject();
	json_append_member(x10->message, "id", json_mkstring(id));
	if(state == 0) {
		json_append_member(x10->message, "state", json_mkstring("on"));
	} else {
		json_append_member(x10->message, "state", json_mkstring("off"));
	}
}

static void x10ParseCode(void) {
	int x = 0;
	int y = 0;

	for(x=1;x<x10->rawlen-1;x+=2) {
		x10->binary[y++] = x10->code[x];
	}
	char id[3];
	int l = x10letters[binToDecRev(x10->binary, 0, 3)];
	int s = x10->binary[18];
	int i = 1;
	int c1 = (binToDec(x10->binary, 0, 7)+binToDec(x10->binary, 8, 15));
	int c2 = (binToDec(x10->binary, 16, 23)+binToDec(x10->binary, 24, 31));
	if(x10->binary[5] == 1) {
		i += 8;
	}
	if(x10->binary[17] == 1) {
		i += 4;
	}
	i += binToDec(x10->binary, 19, 20);
	if(c1 == 255 && c2 == 255) {
		sprintf(id, "%c%d", l, i);
		x10CreateMessage(id, s);
	}
}

static void x10CreateLow(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		x10->raw[i]=(x10->plslen->length*(int)round(x10->pulse/3));
		x10->raw[i+1]=(x10->plslen->length*(int)round(x10->pulse/3));
	}
}

static void x10CreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=2) {
		x10->raw[i]=(x10->plslen->length*(int)round(x10->pulse/3));
		x10->raw[i+1]=(x10->pulse*x10->plslen->length);
	}
}

static void x10ClearCode(void) {
	x10CreateLow(0, 15);
	x10CreateHigh(16, 31);
	x10CreateLow(32, 47);
	x10CreateHigh(48, 63);
}

static void x10CreateLetter(int l) {
	int binary[255];
	int length = 0;
	int i=0, x=0, y = 0;

	for(i=0;i<17;i++) {
		if((int)x10letters[i] == l) {
			length = decToBinRev(i, binary);
			for(x=0;x<=length;x++) {
				if(binary[x]==1) {
					y=x*2;
					x10CreateHigh(7-(y+1),7-y);
					x10CreateLow(23-(y+1),23-y);
				}
			}
			break;
		}
	}
}

static void x10CreateNumber(int n) {
	if(n >= 8) {
		x10CreateHigh(10, 10);
		x10CreateLow(26, 26);
		n -= 8;
	}
	if(n > 4) {
		x10CreateHigh(34, 34);
		x10CreateLow(50, 50);
		n -= 4;
	}
	if(n == 2 || n == 4) {
		x10CreateHigh(38, 38);
		x10CreateLow(54, 54);
	}
	if(n == 3 || n == 4) {
		x10CreateHigh(40, 40);
		x10CreateLow(56, 56);
	}
}

static void x10CreateState(int state) {
	if(state == 0) {
		x10CreateHigh(36, 36);
		x10CreateLow(52, 52);
	}
}

static void x10CreateFooter(void) {
	x10->raw[64]=(x10->plslen->length*(int)round(x10->pulse/3));
	x10->raw[65]=(PULSE_DIV*x10->plslen->length*9);
	x10->raw[66]=(PULSE_DIV*x10->plslen->length*2);
	x10->raw[67]=(PULSE_DIV*x10->plslen->length);
}

static int x10CreateCode(JsonNode *code) {
	char id[4] = {'\0'};
	int state = -1;
	double itmp = -1;
	char *stmp = NULL;

	strcpy(id, "-1");

	if(json_find_string(code, "id", &stmp) == 0)
		strcpy(id, stmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=0;

	if(strcmp(id, "-1") == 0 || state == -1) {
		logprintf(LOG_ERR, "x10: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if((int)(id[0]) < 65 || (int)(id[0]) > 80) {
		logprintf(LOG_ERR, "x10: invalid id range");
		return EXIT_FAILURE;
	} else if(atoi(&id[1]) < 0 || atoi(&id[1]) > 16) {
		logprintf(LOG_ERR, "x10: invalid id range");
		return EXIT_FAILURE;
	} else {
		x10CreateMessage(id, state);
		x10ClearCode();
		x10CreateLetter((int)id[0]);
		x10CreateNumber(atoi(&id[1]));
		x10CreateState(state);
		x10CreateFooter();
	}
	return EXIT_SUCCESS;
}

static void x10PrintHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void x10Init(void) {
	protocol_register(&x10);
	protocol_set_id(x10, "x10");
	protocol_device_add(x10, "x10", "x10 based devices");
	protocol_plslen_add(x10, 150);
	x10->devtype = SWITCH;
	x10->hwtype = RF433;
	x10->pulse = 12;
	x10->rawlen = 68;

	options_add(&x10->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&x10->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&x10->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, "^[ABCDEFGHIJKLMNOP]([1][0-6]{1}|[1-9]{1})$");

	options_add(&x10->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	x10->parseCode=&x10ParseCode;
	x10->createCode=&x10CreateCode;
	x10->printHelp=&x10PrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "x10";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	x10Init();
}
#endif
