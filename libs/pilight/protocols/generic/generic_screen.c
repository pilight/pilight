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

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "generic_screen.h"

static int createCode(struct JsonNode *code, char *message) {
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
		int x = snprintf(message, 255, "{");
		x += snprintf(&message[x], 255-x, "\"id\":%d,", id);
		if(state == 1) {
			x += snprintf(&message[x], 255-x, "\"state\":\"up\"");
		}	else {
			x += snprintf(&message[x], 255-x, "\"state\":\"down\"");
		}
		x += snprintf(&message[x], 255-x, "}");
	}

	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --up\t\t\tsend an up signal\n");
	printf("\t -f --down\t\t\tsend an down signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void genericScreenInit(void) {

	protocol_register(&generic_screen);
	protocol_set_id(generic_screen, "generic_screen");
	protocol_device_add(generic_screen, "generic_screen", "Generic Screens");
	generic_screen->devtype = SCREEN;

	options_add(&generic_screen->options, 't', "up", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&generic_screen->options, 'f', "down", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&generic_screen->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,})$");

	options_add(&generic_screen->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&generic_screen->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	generic_screen->printHelp=&printHelp;
	generic_screen->createCode=&createCode;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "generic_screen";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	genericScreenInit();
}
#endif
