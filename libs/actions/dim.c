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
#include "dim.h"
#include "devices.h"
#include "log.h"
#include "dso.h"
#include "../../pilight.h"

static int actionDimArguments(struct JsonNode *arguments) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jschild = NULL;
	struct JsonNode *jdchild = NULL;
	double nr1 = 0.0, nr2 = 0.0;
	int nrvalues = 0;
	char *device = NULL, *state = NULL;
	jdevice = json_find_member(arguments, "DEVICE");
	jto = json_find_member(arguments, "TO");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "dim action is missing a \"DEVICE\"");
		return -1;
	}
	if(jto == NULL) {
		logprintf(LOG_ERR, "dim action is missing a \"TO ...\" statement");
		return -1;
	}
	json_find_number(jdevice, "order", &nr1);
	json_find_number(jto, "order", &nr2);
	if((int)nr1 != 1 || (int)nr2 != 2) {
		logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ...\"");
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
		logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ...\"");
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
							if(jschild->tag == JSON_NUMBER) {
								struct protocols_t *tmp_protocols = dev->protocols;
								if(tmp_protocols->listener->devtype == DIMMER) {
									struct devices_settings_t *tmp_settings = dev->settings;
									int match1 = 0, match2 = 0;
									while(tmp_settings) {
										if(strcmp(tmp_settings->name, "dimlevel-maximum") == 0) {
											if(tmp_settings->values->type == JSON_NUMBER && 
												(int)tmp_settings->values->number_ < (int)jschild->number_) {
												logprintf(LOG_ERR, "device \"%s\" can't be set to dimlevel \"%d\"", jdchild->string_, (int)jschild->number_);
												return -1;
											}
											match1 = 1;
										}
										if(strcmp(tmp_settings->name, "dimlevel-minimum") == 0) {
											if(tmp_settings->values->type == JSON_NUMBER && 
												(int)tmp_settings->values->number_ > (int)jschild->number_) {
												logprintf(LOG_ERR, "device \"%s\" can't be set to dimlevel \"%d\"", jdchild->string_, (int)jschild->number_);
												return -1;
											}
											match2 = 1;
										}
										tmp_settings = tmp_settings->next;
									}
									if(match1 == 0 || match2 == 0) {
										while(tmp_protocols) {
											struct options_t *opt = tmp_protocols->listener->options;
											while(opt) {
												if(match2 == 0 && strcmp(opt->name, "dimlevel-maximum") == 0 && 
													opt->vartype == JSON_NUMBER && (int)(intptr_t)opt->def < (int)jschild->number_) {
													logprintf(LOG_ERR, "device \"%s\" can't be set to dimlevel \"%d\"", jdchild->string_, (int)jschild->number_);
													return -1;
												}
												if(match1 == 0 && strcmp(opt->name, "dimlevel-minimum") == 0 && 
													opt->vartype == JSON_NUMBER && (int)(intptr_t)opt->def > (int)jschild->number_) {
													logprintf(LOG_ERR, "device \"%s\" can't be set to dimlevel \"%d\"", jdchild->string_, (int)jschild->number_);
													return -1;
												}
												opt = opt->next;
											}
											tmp_protocols = tmp_protocols->next;
										}
									} else {
										return -1;
									}
								} else {
									logprintf(LOG_ERR, "device \"%s\" doesn't support dimming", jdchild->string_);
									return -1;
								}
							} else {
								logprintf(LOG_ERR, "device \"%s\" doesn't exists", jdchild->string_);
								return -1;
							}
						jschild = jschild->next;
						}
					} else {
						return -1;
					}
				} else {
					return -1;
				}
			} else {
				return -1;
			}
			jdchild = jdchild->next;
		}
	} else {
		return -1;
	}
	return 0;
}

static int actionDimRun(struct JsonNode *arguments) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jdchild = NULL;
	struct JsonNode *jdimlevel = NULL;
	double dimlevel = 0.0;
	char state[3];
	if((jdevice = json_find_member(arguments, "DEVICE")) != NULL &&
		 (jto = json_find_member(arguments, "TO")) != NULL) {
		if((jdvalues = json_find_member(jdevice, "value")) != NULL) {
			jdchild = json_first_child(jdvalues);
			while(jdchild) {
				if(jdchild->tag == JSON_STRING) {
					struct devices_t *dev = NULL;
					if(devices_get(jdchild->string_, &dev) == 0) {
						if((jsvalues = json_find_member(jto, "value")) != NULL) {
							jdimlevel = json_find_element(jsvalues, 0);
							if(jdimlevel != NULL && jdimlevel->tag == JSON_NUMBER) {
								dimlevel = (int)jdimlevel->number_;
								strcpy(state, "on");
								JsonNode *jvalues = json_mkobject();
								json_append_member(jvalues, "dimlevel", json_mknumber(dimlevel, 0));
								pilight.control(dev, state, json_first_child(jvalues));
								json_delete(jvalues);
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
void actionDimInit(void) {
	event_action_register(&action_dim, "dim");

	options_add(&action_dim->options, 'a', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_dim->options, 'b', "TO", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	action_dim->run = &actionDimRun;
	action_dim->checkArguments = &actionDimArguments;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "dim";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	actionDimInit();
}
#endif
