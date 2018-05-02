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
#include "../../protocols/protocol.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "label.h"

#include "../../../libuv/uv.h"

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

static void *reason_control_device_free(void *param) {
	struct reason_control_device_t *data = param;
	json_delete(data->values);
	FREE(data->dev);
	FREE(data);
	return NULL;
}

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

	if(obj == NULL) {
		/* Internal error */
		return -1;
	}

	if(obj->arguments == NULL) {
		/* Internal error */
		return -1;
	}

	jdevice = json_find_member(obj->arguments, "DEVICE");
	jto = json_find_member(obj->arguments, "TO");
	jfor = json_find_member(obj->arguments, "FOR");
	jafter = json_find_member(obj->arguments, "AFTER");
	jcolor = json_find_member(obj->arguments, "COLOR");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "label action is missing a \"DEVICE\"");
		return -1;
	}

	if(jto == NULL) {
		logprintf(LOG_ERR, "label action is missing a \"TO ...\" statement");
		return -1;
	}

	if(jdevice->tag != JSON_OBJECT) {
		/* Internal error */
		return -1;
	}

	if(jto->tag != JSON_OBJECT) {
		/* Internal error */
		return -1;
	}

	json_find_number(jdevice, "order", &nr1);
	json_find_number(jto, "order", &nr2);

	if(jcolor != NULL) {
		json_find_number(jcolor, "order", &nr5);

		if(jcolor->tag != JSON_OBJECT) {
			/* Internal error */
			return -1;
		}
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

	/*
	 * FIXME: Possibly redundant
	 */
	if(nr5 > 0 && nr5 != 3) {
		logprintf(LOG_ERR, "label actions are formatted as \"label DEVICE ... TO ... COLOR ...\"");
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
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "label action \"TO\" only takes one argument");
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
								logprintf(LOG_ERR, "label action \"%s\" is not a valid unit", array[1]);
								array_free(&array, l);
								return -1;
							}
							array_free(&array, l);
						} else {
							logprintf(LOG_ERR, "label action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
							if(l > 0) {
								array_free(&array, l);
							}
							return -1;
						}
					}
					jcchild = jcchild->next;
				}
			} else {
				/* Internal error */
				return -1;
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
										logprintf(LOG_ERR, "label action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
										array_free(&array, l);
										return -1;
									}
									break;
								}
							}
							if(match == 0) {
								logprintf(LOG_ERR, "label action \"%s\" is not a valid unit", array[1]);
								array_free(&array, l);
								return -1;
							}
							array_free(&array, l);
						} else {
							logprintf(LOG_ERR, "label action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
							if(l > 0) {
								array_free(&array, l);
							}
							return -1;
						}
					}
					jdchild = jdchild->next;
				}
			} else {
				/* Internal error */
				return -1;
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
			if(jevalues->tag == JSON_ARRAY) {
				jechild = json_first_child(jevalues);
				while(jechild) {
					nrvalues++;
					jechild = jechild->next;
				}
			} else {
				/* Internal error */
				return -1;
			}
		}
		if(nrvalues != 1) {
			logprintf(LOG_ERR, "label action \"COLOR\" only takes one argument");
			return -1;
		}
	}

	if((jbvalues = json_find_member(jdevice, "value")) != NULL) {
		if(jbvalues->tag == JSON_ARRAY) {
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
							logprintf(LOG_ERR, "the label action only works with label devices");
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
			logprintf(LOG_DEBUG, "skipping overridden action %s for device %s", action_label->name, data->device);
			goto close;
		}
	}

	if(strcmp(data->old_label, data->new_label) == 0 && data->new_color != NULL && strcmp(data->old_color, data->new_color) == 0) {
		logprintf(LOG_DEBUG, "device \"%s\" is already labelled \"%s\" with color \"%s\", aborting action \"%s\"", data->device, data->new_label, data->new_color, action_label->name);
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

			data1->state = NULL;

			data1->values = json_mkobject();
			if(data->new_color != NULL) {
				json_append_member(data1->values, "color", json_mkstring(data->new_color));
			}
			json_append_member(data1->values, "label", json_mkstring(data->new_label));

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
			struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			if((data1->dev = STRDUP(data->device)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			data1->state = NULL;

			data1->values = json_mkobject();
			if(data->new_color != NULL) {
				json_append_member(data1->values, "color", json_mkstring(data->old_color));
			}
			json_append_member(data1->values, "label", json_mkstring(data->old_label));

			eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);

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

	// pth->userdata = NULL;

	// event_action_stopped(pth);

end:
	return;
}

static void thread_free(uv_work_t *req, int status) {
	FREE(req);
}

static void prepare(struct rules_actions_t *obj, char *dev) {
	struct JsonNode *json = obj->arguments;

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

	if((jcolor = json_find_member(json, "COLOR")) != NULL) {
		if((jevalues = json_find_member(jcolor, "value")) != NULL) {
			jcolor = json_find_element(jevalues, 0);
			if(jcolor != NULL && jcolor->tag == JSON_STRING) {
				color = jcolor->string_;
				if((new_color = MALLOC(strlen(color)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
	if(devices_select_string_setting(ORIGIN_ACTION, dev, "label", &old_label) == 0) {
		// true
	} else if(devices_select_number_setting(ORIGIN_ACTION, dev, "label", &itmp, NULL) == 0) {
		int len = snprintf(NULL, 0, "%f", itmp);
		if((old_label = MALLOC(len+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		snprintf(old_label, len, "%f", itmp);
		free_old_label = 1;
	} else {
		logprintf(LOG_NOTICE, "could not store old label of \"%s\"", dev);
		goto end;
	}
	if(devices_select_string_setting(ORIGIN_ACTION, dev, "color", &old_color) != 0) {
		logprintf(LOG_NOTICE, "could not store old color of \"%s\"", dev);
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
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					memset(label, '\0', l);
					free_label = 1;
					l = snprintf(label, l + 1, "%.*f", jlabel->decimals_, jlabel->number_);
					label[l] = '\0';
				}
				if((new_label = MALLOC(strlen(label)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(new_label, label);
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

	if((data->old_label = MALLOC(strlen(old_label)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(data->old_label, old_label);

	if((data->new_label = MALLOC(strlen(new_label)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(data->new_label, new_label);

	if((data->old_color = MALLOC(strlen(old_color)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(data->old_color, old_color);

	if(new_color != NULL) {
		if((data->new_color = MALLOC(strlen(new_color)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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

	data->exec = event_action_set_execution_id(dev);

	uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
	if(tp_work_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	tp_work_req->data = data;
	if(uv_queue_work(uv_default_loop(), tp_work_req, "action_label", thread, thread_free) < 0) {
		FREE(tp_work_req);
	}

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
	return;

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

	return;
}

static int run(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jbvalues = NULL;
	struct JsonNode *jbchild = NULL;
	struct device_t *dev = NULL;

	if((jdevice = json_find_member(obj->arguments, "DEVICE")) != NULL &&
		 (jto = json_find_member(obj->arguments, "TO")) != NULL) {
		if((jbvalues = json_find_member(jdevice, "value")) != NULL) {
			jbchild = json_first_child(jbvalues);
			while(jbchild) {
				if(jbchild->tag == JSON_STRING) {
					if(devices_select_struct(ORIGIN_ACTION, jbchild->string_, &dev) == 0) {
						prepare(obj, jbchild->string_);
						// event_action_thread_start(dev, action_label, thread, obj);
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
	module->version = "3.1";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionLabelInit();
}
#endif
