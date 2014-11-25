/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include <unistd.h>

#include "action.h"
#include "options.h"
#include "switch.h"
#include "devices.h"
#include "log.h"
#include "dso.h"
#include "../../pilight.h"

static int actionSwitchArguments(struct JsonNode *arguments) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jstate = NULL;
	double nr1 = 0.0, nr2 = 0.0;
	char *value1 = NULL, *value2 = NULL;
	jdevice = json_find_member(arguments, "DEVICE");
	jstate = json_find_member(arguments, "TO");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "switch action is missing a \"DEVICE\"");
		return -1;
	}
	if(jstate == NULL) {
		logprintf(LOG_ERR, "switch action is missing a \"TO ...\" statement");
		return -1;
	}
	json_find_number(jdevice, "order", &nr1);
	json_find_number(jstate, "order", &nr2);
	if((int)nr1 != 1 || (int)nr2 != 2) {
		logprintf(LOG_ERR, "switch actions are formatted as \"switch DEVICE ... TO ...\"");
		return -1;
	}
	json_find_string(jdevice, "value", &value1);
	json_find_string(jstate, "value", &value2);

	struct devices_t *dev = NULL;
	if(devices_get(value1, &dev) == 0) {
		struct protocols_t *tmp = dev->protocols;
		int match1 = 0;
		while(tmp) {
			struct options_t *opt = tmp->listener->options;
			while(opt) {
				if(opt->conftype == DEVICES_STATE) {
					if(strcmp(opt->name, value2) == 0) {
						match1 = 1;
						break;
					}
				}
				opt = opt->next;
			}
			if(match1 == 1) {
				break;
			}
			tmp = tmp->next;
		}
		if(match1 == 0) {
			logprintf(LOG_ERR, "device \"%s\" can't be set to state \"%s\"", value1, value2);
			return -1;
		}
	} else {
		logprintf(LOG_ERR, "device \"%s\" doesn't exists", value1);
		return -1;
	}
	return 0;
}

static int actionSwitchRun(struct JsonNode *arguments) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jstate = NULL;
	char *device = NULL, *state = NULL;
	if((jdevice = json_find_member(arguments, "DEVICE")) != NULL &&
		 (jstate = json_find_member(arguments, "TO")) != NULL) {
		if(json_find_string(jdevice, "value", &device) == 0 &&
			 json_find_string(jstate, "value", &state) == 0) {

			struct devices_t *dev = NULL;
			if(devices_get(device, &dev) == 0) {
				pilight.control(dev, state, NULL);
			}
		}
	}
	return 0;
}

#ifndef MODULE
__attribute__((weak))
#endif
void actionSwitchInit(void) {
	event_action_register(&action_switch, "switch");

	options_add(&action_switch->options, 'a', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_switch->options, 'b', "TO", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_switch->run = &actionSwitchRun;
	action_switch->checkArguments = &actionSwitchArguments;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "switch";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	actionSwitchInit();
}
#endif
