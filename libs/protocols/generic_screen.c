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
#include "generic_screen.h"

static void genScreenCreateMessage(int id, int state) {
	generic_screen->message = json_mkobject();
	json_append_member(generic_screen->message, "id", json_mknumber(id));
	if(state == 1) {
		json_append_member(generic_screen->message, "state", json_mkstring("up"));
	} else {
		json_append_member(generic_screen->message, "state", json_mkstring("down"));
	}
}

static int genScreenCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "down", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "up", &itmp) == 0)
		state=1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "generic_screen: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		genScreenCreateMessage(id, state);
	}

	return EXIT_SUCCESS;
}

static void genScreenPrintHelp(void) {
	printf("\t -t --up\t\t\tsend an up signal\n");
	printf("\t -f --down\t\t\tsend an down signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void genScreenInit(void) {

	protocol_register(&generic_screen);
	protocol_set_id(generic_screen, "generic_screen");
	protocol_device_add(generic_screen, "generic_screen", "Generic Screens");
	generic_screen->devtype = SCREEN;

	options_add(&generic_screen->options, 't', "up", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&generic_screen->options, 'f', "down", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&generic_screen->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,})$");

	options_add(&generic_screen->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	generic_screen->printHelp=&genScreenPrintHelp;
	generic_screen->createCode=&genScreenCreateCode;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "generic_screen";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	genScreenInit();
}
#endif
