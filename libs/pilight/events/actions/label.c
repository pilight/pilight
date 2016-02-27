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
#include <errno.h>

#include "../action.h"
#include "../events.h"
#include "../../core/options.h"
#include "../../core/threadpool.h"
#include "../../protocols/protocol.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "label.h"

#define INIT		0
#define EXECUTE	1
#define RESTORE	2

typedef struct data_t {
	char *device;
	char *old_label;
	char *new_label;
	char *old_color;
	char *new_color;
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

static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jfor = NULL;
	struct JsonNode *jafter = NULL;
	struct JsonNode *jcolor = NULL;
	struct JsonNode *javalues = NULL;
	struct JsonNode *jbvalues = NULL;
	struct JsonNode *jcvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jevalues = NULL;
	struct JsonNode *jachild = NULL;
	struct JsonNode *jbchild = NULL;
	struct JsonNode *jcchild = NULL;
	struct JsonNode *jdchild = NULL;
	struct JsonNode *jechild = NULL;
	char **array = NULL;
	double nr1 = 0.0, nr2 = 0.0, nr3 = 0.0, nr4 = 0.0, nr5 = 0.0;
	int nrvalues = 0, l = 0, i = 0, match = 0;
	int	nrunits = (sizeof(units)/sizeof(units[0]));

	jdevice = json_find_member(obj->parsedargs, "DEVICE");
	jto = json_find_member(obj->parsedargs, "TO");
	jfor = json_find_member(obj->parsedargs, "FOR");
	jafter = json_find_member(obj->parsedargs, "AFTER");
	jcolor = json_find_member(obj->parsedargs, "COLOR");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "label action is missing a \"DEVICE\"");
		return -1;
	}

	if(jto == NULL) {
		logprintf(LOG_ERR, "label action is missing a \"TO ...\" statement");
		return -1;
	}

	json_find_number(jdevice, "order", &nr1);
	json_find_number(jto, "order", &nr2);

	if(jcolor != NULL) {
		json_find_number(jcolor, "order", &nr5);
	}

	if(jfor != NULL) {
		json_find_number(jfor, "order", &nr3);
		if(nr3 < nr2) {
			logprintf(LOG_ERR, "label actions are formatted as \"label DEVICE ... TO ... FOR ...\"");
			return -1;
		} else if(nr5 > 0 && nr3 < nr5) {
			logprintf(LOG_ERR, "label actions are formatted as \"label DEVICE ... TO ... COLOR ... FOR ...\"");
			return -1;
		}
	}

	if(jafter != NULL) {
		json_find_number(jafter, "order", &nr4);
		if(nr4 < nr2) {
			logprintf(LOG_ERR, "label actions are formatted as \"label DEVICE ... TO ... AFTER ...\"");
			return -1;
		} else if(nr5 > 0 && nr4 < nr5) {
			logprintf(LOG_ERR, "label actions are formatted as \"label DEVICE ... TO ... COLOR ... AFTER ...\"");
			return -1;
		}
	}

	if((int)nr1 != 1 || (int)nr2 != 2) {
		logprintf(LOG_ERR, "label actions are formatted as \"label DEVICE ... TO ...\"");
		return -1;
	}
	if(nr5 > 0 && nr5 != 3) {
		logprintf(LOG_ERR, "label actions are formatted as \"label DEVICE ... TO ... COLOR ...\"");
		return -1;
	}

	nrvalues = 0;
	if((javalues = json_find_member(jto, "value")) != NULL) {
		jachild = json_first_child(javalues);
		while(jachild) {
			nrvalues++;
			jachild = jachild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "label action \"TO\" only takes one argument");
		return -1;
	}

	nrvalues = 0;
	if(jfor != NULL) {
		if((jcvalues = json_find_member(jfor, "value")) != NULL) {
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
								if(isNumeric(array[0]) != 0 && atoi(array[0]) <= 0) {
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
						array_free(&array, l);
					} else {
						logprintf(LOG_ERR, "switch action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
						if(l > 0) {
							array_free(&array, l);
						}
						return -1;
					}
				}
				jcchild = jcchild->next;
			}
		}
		if(nrvalues != 1) {
			logprintf(LOG_ERR, "label action \"FOR\" only takes one argument");
			return -1;
		}
	}

	nrvalues = 0;
	if(jafter != NULL) {
		if((jdvalues = json_find_member(jafter, "value")) != NULL) {
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
								if(isNumeric(array[0]) != 0) {
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
						array_free(&array, l);
					} else {
						logprintf(LOG_ERR, "switch action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
						if(l > 0) {
							array_free(&array, l);
						}
						return -1;
					}
				}
				jdchild = jdchild->next;
			}
		}
		if(nrvalues != 1) {
			logprintf(LOG_ERR, "label action \"AFTER\" only takes one argument");
			return -1;
		}
	}

	nrvalues = 0;
	if(jcolor != NULL) {
		if((jevalues = json_find_member(jcolor, "value")) != NULL) {
			jechild = json_first_child(jevalues);
			while(jechild) {
				nrvalues++;
				jechild = jechild->next;
			}
		}
		if(nrvalues != 1) {
			logprintf(LOG_ERR, "label action \"COLOR\" only takes one argument");
			return -1;
		}
	}

	if((jbvalues = json_find_member(jdevice, "value")) != NULL) {
		jbchild = json_first_child(jbvalues);
		while(jbchild) {
			if(jbchild->tag == JSON_STRING) {
				if(devices_select(ORIGIN_ACTION, jbchild->string_, NULL) == 0) {
					struct protocol_t *protocol = NULL;
					if(devices_select_protocol(ORIGIN_ACTION, jbchild->string_, 0, &protocol) == 0) {
						if(protocol->devtype == LABEL) {
							match = 1;
							break;
						}
					}
					if(match == 0) {
						logprintf(LOG_ERR, "the label action only works with the label devices");
						return -1;
					}
				} else {
					logprintf(LOG_ERR, "device \"%s\" doesn't exists", jbchild->string_);
					return -1;
				}
			} else {
				return -1;
			}
			jbchild = jbchild->next;
		}
	}
	return 0;
}

