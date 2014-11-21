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
#include "toggle.h"
#include "log.h"
#include "devices.h"
#include "../../pilight.h"

static int actionToggleArguments(struct JsonNode *arguments) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jstate1 = NULL;
	struct JsonNode *jstate2 = NULL;	
	double nr1 = 0.0, nr2 = 0.0, nr3 = 0.0;
	char *value1 = NULL, *value2 = NULL, *value3 = NULL;
	jdevice = json_find_member(arguments, "DEVICE");
	jstate1 = json_find_member(arguments, "BETWEEN");
	jstate2 = json_find_member(arguments, "AND");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "toggle action is missing a \"DEVICE\"");
		return -1;
	}
	if(jstate1 == NULL || jstate2 == NULL) {
		logprintf(LOG_ERR, "toggle action is missing a \"BETWEEN ... AND ...\" statement");
		return -1;
	}
	json_find_number(jdevice, "order", &nr1);
	json_find_number(jstate1, "order", &nr2);
	json_find_number(jstate2, "order", &nr3);	
	if((int)nr1 != 1 || (int)nr2 != 2 || (int)nr3 != 3) {
		logprintf(LOG_ERR, "toggle actions are formatted as \"toggle DEVICE ... BETWEEN ... AND ...\"");
		return -1;
	}
	json_find_string(jdevice, "value", &value1);
	json_find_string(jstate1, "value", &value2);
	json_find_string(jstate2, "value", &value3);
	
	struct devices_t *dev = NULL;
	if(devices_get(value1, &dev) == 0) {
		struct protocols_t *tmp = dev->protocols;
		int match1 = 0, match2 = 0;
		while(tmp) {
			struct options_t *opt = tmp->listener->options;
			while(opt) {
				if(opt->conftype == DEVICES_STATE) {
					if(strcmp(opt->name, value2) == 0) {
						match1 = 1;
					}
					if(strcmp(opt->name, value3) == 0) {
						match2 = 1;
					}
					if(match1 == 1 && match2 == 1) {
						break;
					}
				}
				opt = opt->next;
			}
			if(match1 == 1 && match2 == 1) {
				break;
			}
			tmp = tmp->next;
		}
		if(match1 == 0) {
			logprintf(LOG_ERR, "device \"%s\" can't be set to state \"%s\"", value1, value2);
			return -1;
		}
		if(match2 == 0) {
			logprintf(LOG_ERR, "device \"%s\" can't be set to state \"%s\"", value1, value3);
			return -1;
		}
	} else {
		logprintf(LOG_ERR, "device \"%s\" doesn't exists", value1);
		return -1;		
	}
	return 0;
}

static int actionToggleRun(struct JsonNode *arguments) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jstate1 = NULL;
	struct JsonNode *jstate2 = NULL;
	char *device = NULL, *state1 = NULL, *state2 = NULL, *cstate = NULL;
	if((jdevice = json_find_member(arguments, "DEVICE")) != NULL &&
		 (jstate1 = json_find_member(arguments, "BETWEEN")) != NULL &&
		 (jstate2 = json_find_member(arguments, "AND")) != NULL) {
		if(json_find_string(jdevice, "value", &device) == 0 &&
			 json_find_string(jstate1, "value", &state1) == 0 &&
			 json_find_string(jstate2, "value", &state2) == 0) {


			struct devices_t *dev = NULL;
			if(devices_get(device, &dev) == 0) {
				struct devices_settings_t *tmp_settings = dev->settings;
				while(tmp_settings) {
					if(strcmp(tmp_settings->name, "state") == 0) {
						if(tmp_settings->values->type == JSON_STRING) {
								cstate = tmp_settings->values->string_;
								break;
						}
					}
					tmp_settings = tmp_settings->next;
				}
				if(strcmp(state1, cstate) == 0) {
					pilight.control(dev, state2, NULL);
				} else if(strcmp(state2, cstate) == 0) {
					pilight.control(dev, state1, NULL);
				}
			}
		}
	}
	return 0;
}

#ifndef MODULE
__attribute__((weak))
#endif
void actionToggleInit(void) {
	event_action_register(&action_toggle, "toggle");
	
	options_add(&action_toggle->options, 'a', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_toggle->options, 'b', "BETWEEN", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_toggle->options, 'c', "AND", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_toggle->run = &actionToggleRun;
	action_toggle->checkArguments = &actionToggleArguments;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "toggle";
	module->version = "1.0";
	module->reqversion = "5.0";
	*reqcommit = "87";
}

void init(void) {
	actionToggleInit();
}
#endif
