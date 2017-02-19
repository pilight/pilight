/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

static void *reason_control_device_free(void *param) {
	struct reason_control_device_t *data = param;
	FREE(data->state);
	FREE(data->dev);
	FREE(data);
	return NULL;
}

static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jbetween = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jschild = NULL;
	struct JsonNode *jdchild = NULL;
	double nr1 = 0.0, nr2 = 0.0;
	int nrvalues = 0;

	if(obj == NULL) {
		return -1;
	}

	if(obj->parsedargs == NULL) {
		return -1;
	}

	jdevice = json_find_member(obj->parsedargs, "DEVICE");
	jbetween = json_find_member(obj->parsedargs, "BETWEEN");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "toggle action is missing a \"DEVICE\"");
		return -1;
	}

	if(jdevice->tag != JSON_OBJECT) {
		/* Internal error */
		return -1;
	}

	if(jbetween == NULL) {
		logprintf(LOG_ERR, "toggle action is missing a \"BETWEEN ... AND ...\" statement");
		return -1;
	}

	if(jbetween->tag != JSON_OBJECT) {
		/* Internal error */
		return -1;
	}

	json_find_number(jdevice, "order", &nr1);
	json_find_number(jbetween, "order", &nr2);
	if((int)nr1 != 1 || (int)nr2 != 2) {
		logprintf(LOG_ERR, "toggle actions are formatted as \"toggle DEVICE ... BETWEEN ... AND ...\"");
		return -1;
	}

	if((jsvalues = json_find_member(jbetween, "value")) != NULL) {
		if(jsvalues->tag == JSON_ARRAY) {
			jschild = json_first_child(jsvalues);
			while(jschild) {
				nrvalues++;
				jschild = jschild->next;
			}
		} else {
			/* Internal error */
			return -1;
		}
	}

	if(nrvalues != 2) {
		logprintf(LOG_ERR, "toggle actions are formatted as \"toggle DEVICE ... BETWEEN ... AND ...\"");
		return -1;
	}

	if((jdvalues = json_find_member(jdevice, "value")) != NULL) {
		if(jdvalues->tag == JSON_ARRAY) {
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
										logprintf(LOG_ERR, "device \"%s\" cannot be set to state \"%s\"", jdchild->string_, jschild->string_);
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
						logprintf(LOG_ERR, "device \"%s\" does not exist", jdchild->string_);
						return -1;
					}
				} else {
					return -1;
				}
				jdchild = jdchild->next;
			}
		} else {
			/* Internal error */
			return -1;
		}
	}

	return 0;
}

static void prepare(struct rules_actions_t *obj, char *dev) {
	struct JsonNode *json = obj->parsedargs;

	struct JsonNode *jbetween = NULL;
	struct JsonNode *jsvalues = NULL;
	struct JsonNode *jstate1 = NULL;
	struct JsonNode *jstate2 = NULL;
	char *cstate = NULL, *state1 = NULL, *state2 = NULL;

	if(devices_select_string_setting(ORIGIN_ACTION, dev, "state", &cstate) != 0) {
		logprintf(LOG_NOTICE, "could not select old state of \"%s\"", dev);
	} else {
		if((jbetween = json_find_member(json, "BETWEEN")) != NULL) {
				if((jsvalues = json_find_member(jbetween, "value")) != NULL) {
				jstate1 = json_find_element(jsvalues, 0);
				jstate2 = json_find_element(jsvalues, 1);
				if(jstate1 != NULL && jstate2 != NULL &&
					jstate1->tag == JSON_STRING && jstate2->tag == JSON_STRING) {
					state1 = jstate1->string_;
					state2 = jstate2->string_;

					struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
					if(data1 == NULL) {
						OUT_OF_MEMORY
					}
					data1->dev = STRDUP(dev);
					data1->values = NULL;
					if(strcmp(state1, cstate) == 0) {
						data1->state = STRDUP(state2);
					} else if(strcmp(state2, cstate) == 0) {
						data1->state = STRDUP(state1);
					}
					eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);
				}
			}
		}
	}

	return;
}

static int run(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jbetween = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jdchild = NULL;
	struct device_t *dev = NULL;

	if((jdevice = json_find_member(obj->parsedargs, "DEVICE")) != NULL &&
		 (jbetween = json_find_member(obj->parsedargs, "BETWEEN")) != NULL) {
		if((jdvalues = json_find_member(jdevice, "value")) != NULL) {
			jdchild = json_first_child(jdvalues);
			while(jdchild) {
				if(jdchild->tag == JSON_STRING) {
					if(devices_select(ORIGIN_ACTION, jdchild->string_, NULL) == 0) {
						if(devices_select_struct(ORIGIN_ACTION, jdchild->string_, &dev) == 0) {
							prepare(obj, jdchild->string_);
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
	module->version = "3.1";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionToggleInit();
}
#endif