static void *execute(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *data = task->userdata;
	struct event_action_thread_t *pth = data->pth;
	unsigned long exec = 0;

	/*
	 * Check if we are the last action for this device
	 * or else skip execution.
	 */
	if(event_action_get_execution_id(data->device, &exec) == 0) {
		if(exec != data->exec) {
			logprintf(LOG_DEBUG, "skipping overridden action %s for device %s", action_label->name, data->device);
			goto close;
		}
	}

	if(strcmp(data->old_label, data->new_label) == 0 && data->new_color != NULL && strcmp(data->old_color, data->new_color) == 0) {
		logprintf(LOG_DEBUG, "device \"%s\" is already labeled \"%s\" with color \"%s\", aborting action \"%s\"", data->device, data->new_label, data->new_color, action_label->name);
		data->steps = -1;
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
				logprintf(LOG_DEBUG, "INIT %lu %lu", tv.tv_sec, tv.tv_usec);

				threadpool_add_scheduled_work(pth->action->name, execute, tv, data);
				goto end;
			}
		};
		case EXECUTE: {
			struct timeval tv;

			if(pilight.control != NULL) {
				struct JsonNode *jvalues = json_mkobject();
				if(data->new_color != NULL) {
					json_append_member(jvalues, "color", json_mkstring(data->new_color));
				}
				json_append_member(jvalues, "label", json_mkstring(data->new_label));
				pilight.control(pth->device->id, NULL, json_first_child(jvalues), ORIGIN_ACTION);
				json_delete(jvalues);
			}
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
				threadpool_add_scheduled_work(pth->action->name, execute, tv, data);
				goto end;
			}
		} break;
		case RESTORE: {
			logprintf(LOG_DEBUG, "RESTORE");
			if(pilight.control != NULL) {
				struct JsonNode *jvalues = json_mkobject();
				if(data->old_color != NULL) {
					json_append_member(jvalues, "color", json_mkstring(data->old_color));
				}
				json_append_member(jvalues, "label", json_mkstring(data->old_label));
				pilight.control(pth->device->id, NULL, json_first_child(jvalues), ORIGIN_ACTION);
				json_delete(jvalues);
			}
		} break;
	}

close:
	FREE(data->old_color);
	if(data->new_color != NULL) {
		FREE(data->new_color);
	}
	FREE(data->old_label);
	FREE(data->new_label);
	FREE(data->device);
	FREE(data);

	pth->userdata = NULL;

	event_action_stopped(pth);

