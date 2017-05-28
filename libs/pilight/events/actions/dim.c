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

#include "../../core/pilight.h"
#include "../../core/options.h"
#include "../../protocols/protocol.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../action.h"
#include "../events.h"
#include "dim.h"

#define DECREASING 0
#define INCREASING 1

#define INIT						0
#define EXECUTE					1
#define EXECUTESTEPS		2
#define RESTORE					3

typedef struct data_t {
	char *device;
	char old_state[4];
	struct timeval interval;
	int from_dimlevel;
	int to_dimlevel;
	int old_dimlevel;
	int steps;
	int direction;
	int seconds_for;
	int type_for;
	int seconds_in;
	int type_in;
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
	FREE(data->state);
	FREE(data);
	return NULL;
}

static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jfor = NULL;
	struct JsonNode *jafter = NULL;
	struct JsonNode *jfrom = NULL;
	struct JsonNode *jin = NULL;
	struct JsonNode *javalues = NULL;
	struct JsonNode *jbvalues = NULL;
	struct JsonNode *jcvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jevalues = NULL;
	struct JsonNode *jfvalues = NULL;
	struct JsonNode *jachild = NULL;
	struct JsonNode *jbchild = NULL;
	struct JsonNode *jcchild = NULL;
	struct JsonNode *jdchild = NULL;
	struct JsonNode *jechild = NULL;
	struct JsonNode *jfchild = NULL;
	char **array = NULL;
	double nr1 = 0.0, nr2 = 0.0, nr3 = 0.0, nr4 = 0.0, nr5 = 0.0, nr6 = 0.0;
	double dimfrom = 0.0, dimto = 0.0;
	int nrvalues = 0, l = 0, i = 0, match = 0, match1 = 0, match2 = 0;
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
	jin = json_find_member(obj->parsedargs, "IN");
	jafter = json_find_member(obj->parsedargs, "AFTER");
	jfrom = json_find_member(obj->parsedargs, "FROM");

	if(jdevice == NULL) {
		logprintf(LOG_ERR, "dim action is missing a \"DEVICE ...\" statement");
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

	if(jfor != NULL) {
		json_find_number(jfor, "order", &nr3);
		if(nr3 < nr2) {
			logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ... FOR ...\"");
			return -1;
		}
	}

	if(jafter != NULL) {
		json_find_number(jafter, "order", &nr4);
		if(nr4 < nr2) {
			logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ... AFTER ...\"");
			return -1;
		}
	}

	if(jin != NULL) {
		json_find_number(jin, "order", &nr6);
		if(nr6 < nr2) {
			logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
			return -1;
		}
	}

	if(jfrom != NULL) {
		json_find_number(jfrom, "order", &nr5);
		if(nr5 < nr2) {
			logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
			return -1;
		}
		if(jin == NULL) {
			logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
			return -1;
		}
	} else if(jin != NULL) {
		logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
		return -1;
	}

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
		logprintf(LOG_ERR, "dim actions are formatted as \"dim DEVICE ... TO ...\"");
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
										logprintf(LOG_ERR, "dim action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
										array_free(&array, l);
										return -1;
									}
									break;
								}
							}
							if(match == 0) {
								logprintf(LOG_ERR, "dim action \"%s\" is not a valid unit", array[1]);
								array_free(&array, l);
								return -1;
							}
							array_free(&array, l);
						} else {
							logprintf(LOG_ERR, "dim action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
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
			logprintf(LOG_ERR, "dim action \"FOR\" only takes one argument");
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
										logprintf(LOG_ERR, "dim action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
										array_free(&array, l);
										return -1;
									}
									break;
								}
							}
							if(match == 0) {
								logprintf(LOG_ERR, "dim action \"%s\" is not a valid unit", array[1]);
								array_free(&array, l);
								return -1;
							}
							array_free(&array, l);
						} else {
							logprintf(LOG_ERR, "dim action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
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
			logprintf(LOG_ERR, "dim action \"AFTER\" only takes one argument");
			return -1;
		}
	}

	nrvalues = 0;
	if(jfrom != NULL) {
		if((jevalues = json_find_member(jfrom, "value")) != NULL) {
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
			logprintf(LOG_ERR, "dim action \"FROM\" only takes one argument");
			return -1;
		}
	}

	nrvalues = 0;
	if(jin != NULL) {
		if((jfvalues = json_find_member(jin, "value")) != NULL) {
			if(jfvalues->tag == JSON_ARRAY) {
				jfchild = json_first_child(jfvalues);
				while(jfchild) {
					nrvalues++;
					if(jfchild->tag == JSON_STRING) {
						l = explode(jfchild->string_, " ", &array);
						if(l == 2) {
							match = 0;
							for(i=0;i<nrunits;i++) {
								if(strcmp(array[1], units[i].name) == 0) {
									match = 1;
									if(isNumeric(array[0]) != 0 || atoi(array[0]) <= 0) {
										logprintf(LOG_ERR, "dim action \"IN\" requires a positive number and a unit e.g. \"1 MINUTE\"");
										array_free(&array, l);
										return -1;
									}
									break;
								}
							}
							if(match == 0) {
								logprintf(LOG_ERR, "dim action \"%s\" is not a valid unit", array[1]);
								array_free(&array, l);
								return -1;
							}
							array_free(&array, l);
						} else {
							logprintf(LOG_ERR, "dim action \"IN\" requires a positive number and a unit e.g. \"1 MINUTE\"");
							if(l > 0) {
								array_free(&array, l);
							}
							return -1;
						}
					}
					jfchild = jfchild->next;
				}
			} else {
				/* Internal error */
				return -1;
			}
		}
		if(nrvalues != 1) {
			logprintf(LOG_ERR, "dim action \"IN\" only takes one argument");
			return -1;
		}
	}

	if((jbvalues = json_find_member(jdevice, "value")) != NULL) {
		if(jbvalues->tag == JSON_ARRAY) {
			jbchild = json_first_child(jbvalues);
			while(jbchild) {
				if(jbchild->tag == JSON_STRING) {
					char *setting = NULL;
					struct varcont_t val;
					struct protocol_t *protocol = NULL;
					if((javalues = json_find_member(jto, "value")) != NULL) {
						if(javalues->tag == JSON_ARRAY) {
							jachild = json_first_child(javalues);
							while(jachild) {
								match = 0;
								if(jachild->tag == JSON_NUMBER) {
									dimto = jachild->number_;
									match = 1;
								}
								if(match == 1) {
									if(devices_select_protocol(ORIGIN_ACTION, jbchild->string_, 0, &protocol) == 0) {
										if(protocol->devtype == DIMMER) {
											while(devices_select_settings(ORIGIN_ACTION, jbchild->string_, i++, &setting, &val) == 0) {
												if(strcmp(setting, "dimlevel-maximum") == 0 && val.type_ == JSON_NUMBER) {
													if((int)val.number_ < dimto) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimto);
														return -1;
													}
													match1 = 1;
												}
												if(strcmp(setting, "dimlevel-minimum") == 0 && val.type_ == JSON_NUMBER) {
													if((int)val.number_ > dimto) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimto);
														return -1;
													}
													match2 = 1;
												}
											}
											if(match1 == 0 || match2 == 0) {
												struct options_t *opt = protocol->options;
												while(opt) {
													/*
													 * TESTME
													 */
													if(match1 == 0 && strcmp(opt->name, "dimlevel-maximum") == 0 &&
														opt->vartype == JSON_NUMBER && (int)(intptr_t)opt->def < (int)dimto) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimto);
														return -1;
													}
													if(match2 == 0 && strcmp(opt->name, "dimlevel-minimum") == 0 &&
														opt->vartype == JSON_NUMBER && (int)(intptr_t)opt->def > (int)dimto) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimto);
														return -1;
													}
													opt = opt->next;
												}
											}
										} else {
											/*
											 * TESTME
											 */
											logprintf(LOG_ERR, "device \"%s\" does not support dimming", jbchild->string_);
											return -1;
										}
									} else {
										logprintf(LOG_ERR, "device \"%s\" does not exist", jbchild->string_);
										return -1;
									}
								} else {
									/*
									 * TESTME
									 */
									/* Internal error */
									return -1;
								}
								jachild = jachild->next;
							}
						} else {
							/* Internal error */
							return -1;
						}
					} else {
						/* Internal error */
						return -1;
					}

					if((jevalues = json_find_member(jfrom, "value")) != NULL) {
						if(jevalues->tag == JSON_ARRAY) {
							jechild = json_first_child(jevalues);
							while(jechild) {
								match = 0;
								if(jechild->tag == JSON_NUMBER) {
									dimfrom = jechild->number_;
									match = 1;
								}
								if(match == 1) {
									if(devices_select_protocol(ORIGIN_ACTION, jbchild->string_, 0, &protocol) == 0) {
										if(protocol->devtype == DIMMER) {
											i = 0;
											while(devices_select_settings(ORIGIN_ACTION, jbchild->string_, i++, &setting, &val) == 0) {
												if(strcmp(setting, "dimlevel-maximum") == 0 && val.type_ == JSON_NUMBER) {
													if((int)val.number_ < dimfrom) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimfrom);
														return -1;
													}
													match1 = 1;
												}
												if(strcmp(setting, "dimlevel-minimum") == 0 && val.type_ == JSON_NUMBER) {
													if((int)val.number_ > dimfrom) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimfrom);
														return -1;
													}
													match2 = 1;
												}
											}
											if(match1 == 0 || match2 == 0) {
												struct options_t *opt = protocol->options;
												while(opt) {
													/*
													 * TESTME
													 */
													if(match1 == 0 && strcmp(opt->name, "dimlevel-maximum") == 0 &&
														opt->vartype == JSON_NUMBER && (int)(intptr_t)opt->def < (int)dimfrom) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimfrom);
														return -1;
													}
													if(match2 == 0 && strcmp(opt->name, "dimlevel-minimum") == 0 &&
														opt->vartype == JSON_NUMBER && (int)(intptr_t)opt->def > (int)dimfrom) {
														logprintf(LOG_ERR, "device \"%s\" cannot be set to dimlevel \"%d\"", jbchild->string_, (int)dimfrom);
														return -1;
													}
													opt = opt->next;
												}
											}
										} else {
											/*
											 * TESTME
											 */
											logprintf(LOG_ERR, "device \"%s\" does not support dimming", jbchild->string_);
											return -1;
										}
									} else {
										logprintf(LOG_ERR, "device \"%s\" does not exist", jbchild->string_);
										return -1;
									}
								}
								jechild = jechild->next;
							}
						} else {
							/* Internal error */
							return -1;
						}
					}
				} else {
					/* Internal error */
					return -1;
				}
				jbchild = jbchild->next;
			}
		} else {
			/* Internal error */
			return -1;
		}
	} else {
		/* Internal error */
		return -1;
	}
	return 0;
}

