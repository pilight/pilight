/*
	Copyright (C) 2013 - 2016 CurlyMo

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
#include "generic_switch.h"

static int createCode(struct JsonNode *code, char **message) {
	int id = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "generic_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		int x = snprintf((*message), 255, "{");
		x += snprintf(&(*message)[x], 255-x, "\"id\":%d,", id);
		if(state == 1) {
			x += snprintf(&(*message)[x], 255-x, "\"state\":\"on\"");
		}	else {
			x += snprintf(&(*message)[x], 255-x, "\"state\":\"off\"");
		}
		x += snprintf(&(*message)[x], 255-x, "}");
	}

	return EXIT_SUCCESS;

}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void genericSwitchInit(void) {

	protocol_register(&generic_switch);
	protocol_set_id(generic_switch, "generic_switch");
	protocol_device_add(generic_switch, "generic_switch", "Generic Switches");
	generic_switch->devtype = SWITCH;

	options_add(&generic_switch->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&generic_switch->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&generic_switch->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,})$");

	options_add(&generic_switch->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&generic_switch->options, "0", "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	generic_switch->printHelp=&printHelp;
	generic_switch->createCode=&createCode;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "generic_switch";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	genericSwitchInit();
}
#endif
