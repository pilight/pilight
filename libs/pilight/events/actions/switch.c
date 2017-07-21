/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../action.h"
#include "../events.h"
#include "../../core/options.h"
#include "../../storage/storage.h"
#include "../../protocols/protocol.h"
#include "../../core/log.h"
#include "../../core/pilight.h"

#include "../../../libuv/uv.h"

#include "switch.h"

#define INIT		0
#define EXECUTE	1
#define RESTORE	2

typedef struct data_t {
	char *device;
	char old_state[4];
	char new_state[4];
	int steps;
	int seconds_for;
	int type_for;
	int seconds_after;
	int type_after;
	unsigned long exec;

	struct event_action_thread_t *pth;
} data_t;

static struct units_t {
	char name[255];
	int id;
} units[] = {
	{ "MILLISECOND", 	1 },
	{ "SECOND", 2 },
	{ "MINUTE", 3 },
	{ "HOUR", 4 },
	{ "DAY", 5 }
};

static void *reason_control_device_free(void *param) {
	struct reason_control_device_t *data = param;
	FREE(data->state);
	FREE(data->dev);
	FREE(data);
	return NULL;
}

static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jfor = NULL;
	struct JsonNode *jafter = NULL;
	struct JsonNode *javalues = NULL;
	struct JsonNode *jbvalues = NULL;
	struct JsonNode *jcvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jachild = NULL;
	struct JsonNode *jbchild = NULL;
	struct JsonNode *jcchild = NULL;
	struct JsonNode *jdchild = NULL;
	char *state = NULL, **array = NULL;
	double nr1 = 0.0, nr2 = 0.0, nr3 = 0.0, nr4 = 0.0;
	int nrvalues = 0, l = 0, i = 0, match = 0;
	int	nrunits = (sizeof(units)/sizeof(units[0]));

	if(obj == NULL) {
		/* Internal error */
		return -1;
	}

	if(obj->parsedargs == NULL) {
		/* Internal error */
		return -1;
	}

	jdevice = json_find_member(obj->parsedargs, "DEVICE");
	jto = json_find_member(obj->parsedargs, "TO");
	jfor = json_find_member(obj->parsedargs, "FOR");
	jafter = json_find_member(obj->parsedargs, "AFTER");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "switch action is missing a \"DEVICE\" statement");
		return -1;
	}

	if(jdevice->tag != JSON_OBJECT) {
		/* Internal error */
		return -1;
	}

	if(jto == NULL) {
		logprintf(LOG_ERR, "switch action is missing a \"TO ...\" statement");
		return -1;
	}

	if(jto->tag != JSON_OBJECT) {
		/* Internal error */
		return -1;
	}

	json_find_number(jdevice, "order", &nr1);
	json_find_number(jto, "order", &nr2);

	if(jfor != NULL) {
		json_find_number(jfor, "order", &nr3);
		if(nr3 < nr2) {
			logprintf(LOG_ERR, "switch actions are formatted as \"switch DEVICE ... TO ... FOR ...\"");
			return -1;
		}

		if(jfor->tag != JSON_OBJECT) {
			/* Internal error */
			return -1;
		}
	}

	if(jafter != NULL) {
		json_find_number(jafter, "order", &nr4);
		if(nr4 < nr2) {
			logprintf(LOG_ERR, "switch actions are formatted as \"switch DEVICE ... TO ... AFTER ...\"");
			return -1;
		}

		if(jafter->tag != JSON_OBJECT) {
			/* Internal error */
			return -1;
		}
	}

	if((int)nr1 != 1 || (int)nr2 != 2) {
		logprintf(LOG_ERR, "switch actions are formatted as \"switch DEVICE ... TO ...\"");
		return -1;
	}

	nrvalues = 0;
	if((javalues = json_find_member(jto, "value")) != NULL) {
		if(javalues->tag == JSON_ARRAY) {
			jachild = json_first_child(javalues);
			while(jachild) {
				nrvalues++;
				jachild = jachild->next;
			}
		} else {
			/* Internal error */
			return -1;
		}
	}

	if(nrvalues > 1) {
		logprintf(LOG_ERR, "switch action \"TO\" only takes one argument");
		return -1;
	}

	nrvalues = 0;
	if(jfor != NULL) {
		if((jcvalues = json_find_member(jfor, "value")) != NULL) {
			if(jcvalues->tag == JSON_ARRAY) {
				jcchild = json_first_child(jcvalues);
				while(jcchild) {
					nrvalues++;
					if(jcchild->tag == JSON_STRING) {
						l = explode(jcchild->string_, " ", &array);
						if(l == 2) {
							match = 0;
							for(i=0;i<nrunits;i++) {
								if(strcmp(array[1], units[i].name) == 0) {
									match = 1;
									if(isNumeric(array[0]) != 0 || atoi(array[0]) <= 0) {
										logprintf(LOG_ERR, "switch action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
										array_free(&array, l);
										return -1;
									}
									break;
								}
							}
							if(match == 0) {
								logprintf(LOG_ERR, "switch action \"%s\" is not a valid unit", array[1]);
								array_free(&array, l);
								return -1;
							}
						} else {
							logprintf(LOG_ERR, "switch action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
							if(l > 0) {
								array_free(&array, l);
							}
							return -1;
						}
						array_free(&array, l);
					}
					jcchild = jcchild->next;
				}
			}
		} else {
			/* Internal error */
			return -1;
		}
		if(nrvalues != 1) {
			logprintf(LOG_ERR, "switch action \"FOR\" only takes one argument");
			return -1;
		}
	}

	nrvalues = 0;
	if(jafter != NULL) {
		if((jdvalues = json_find_member(jafter, "value")) != NULL) {
			if(jdvalues->tag == JSON_ARRAY) {
				jdchild = json_first_child(jdvalues);
				while(jdchild) {
					nrvalues++;
					if(jdchild->tag == JSON_STRING) {
						l = explode(jdchild->string_, " ", &array);
						if(l == 2) {
							match = 0;
							for(i=0;i<nrunits;i++) {
								if(strcmp(array[1], units[i].name) == 0) {
									match = 1;
									if(isNumeric(array[0]) != 0 || atoi(array[0]) <= 0) {
										logprintf(LOG_ERR, "switch action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
										array_free(&array, l);
										return -1;
									}
									break;
								}
							}
							if(match == 0) {
								logprintf(LOG_ERR, "switch action \"%s\" is not a valid unit", array[1]);
								array_free(&array, l);
								return -1;
							}
						} else {
							logprintf(LOG_ERR, "switch action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
							if(l > 0) {
								array_free(&array, l);
							}
							return -1;
						}
						array_free(&array, l);
					}
					jdchild = jdchild->next;
				}
			}
		} else {
			/* Internal error */
			return -1;
		}
		if(nrvalues != 1) {
			logprintf(LOG_ERR, "switch action \"AFTER\" only takes one argument");
			return -1;
		}
	}

	if((jbvalues = json_find_member(jdevice, "value")) != NULL) {
		if(jbvalues->tag == JSON_ARRAY) {
			jbchild = json_first_child(jbvalues);
			while(jbchild) {
				if(jbchild->tag == JSON_STRING) {
					if(devices_select(ORIGIN_ACTION, jbchild->string_, NULL) == 0) {
						if((javalues = json_find_member(jto, "value")) != NULL) {
							jachild = json_first_child(javalues);
							while(jachild) {
								if(jachild->tag == JSON_STRING) {
									state = jachild->string_;
									int match1 = 0;
									struct protocol_t *protocol = NULL;
									i = 0;
									while(devices_select_protocol(ORIGIN_ACTION, jbchild->string_, i++, &protocol) == 0) {
										struct options_t *opt = protocol->options;
										while(opt) {
											if(opt->conftype == DEVICES_STATE) {
												if(strcmp(opt->name, state) == 0) {
													match1 = 1;
													break;
												}
											}
											opt = opt->next;
										}
										if(match == 1) {
											break;
										}
									}
									if(match1 == 0) {
										logprintf(LOG_ERR, "device \"%s\" cannot be set to state \"%s\"", jbchild->string_, state);
										return -1;
									}
								} else {
									return -1;
								}
								jachild = jachild->next;
							}
						} else {
							return -1;
						}
					} else {
						logprintf(LOG_ERR, "device \"%s\" does not exist", jbchild->string_);
						return -1;
					}
				} else {
					return -1;
				}
				jbchild = jbchild->next;
			}
		} else {
			/* Internal error */
			return -1;
		}
	}

	return 0;
}

static void thread(uv_work_t *req) {
	struct data_t *data = req->data;
	unsigned long exec = 0;

	/*
	 * Check if we are the last action for this device
	 * or else skip execution.
	 */
	if(event_action_get_execution_id(data->device, &exec) == 0) {
		if(exec != data->exec) {
			logprintf(LOG_DEBUG, "skipping overridden action %s for device %s", action_switch->name, data->device);
			goto close;
		}
	}

	if(strcmp(data->old_state, data->new_state) == 0) {
		logprintf(LOG_DEBUG, "device \"%s\" is already \"%s\", aborting action \"%s\"", data->device, data->new_state, action_switch->name);
		data->steps = -1;
		goto close;
	}

	switch(data->steps) {
		case INIT: {
			data->steps = EXECUTE;

			if(data->seconds_after > 0) {
				struct timeval tv;
				switch(data->type_after) {
					case 1:
						tv.tv_sec = 0;
						tv.tv_usec = data->seconds_after;
					break;
					case 2:
					case 3:
					case 4:
					case 5:
						tv.tv_sec = data->seconds_after;
						tv.tv_usec = 0;
					break;
				}

				uv_timer_t *timer_req = NULL;
				if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				timer_req->data = data;
				uv_timer_init(uv_default_loop(), timer_req);
				assert(tv.tv_sec > 0 || tv.tv_usec > 0);
				uv_timer_start(timer_req, (void (*)(uv_timer_t *))thread, tv.tv_sec*1000+tv.tv_usec, 0);

				goto end;
			}
		};
		case EXECUTE: {
			struct timeval tv;

			struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			if((data1->dev = STRDUP(data->device)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			if((data1->state = STRDUP(data->new_state)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			data1->values = NULL;

			eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);

			if(data->seconds_for > 0) {
				switch(data->type_for) {
					case 1:
						tv.tv_sec = 0;
						tv.tv_usec = data->seconds_for;
					break;
					case 2:
					case 3:
					case 4:
					case 5:
						tv.tv_sec = data->seconds_for;
						tv.tv_usec = 0;
					break;
				}
				data->steps = RESTORE;
				logprintf(LOG_DEBUG, "EXECUTE %lu %lu", tv.tv_sec, tv.tv_usec);

				uv_timer_t *timer_req = NULL;
				if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				timer_req->data = data;
				uv_timer_init(uv_default_loop(), timer_req);
				assert(tv.tv_sec > 0 || tv.tv_usec > 0);
				uv_timer_start(timer_req, (void (*)(uv_timer_t *))thread, tv.tv_sec*1000+tv.tv_usec, 0);

				goto end;
			}
		} break;
		case RESTORE: {
			logprintf(LOG_DEBUG, "RESTORE");

			struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			data1->dev = STRDUP(data->device);
			data1->state = STRDUP(data->old_state);
			data1->values = NULL;

			eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);
		} break;
	}

close:
	FREE(data->device);
	FREE(data);

end:
	return;
}

static void thread_free(uv_work_t *req, int status) {
	FREE(req);
}

static void prepare(struct rules_actions_t *obj, char *dev) {
	struct JsonNode *json = obj->parsedargs;

	struct JsonNode *jto = NULL;
	struct JsonNode *jafter = NULL;
	struct JsonNode *jfor = NULL;
	struct JsonNode *javalues = NULL;
	struct JsonNode *jcvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jstate = NULL;
	struct JsonNode *jaseconds = NULL;
	char *old_state = NULL;
	char *state = NULL, **array = NULL;
	int seconds_after = 0, type_after = 0;
	int	l = 0, i = 0, nrunits = (sizeof(units)/sizeof(units[0]));
	int seconds_for = 0, type_for = 0;

	if((jafter = json_find_member(json, "AFTER")) != NULL) {
		if((jdvalues = json_find_member(jafter, "value")) != NULL) {
			jaseconds = json_find_element(jdvalues, 0);
			if(jaseconds != NULL) {
				if(jaseconds->tag == JSON_STRING) {
					l = explode(jaseconds->string_, " ", &array);
					if(l == 2) {
						for(i=0;i<nrunits;i++) {
							if(strcmp(array[1], units[i].name) == 0) {
								seconds_after = atoi(array[0]);
								type_after = units[i].id;
								break;
							}
						}
					}
					if(l > 0) {
						array_free(&array, l);
					}
				}
			}
		}
	}

	if((jfor = json_find_member(json, "FOR")) != NULL) {
		if((jcvalues = json_find_member(jfor, "value")) != NULL) {
			jaseconds = json_find_element(jcvalues, 0);
			if(jaseconds != NULL) {
				if(jaseconds->tag == JSON_STRING) {
					l = explode(jaseconds->string_, " ", &array);
					if(l == 2) {
						for(i=0;i<nrunits;i++) {
							if(strcmp(array[1], units[i].name) == 0) {
								seconds_for = atoi(array[0]);
								type_for = units[i].id;
								break;
							}
						}
					}
					if(l > 0) {
						array_free(&array, l);
					}
				}
			}
		}
	}

	switch(type_for) {
		case 3:
			seconds_for *= 60;
		break;
		case 4:
			seconds_for *= (60*60);
		break;
		case 5:
			seconds_for *= (60*60*24);
		break;
	}

	switch(type_after) {
		case 3:
			seconds_after *= 60;
		break;
		case 4:
			seconds_after *= (60*60);
		break;
		case 5:
			seconds_after *= (60*60*24);
		break;
	}

	/* Select current state */
	if(devices_select_string_setting(ORIGIN_ACTION, dev, "state", &old_state) != 0) {
		logprintf(LOG_NOTICE, "could not select old state of \"%s\"", dev);
		return;
	}

	if((jto = json_find_member(json, "TO")) != NULL) {
		if((javalues = json_find_member(jto, "value")) != NULL) {
			jstate = json_find_element(javalues, 0);
			if(jstate != NULL && jstate->tag == JSON_STRING) {
				state = jstate->string_;
			}
		}
	}

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	if((data->device = MALLOC(strlen(dev)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(data->device, dev);
	data->seconds_for = seconds_for;
	data->type_for = type_for;
	data->seconds_after = seconds_after;
	data->type_after = type_after;
	data->steps = INIT;
	strcpy(data->old_state, old_state);
	strcpy(data->new_state, state);

	data->exec = event_action_set_execution_id(dev);

	uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
	if(tp_work_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	tp_work_req->data = data;
	if(uv_queue_work(uv_default_loop(), tp_work_req, "action_switch", thread, thread_free) < 0) {
		FREE(tp_work_req);
	}

	return;
}

static void *gc(void *param) {
	struct event_action_thread_t *pth = param;
	struct data_t *data = pth->userdata;
	if(data != NULL) {
		if(data->device != NULL) {
			FREE(data->device);
		}
		FREE(data);
	}
	return NULL;
}

static int run(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jbvalues = NULL;
	struct JsonNode *jbchild = NULL;
	struct device_t *dev = NULL;

	if((jdevice = json_find_member(obj->parsedargs, "DEVICE")) != NULL &&
		 (jto = json_find_member(obj->parsedargs, "TO")) != NULL) {
		if((jbvalues = json_find_member(jdevice, "value")) != NULL) {
			jbchild = json_first_child(jbvalues);
			while(jbchild) {
				if(jbchild->tag == JSON_STRING) {
					if(devices_select(ORIGIN_ACTION, jbchild->string_, NULL) == 0) {
						if(devices_select_struct(ORIGIN_ACTION, jbchild->string_, &dev) == 0) {
							prepare(obj, jbchild->string_);
						}
					}
				}
				jbchild = jbchild->next;
			}
		}
	}
	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionSwitchInit(void) {
	event_action_register(&action_switch, "switch");

	options_add(&action_switch->options, 'a', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_switch->options, 'b', "TO", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_switch->options, 'c', "AFTER", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_switch->options, 'd', "FOR", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_switch->run = &run;
	action_switch->checkArguments = &checkArguments;
	action_switch->gc = &gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "switch";
	module->version = "4.1";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionSwitchInit();
}
#endif