static void thread(uv_work_t *req) {
	struct data_t *data = req->data;

	unsigned long exec = 0;
	char state[3];

	/*
	 * Check if we are the last action for this device
	 * or else skip execution.
	 */
	if(event_action_get_execution_id(data->device, &exec) == 0) {
		if(exec != data->exec) {
			logprintf(LOG_DEBUG, "skipping overridden action %s for device %s", action_dim->name, data->device);
			goto close;
		}
	}

	if(data->steps == INIT && data->from_dimlevel == data->to_dimlevel) {
		logprintf(LOG_DEBUG, "device \"%s\" is already dimmed to \"%d\", aborting action \"%s\"", data->device, data->from_dimlevel, action_dim->name);
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
					OUT_OF_MEMORY
				}
				timer_req->data = data;
				uv_timer_init(uv_default_loop(), timer_req);
				assert(tv.tv_sec > 0 || tv.tv_usec > 0);
				uv_timer_start(timer_req, (void (*)(uv_timer_t *))thread, tv.tv_sec*1000+tv.tv_usec, 0);
				goto end;
			}
		};
		case EXECUTE: {
			if(data->seconds_in == 0) {
				struct timeval tv;
				strcpy(state, "on");

				struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
				if(data1 == NULL) {
					OUT_OF_MEMORY
				}
				if((data1->dev = STRDUP(data->device)) == NULL) {
					OUT_OF_MEMORY
				}
				if((data1->state = STRDUP(state)) == NULL) {
					OUT_OF_MEMORY
				}
				data1->values = json_mkobject();
				json_append_member(data1->values, "dimlevel", json_mknumber(data->to_dimlevel, 0));

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
						OUT_OF_MEMORY
					}
					timer_req->data = data;
					uv_timer_init(uv_default_loop(), timer_req);
					assert(tv.tv_sec > 0 || tv.tv_usec > 0);
					uv_timer_start(timer_req, (void (*)(uv_timer_t *))thread, tv.tv_sec*1000+tv.tv_usec, 0);
					goto end;
				}
				goto close;
			}
		}
		case EXECUTESTEPS: {
			strcpy(state, "on");

			struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY
			}
			if((data1->dev = STRDUP(data->device)) == NULL) {
				OUT_OF_MEMORY
			}
			if((data1->state = STRDUP(state)) == NULL) {
				OUT_OF_MEMORY
			}
			data1->values = json_mkobject();

			json_append_member(data1->values, "dimlevel", json_mknumber(data->from_dimlevel, 0));

			eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);

			if(data->direction == 1) {
				data->from_dimlevel++;
			} else if(data->direction == 0) {
				data->from_dimlevel--;
			}

			if((data->direction == 1 && data->from_dimlevel <= data->to_dimlevel) ||
			 (data->direction == 0 && data->from_dimlevel >= data->to_dimlevel)) {

				uv_timer_t *timer_req = NULL;
				if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
					OUT_OF_MEMORY
				}
				timer_req->data = data;
				uv_timer_init(uv_default_loop(), timer_req);
				assert(data->interval.tv_sec > 0 || data->interval.tv_usec > 0);
				uv_timer_start(timer_req, (void (*)(uv_timer_t *))thread, (data->interval.tv_sec*1000+(data->interval.tv_sec)), 0);

				goto end;
			} else {
				if(data->seconds_for > 0) {
					struct timeval tv;
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
						OUT_OF_MEMORY
					}
					timer_req->data = data;
					uv_timer_init(uv_default_loop(), timer_req);
					assert(tv.tv_sec > 0 || tv.tv_usec > 0);
					uv_timer_start(timer_req, (void (*)(uv_timer_t *))thread, tv.tv_sec*1000+tv.tv_usec, 0);
					goto end;
				}
			}
		} break;
		case RESTORE: {
			struct reason_control_device_t *data1 = MALLOC(sizeof(struct reason_control_device_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY
			}
			if((data1->dev = STRDUP(data->device)) == NULL) {
				OUT_OF_MEMORY
			}
			if((data1->state = STRDUP(data->old_state)) == NULL) {
				OUT_OF_MEMORY
			}
			data1->values = json_mkobject();

			json_append_member(data1->values, "dimlevel", json_mknumber(data->old_dimlevel, 0));

			eventpool_trigger(REASON_CONTROL_DEVICE, reason_control_device_free, data1);

		} break;
	}