end:
	return NULL;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct event_action_thread_t *pth = task->userdata;
	struct JsonNode *json = pth->obj->parsedargs;

	struct JsonNode *jto = NULL;
	struct JsonNode *jafter = NULL;
	struct JsonNode *jfor = NULL;
	struct JsonNode *jcolor = NULL;
	struct JsonNode *javalues = NULL;
	struct JsonNode *jcvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jevalues = NULL;
	struct JsonNode *jlabel = NULL;
	struct JsonNode *jaseconds = NULL;
	char *new_label = NULL, *old_label = NULL, *label = NULL, **array = NULL;
	char *new_color = NULL, *old_color = NULL, *color = NULL;
	int seconds_after = 0, type_after = 0, free_label = 0, free_old_label = 0;
	int	l = 0, i = 0, nrunits = (sizeof(units)/sizeof(units[0]));
	int seconds_for = 0, type_for = 0;
	double itmp = 0.0;

	event_action_started(pth);

	if((jcolor = json_find_member(json, "COLOR")) != NULL) {
		if((jevalues = json_find_member(jcolor, "value")) != NULL) {
			jcolor = json_find_element(jevalues, 0);
			if(jcolor != NULL && jcolor->tag == JSON_STRING) {
				color = jcolor->string_;
				if((new_color = MALLOC(strlen(color)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(new_color, color);
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

	/* Store current label */
	if(devices_select_string_setting(ORIGIN_ACTION, pth->device->id, "label", &old_label) == 0) {
		// true
	} else if(devices_select_number_setting(ORIGIN_ACTION, pth->device->id, "label", &itmp, NULL) == 0) {
		int len = snprintf(NULL, 0, "%f", itmp);
		if((old_label = MALLOC(len+1)) == NULL) {
			OUT_OF_MEMORY
		}
		snprintf(old_label, len, "%f", itmp);
		free_old_label = 1;
	} else {
		logprintf(LOG_NOTICE, "could not store old label of \"%s\"", pth->device->id);
		event_action_stopped(pth);
		goto end;
	}
	if(devices_select_string_setting(ORIGIN_ACTION, pth->device->id, "color", &old_color) != 0) {
		logprintf(LOG_NOTICE, "could not store old color of \"%s\"", pth->device->id);
		event_action_stopped(pth);
		goto end;
	}

	if((jto = json_find_member(json, "TO")) != NULL) {
		if((javalues = json_find_member(jto, "value")) != NULL) {
			jlabel = json_find_element(javalues, 0);
			if(jlabel != NULL) {
				if(jlabel->tag == JSON_STRING) {
					label = jlabel->string_;
				} else if(jlabel->tag == JSON_NUMBER) {
					int l = snprintf(NULL, 0, "%.*f", jlabel->decimals_, jlabel->number_);
					if(l < 2) {
						l = 2;
					}
					if((label = MALLOC(l+1)) == NULL) {
						OUT_OF_MEMORY
					}
					memset(label, '\0', l);
					free_label = 1;
					l = snprintf(label, l, "%.*f", jlabel->decimals_, jlabel->number_);
					label[l] = '\0';
				}
				if((new_label = MALLOC(strlen(label)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(new_label, label);
			}
		}
	}

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	if((data->device = MALLOC(strlen(pth->device->id)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(data->device, pth->device->id);

	if((data->old_label = MALLOC(strlen(old_label)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(data->old_label, old_label);

	if((data->new_label = MALLOC(strlen(new_label)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(data->new_label, new_label);

	if((data->old_color = MALLOC(strlen(old_color)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(data->old_color, old_color);

	if(new_color != NULL) {
		if((data->new_color = MALLOC(strlen(new_color)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(data->new_color, new_color);
	} else {
		data->new_color = NULL;
	}

	data->seconds_for = seconds_for;
	data->type_for = type_for;
	data->seconds_after = seconds_after;
	data->type_after = type_after;
	data->steps = INIT;
	data->pth = pth;

	pth->userdata = (void *)data;

	data->exec = event_action_set_execution_id(pth->device->id);

	threadpool_add_work(REASON_END, NULL, action_label->name, 0, execute, NULL, (void *)data);

	if(free_label == 1) {
		FREE(label);
	}

	if(free_old_label == 1) {
		FREE(old_label);
	}

	if(new_label != NULL) {
		FREE(new_label);
	}

	if(new_color != NULL) {
		FREE(new_color);
	}
	return (void *)NULL;

end:
	if(free_label == 1) {
		FREE(label);
	}

	if(free_old_label == 1) {
		FREE(old_label);
	}

	if(new_label != NULL) {
		FREE(new_label);
	}

	if(new_color != NULL) {
		FREE(new_color);
	}

	event_action_stopped(pth);
	return (void *)NULL;
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
					if(devices_select_struct(ORIGIN_ACTION, jbchild->string_, &dev) == 0) {
						event_action_thread_start(dev, action_label, thread, obj);
					}
				}
				jbchild = jbchild->next;
			}
		}
	}
	return 0;
}

static void *gc(void *param) {
	struct event_action_thread_t *pth = param;
	struct data_t *data = pth->userdata;
	if(data != NULL) {
		if(data->old_color != NULL) {
			FREE(data->old_color);
		}
		if(data->new_color != NULL) {
			FREE(data->new_color);
		}
		if(data->device != NULL) {
			FREE(data->device);
		}
		if(data->old_label != NULL) {
			FREE(data->old_label);
		}
		if(data->new_label != NULL) {
			FREE(data->new_label);
		}
		FREE(data);
	}
	return NULL;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionLabelInit(void) {
	event_action_register(&action_label, "label");

	options_add(&action_label->options, 'a', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_label->options, 'b', "TO", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING | JSON_NUMBER, NULL, NULL);
	options_add(&action_label->options, 'c', "AFTER", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_label->options, 'd', "FOR", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_label->options, 'e', "COLOR", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_label->run = &run;
	action_label->gc = &gc;
	action_label->checkArguments = &checkArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "label";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionLabelInit();
}
#endif
