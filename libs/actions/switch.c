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
	struct JsonNode *jto = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jschild = NULL;
	struct JsonNode *jdchild = NULL;
	double nr1 = 0.0, nr2 = 0.0;
	int nrvalues = 0;
	jdevice = json_find_member(arguments, "DEVICE");
	jto = json_find_member(arguments, "TO");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "switch action is missing a \"DEVICE\"");
		return -1;
	}
	if(jto == NULL) {
		logprintf(LOG_ERR, "switch action is missing a \"TO ...\" statement");
		return -1;
	}
	json_find_number(jdevice, "order", &nr1);
	json_find_number(jto, "order", &nr2);
	if((int)nr1 != 1 || (int)nr2 != 2) {
		logprintf(LOG_ERR, "switch actions are formatted as \"switch DEVICE ... TO ...\"");
		return -1;
	}
	if((jsvalues = json_find_member(jto, "value")) != NULL) {
		jschild = json_first_child(jsvalues);
		while(jschild) {
			nrvalues++;
			jschild = jschild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "switch actions are formatted as \"switch DEVICE ... TO ...\"");
		return -1;	
	}
	if((jdvalues = json_find_member(jdevice, "value")) != NULL) {
		jdchild = json_first_child(jdvalues);
		while(jdchild) {
			if(jdchild->tag == JSON_STRING) {
				struct devices_t *dev = NULL;
				if(devices_get(jdchild->string_, &dev) == 0) {
					if((jsvalues = json_find_member(jto, "value")) != NULL) {
						jschild = json_first_child(jsvalues);
						while(jschild) {
							if(jschild->tag == JSON_STRING) {
									struct protocols_t *tmp = dev->protocols;
									int match1 = 0;
									while(tmp) {
										struct options_t *opt = tmp->listener->options;
										while(opt) {
											if(opt->conftype == DEVICES_STATE) {
												if(strcmp(opt->name, jschild->string_) == 0) {
													match1 = 1;
													break;
												}
											}
											opt = opt->next;
										}
										tmp = tmp->next;
									}
									if(match1 == 0) {
										logprintf(LOG_ERR, "device \"%s\" can't be set to state \"%s\"", jdchild->string_, jschild->string_);
										return -1;
									}
								} else {
									return -1;
								}
							jschild = jschild->next;
						}
					} else {
						return -1;
					}
				} else {
					logprintf(LOG_ERR, "device \"%s\" doesn't exists", jdchild->string_);
					return -1;
				}
			} else {
				return -1;
			}
			jdchild = jdchild->next;
		}
	}
	return 0;
}

static int actionSwitchRun(struct JsonNode *arguments) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jdchild = NULL;
	struct JsonNode *jstate = NULL;
	char *state = NULL;
	if((jdevice = json_find_member(arguments, "DEVICE")) != NULL &&
		 (jto = json_find_member(arguments, "TO")) != NULL) {
		if((jdvalues = json_find_member(jdevice, "value")) != NULL) {
			jdchild = json_first_child(jdvalues);
			while(jdchild) {
				if(jdchild->tag == JSON_STRING) {
					struct devices_t *dev = NULL;
					if(devices_get(jdchild->string_, &dev) == 0) {
						if((jsvalues = json_find_member(jto, "value")) != NULL) {
							jstate = json_find_element(jsvalues, 0);
							if(jstate != NULL && jstate->tag == JSON_STRING) {
								 state = jstate->string_;
								 
								 pilight.control(dev, state, NULL);
							}
						}
					}
				}
				jdchild = jdchild->next;
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