close:
	FREE(data->device);
	FREE(data);
	return;

end:
	return;
}

static void thread_free(uv_work_t *req, int status) {
	FREE(req);
}

static void prepare(struct rules_actions_t *obj, char *dev) {
	struct JsonNode *json = obj->parsedargs;
	struct JsonNode *jedimlevel = NULL;
	struct JsonNode *jsdimlevel = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jafter = NULL;
	struct JsonNode *jfor = NULL;
	struct JsonNode *jfrom = NULL;
	struct JsonNode *jin = NULL;
	struct JsonNode *javalues = NULL;
	struct JsonNode *jcvalues = NULL;
	struct JsonNode *jdvalues = NULL;
	struct JsonNode *jevalues = NULL;
	struct JsonNode *jfvalues = NULL;
	struct JsonNode *jaseconds = NULL;
	struct JsonNode *jiseconds = NULL;
	char *old_state = NULL, **array = NULL;
	double old_dimlevel = 0.0, new_dimlevel = 0.0, cur_dimlevel = 0.0;
	int seconds_after = 0, seconds_for = 0, seconds_in = 0, has_in = 0, dimdiff = 0;
	int type_for = 0, type_after = 0, type_in = 0;
	int direction = 0;
	int	l = 0, i = 0, nrunits = (sizeof(units)/sizeof(units[0]));
	char state[3];

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

	if((jin = json_find_member(json, "IN")) != NULL) {
		if((jfvalues = json_find_member(jin, "value")) != NULL) {
			jiseconds = json_find_element(jfvalues, 0);
			if(jiseconds != NULL) {
				if(jiseconds->tag == JSON_STRING) {
					l = explode(jiseconds->string_, " ", &array);
					if(l == 2) {
						for(i=0;i<nrunits;i++) {
							if(strcmp(array[1], units[i].name) == 0) {
								seconds_in = atoi(array[0]);
								type_in = units[i].id;
								has_in = 1;
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


	if(pilight.debuglevel >= 2) {
		fprintf(stderr, "action dim: for %d:%d, after %d:%d, in: %d:%d\n", seconds_for, type_for, seconds_after, type_after, seconds_in, type_in);
	}

	/* Store current state and dimlevel */
	if(devices_select_string_setting(ORIGIN_ACTION, dev, "state", &old_state) != 0) {
		logprintf(LOG_NOTICE, "could not store old state of \"%s\"", dev);
		goto end;
	}
	if(devices_select_number_setting(ORIGIN_ACTION, dev, "dimlevel", &cur_dimlevel, NULL) != 0) {
		logprintf(LOG_NOTICE, "could not store old dimlevel of \"%s\"", dev);
		goto end;
	}

	if((jfrom = json_find_member(json, "FROM")) != NULL) {
		if((jevalues = json_find_member(jfrom, "value")) != NULL) {
			jsdimlevel = json_find_element(jevalues, 0);
			if(jsdimlevel != NULL) {
				if(jsdimlevel->tag == JSON_NUMBER) {
					old_dimlevel = (int)jsdimlevel->number_;
				}
			}
		}
	}

	if((jto = json_find_member(json, "TO")) != NULL) {
		if((javalues = json_find_member(jto, "value")) != NULL) {
			jedimlevel = json_find_element(javalues, 0);
			if(jedimlevel != NULL) {
				if(jedimlevel->tag == JSON_NUMBER) {
					new_dimlevel = (int)jedimlevel->number_;
					strcpy(state, "on");
				}
			}
		}
	}

	if(pilight.debuglevel >= 2) {
		fprintf(stderr, "action dim: old %d, new %d, direction %d\n", (int)old_dimlevel, (int)new_dimlevel, direction);
	}

	if(old_dimlevel < new_dimlevel) {
		direction = INCREASING;
	} else if(old_dimlevel > new_dimlevel) {
		direction = DECREASING;
	}

	dimdiff = abs((int)(new_dimlevel - old_dimlevel));

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	memset(&data->interval, 0, sizeof(struct timeval));

	if(has_in == 1) {
		calc_time_interval(type_in, seconds_in, dimdiff, &data->interval);
	}

	if((data->device = MALLOC(strlen(dev)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(data->device, dev);
	data->old_dimlevel = cur_dimlevel;
	data->from_dimlevel = old_dimlevel;
	data->to_dimlevel = new_dimlevel;
	data->direction = direction;
	data->seconds_for = seconds_for;
	data->type_for = type_for;
	data->seconds_in = seconds_in;
	data->type_in = type_in;
	data->seconds_after = seconds_after;
	data->type_after = type_after;
	data->steps = INIT;
	strcpy(data->old_state, old_state);

	data->exec = event_action_set_execution_id(dev);

	uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
	if(tp_work_req == NULL) {
		OUT_OF_MEMORY
	}
	tp_work_req->data = data;
	if(uv_queue_work(uv_default_loop(), tp_work_req, "action_dim", thread, thread_free) < 0) {
		FREE(tp_work_req);
	}

end:
	return;
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

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionDimInit(void) {
	event_action_register(&action_dim, "dim");

	options_add(&action_dim->options, 'a', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_dim->options, 'b', "TO", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&action_dim->options, 'c', "FROM", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&action_dim->options, 'd', "FOR", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_dim->options, 'e', "AFTER", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_dim->options, 'f', "IN", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_dim->run = &run;
	action_dim->gc = &gc;
	action_dim->checkArguments = &checkArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "dim";
	module->version = "4.0.1";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionDimInit();
}
#endif
