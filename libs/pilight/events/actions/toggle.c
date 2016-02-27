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

#include "../action.h"
#include "../../core/options.h"
#include "../../core/threadpool.h"
#include "../../storage/storage.h"
#include "../../protocols/protocol.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "toggle.h"

static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jbetween = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jschild = NULL;
	struct JsonNode *jdchild = NULL;
	double nr1 = 0.0, nr2 = 0.0;
	int nrvalues = 0;
	jdevice = json_find_member(obj->arguments, "DEVICE");
	jbetween = json_find_member(obj->arguments, "BETWEEN");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "toggle action is missing a \"DEVICE\"");
		return -1;
	}
	if(jbetween == NULL && jbetween->tag != JSON_ARRAY) {
		logprintf(LOG_ERR, "toggle action is missing a \"BETWEEN ... AND ...\" statement");
		return -1;
	}
	json_find_number(jdevice, "order", &nr1);
	json_find_number(jbetween, "order", &nr2);
	if((int)nr1 != 1 || (int)nr2 != 2) {
		logprintf(LOG_ERR, "toggle actions are formatted as \"toggle DEVICE ... BETWEEN ... AND ...\"");
		return -1;
	}
	if((jsvalues = json_find_member(jbetween, "value")) != NULL) {
		jschild = json_first_child(jsvalues);
		while(jschild) {
			nrvalues++;
			jschild = jschild->next;
		}
	}
	if(nrvalues != 2) {
		logprintf(LOG_ERR, "toggle actions are formatted as \"toggle DEVICE ... BETWEEN ... AND ...\"");
		return -1;
	}
	if((jdvalues = json_find_member(jdevice, "value")) != NULL) {
		jdchild = json_first_child(jdvalues);
		while(jdchild) {
			if(jdchild->tag == JSON_STRING) {
				if(devices_select(ORIGIN_ACTION, jdchild->string_, NULL) == 0) {
					if((jsvalues = json_find_member(jbetween, "value")) != NULL) {
						jschild = json_first_child(jsvalues);
						while(jschild) {
							if(jschild->tag == JSON_STRING) {
								int match1 = 0;
								int i = 0;
								struct protocol_t *protocol = NULL;
								while(devices_select_protocol(ORIGIN_CONFIG, jdchild->string_, i++, &protocol) == 0) {
									struct options_t *opt = protocol->options;
									while(opt) {
										if(opt->conftype == DEVICES_STATE) {
											if(strcmp(opt->name, jschild->string_) == 0) {
												match1 = 1;
												break;
											}
										}
										opt = opt->next;
									}
									if(match1 == 1) {
										break;
									}
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
	} else {
		return -1;
	}
	return 0;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct event_action_thread_t *pth = task->userdata;
	struct JsonNode *json = pth->obj->parsedargs;

	struct JsonNode *jbetween = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jstate1 = NULL;
	struct JsonNode *jstate2 = NULL;
	char *cstate = NULL, *state1 = NULL, *state2 = NULL;

	event_action_started(pth);

	if(devices_select_string_setting(ORIGIN_ACTION, pth->device->id, "state", &cstate) != 0) {
		logprintf(LOG_NOTICE, "could not select old state of \"%s\"\n", pth->device->id);
		goto end;
	}

	if((jbetween = json_find_member(json, "BETWEEN")) != NULL) {
			if((jsvalues = json_find_member(jbetween, "value")) != NULL) {
			jstate1 = json_find_element(jsvalues, 0);
			jstate2 = json_find_element(jsvalues, 1);
			if(jstate1 != NULL && jstate2 != NULL &&
				jstate1->tag == JSON_STRING && jstate2->tag == JSON_STRING) {
				state1 = jstate1->string_;
				state2 = jstate2->string_;

				if(pilight.control != NULL) {
					if(strcmp(state1, cstate) == 0) {
						pilight.control(pth->device->id, state2, NULL, ORIGIN_ACTION);
					} else if(strcmp(state2, cstate) == 0) {
						pilight.control(pth->device->id, state1, NULL, ORIGIN_ACTION);
					}
				}
			}
		}
	}

end:
	event_action_stopped(pth);

	return (void *)NULL;
}

static int run(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jbetween = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jdchild = NULL;
	struct device_t *dev = NULL;

	if((jdevice = json_find_member(obj->arguments, "DEVICE")) != NULL &&
		 (jbetween = json_find_member(obj->arguments, "BETWEEN")) != NULL) {
		if((jdvalues = json_find_member(jdevice, "value")) != NULL) {
			jdchild = json_first_child(jdvalues);
			while(jdchild) {
				if(jdchild->tag == JSON_STRING) {
					if(devices_select(ORIGIN_ACTION, jdchild->string_, NULL) == 0) {
						if(devices_select_struct(ORIGIN_ACTION, jdchild->string_, &dev) == 0) {
							event_action_thread_start(dev, action_toggle, thread, obj);
						}
					}
				}
				jdchild = jdchild->next;
			}
		}
	}
	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionToggleInit(void) {
	event_action_register(&action_toggle, "toggle");

	options_add(&action_toggle->options, 'a', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_toggle->options, 'b', "BETWEEN", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_toggle->run = &run;
	action_toggle->checkArguments = &checkArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "toggle";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionToggleInit();
}
#endif
