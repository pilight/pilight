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
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../core/options.h"
#include "../core/json.h"
#include "../core/ssdp.h"
#include "../core/socket.h"

#include "../protocols/protocol.h"

#include "../config/rules.h"
#include "../config/settings.h"
#include "../config/devices.h"

#include "events.h"

#include "operator.h"
#include "function.h"
#include "action.h"

#define NONE 0
#define AND	2
#define OR 1

static unsigned short loop = 1;
static char true_[2];
static char false_[2];
static char dot_[2];

static char *recvBuff = NULL;
static int sockfd = 0;

static pthread_mutex_t events_lock;
static pthread_cond_t events_signal;
static pthread_mutexattr_t events_attr;
static unsigned short eventslock_init = 0;

typedef struct eventsqueue_t {
	struct JsonNode *jconfig;
	struct eventsqueue_t *next;
} eventsqueue_t;

static struct eventsqueue_t *eventsqueue;
static struct eventsqueue_t *eventsqueue_head;
static int eventsqueue_number = 0;
static int running = 0;

int events_gc(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	loop = 0;

	if(eventslock_init == 1) {
		pthread_mutex_unlock(&events_lock);
		pthread_cond_signal(&events_signal);
	}

	while(running == 1) {
		usleep(10);
	}

	event_operator_gc();
	event_action_gc();
	event_function_gc();
	logprintf(LOG_DEBUG, "garbage collected events library");
	return 1;
}

void event_cache_device(struct rules_t *obj, char *device) {
	int exists = 0;
	int o = 0;

	if(obj != NULL) {
		for(o=0;o<obj->nrdevices;o++) {
			if(strcmp(obj->devices[o], device) == 0) {
				exists = 1;
				break;
			}
		}
		if(exists == 0) {
			/* Store all devices that are present in this rule */
			if((obj->devices = REALLOC(obj->devices, sizeof(char *)*(unsigned int)(obj->nrdevices+1))) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			if((obj->devices[obj->nrdevices] = MALLOC(strlen(device)+1)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			strcpy(obj->devices[obj->nrdevices], device);
			obj->nrdevices++;
		}
	}
}

static int event_store_val_ptr(struct rules_t *obj, char *device, char *name, struct devices_settings_t *settings) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct rules_values_t *tmp_values = obj->values;
	int match = 0;
	while(tmp_values) {
		if(strcmp(tmp_values->device, device) == 0 &&
		   strcmp(tmp_values->name, name) == 0) {
				match = 1;
				break;
		}
		tmp_values = tmp_values->next;
	}

	if(match == 0) {
		if((tmp_values = MALLOC(sizeof(rules_values_t))) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		tmp_values->next = NULL;
		if((tmp_values->name = MALLOC(strlen(name)+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			FREE(tmp_values);
			exit(EXIT_FAILURE);
		}
		strcpy(tmp_values->name, name);
		if((tmp_values->device = MALLOC(strlen(device)+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			FREE(tmp_values->name);
			FREE(tmp_values);
			exit(EXIT_FAILURE);
		}
		strcpy(tmp_values->device, device);
		tmp_values->settings = settings;
		tmp_values->next = obj->values;
		obj->values = tmp_values;
	}
	return 0;
}

/* This functions checks if the defined event variable
   is part of one of devices in the config. If it is,
   replace the variable with the actual value */

/*
	Return codes:
	-1: An error was found and abort rule parsing
	0: Found variable and filled varcont
	1: Did not find variable and did not fill varcont
*/
int event_lookup_variable(char *var, struct rules_t *obj, int type, struct varcont_t *varcont, int *rtype, unsigned short validate, enum origin_t origin) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	int cached = 0;
	if(strcmp(true_, "1") != 0) {
		strcpy(true_, "1");
	}
	if(strcmp(dot_, ".") != 0) {
		strcpy(dot_, ".");
	}
	if(strcmp(false_, "1") != 0) {
		strcpy(false_, "1");
	}
	if(strcmp(var, "1") == 0 || strcmp(var, "0") == 0 ||
		 strcmp(var, "true") == 0 ||
		 strcmp(var, "false") == 0) {
		if(type == JSON_NUMBER) {
			if(strcmp(var, "true") == 0) {
				varcont->number_ = 1;
			} else if(strcmp(var, "false") == 0) {
				varcont->number_ = 0;
			} else {
				varcont->number_ = atof(var);
			}
			varcont->decimals_ = 0;
			*rtype = JSON_NUMBER;
		} else if(type == JSON_STRING) {
			if(strcmp(var, "true") == 0) {
				varcont->string_ = true_;
			} else if(strcmp(var, "false") == 0) {
				varcont->string_ = false_;
			} else {
				varcont->string_ = var;
			}
			*rtype = JSON_STRING;
		}
		return 0;
	}

	int len = (int)strlen(var);
	int i = 0;
	int nrdots = 0;
	for(i=0;i<len;i++) {
		if(var[i] == '.') {
			nrdots++;
		}
	}

	if(nrdots == 1) {
		char **array = NULL;
		unsigned int n = explode(var, ".", &array);

		if(n < 2) {
			varcont->string_ = dot_;
			array_free(&array, n);
			*rtype = JSON_STRING;
			return 0;
		}

		char device[strlen(array[0])+1];
		strcpy(device, array[0]);

		char name[strlen(array[1])+1];
		strcpy(name, array[1]);

		array_free(&array, n);

		/* Check if the values are not already cached */
		if(obj != NULL) {
			struct rules_values_t *tmp_values = obj->values;
			while(tmp_values) {
				if(strcmp(tmp_values->device, device) == 0 &&
					 strcmp(tmp_values->name, name) == 0) {
						if(tmp_values->settings->values->type != type && (type != (JSON_NUMBER | JSON_STRING))) {
							if(type == JSON_STRING) {
								logprintf(LOG_ERR, "rule #%d invalid: trying to compare a integer variable \"%s.%s\" to a string", obj->nr, device, name);
								*rtype = -1;
								return -1;
							} else if(type == JSON_NUMBER) {
								logprintf(LOG_ERR, "rule #%d invalid: trying to compare a string variable \"%s.%s\" to an integer", obj->nr, device, name);
								*rtype = -1;
								return -1;
							}
						}
						if(tmp_values->settings->values->type == JSON_STRING) {
							varcont->string_ = tmp_values->settings->values->string_;
							*rtype = JSON_STRING;
						} else if(tmp_values->settings->values->type == JSON_NUMBER) {
							varcont->number_ = tmp_values->settings->values->number_;
							varcont->decimals_ = tmp_values->settings->values->decimals;
							*rtype = JSON_NUMBER;
						}
						cached = 1;
						return 0;
				}
				tmp_values = tmp_values->next;
			}
		}
		struct devices_t *dev = NULL;
		if(cached == 0) {
			unsigned int match1 = 0, match2 = 0, match3 = 0, has_state = 0;

			/* We first check if we matched a received protocol */
			struct protocols_t *tmp_protocols = protocols;
			while(tmp_protocols) {
				if(protocol_device_exists(tmp_protocols->listener, device) == 0) {
					match1 = 1;
					break;
				}
				tmp_protocols = tmp_protocols->next;
			}
			if(match1 == 1) {
				if(validate == 1) {
					if(origin == RULE) {
						event_cache_device(obj, device);
					}
					if(strcmp(name, "repeats") != 0 && strcmp(name, "uuid") != 0) {
						struct options_t *options = tmp_protocols->listener->options;
						while(options) {
							if(options->conftype == DEVICES_STATE) {
								has_state = 1;
							}
							if(strcmp(options->name, name) == 0) {
								if(options->vartype != type) {
									if(options->vartype == JSON_STRING) {
										logprintf(LOG_ERR, "rule #%d invalid: trying to compare a string variable \"%s.%s\" to an integer", obj->nr, device, name);
									} else {
										logprintf(LOG_ERR, "rule #%d invalid: trying to compare an integer variable \"%s.%s\" to a string", obj->nr, device, name);
									}
									varcont->string_ = NULL;
									varcont->number_ = 0;
									varcont->decimals_ = 0;
									*rtype = -1;
									return -1;
								}
								match2 = 1;
							}
							options = options->next;
						}
						if(match2 == 0 && ((!(strcmp(name, "state") == 0 && has_state == 1)) || (strcmp(name, "state") != 0))) {
							logprintf(LOG_ERR, "rule #%d invalid: protocol \"%s\" has no field \"%s\"", obj->nr, device, name);
							varcont->string_ = NULL;
							varcont->number_ = 0;
							varcont->decimals_ = 0;
							*rtype = -1;
							return -1;
						}
					} else if(!(strcmp(name, "repeats") == 0 || strcmp(name, "uuid") == 0)) {
						logprintf(LOG_ERR, "rule #%d invalid: protocol \"%s\" has no field \"%s\"", obj->nr, device, name);
						varcont->string_ = NULL;
						varcont->number_ = 0;
						varcont->decimals_ = 0;
						*rtype = -1;
						return -1;
					}
				}

				struct JsonNode *jmessage = NULL, *jnode = NULL;
				if(obj->jtrigger != NULL) {
					if(((jnode = json_find_member(obj->jtrigger, name)) != NULL) ||
					   ((jmessage = json_find_member(obj->jtrigger, "message")) != NULL &&
						 (jnode = json_find_member(jmessage, name)) != NULL)) {
						if(jnode->tag == JSON_STRING) {
							varcont->string_ = jnode->string_;
							*rtype = JSON_STRING;
							return 0;
						} else if(jnode->tag == JSON_NUMBER) {
							varcont->number_ = jnode->number_;
							varcont->decimals_ = jnode->decimals_;
							*rtype = JSON_NUMBER;
							return 0;
						}
					}
				}
				*rtype = -1;
				return 1;
			} else if(devices_get(device, &dev) == 0) {
				if(validate == 1) {
					if(origin == RULE) {
						event_cache_device(obj, device);
					}
					struct protocols_t *tmp = dev->protocols;
					while(tmp) {
						struct options_t *opt = tmp->listener->options;
						while(opt) {
							if(opt->conftype == DEVICES_STATE && strcmp("state", name) == 0) {
								match1 = 1;
								match2 = 1;
								match3 = 1;
								break;
							} else if(strcmp(opt->name, name) == 0) {
								match1 = 1;
								if(opt->vartype == (JSON_NUMBER | JSON_STRING)) {
									break;
								}
								if(opt->conftype == DEVICES_VALUE || opt->conftype == DEVICES_STATE || opt->conftype == DEVICES_SETTING) {
									match2 = 1;
									if(type == (JSON_STRING | JSON_NUMBER)) {
										match3 = 1;
									} else if(opt->vartype == JSON_STRING && type == JSON_STRING) {
										match3 = 1;
									} else if(opt->vartype == JSON_NUMBER && type == JSON_NUMBER) {
										match3 = 1;
									}
									break;
								}
							}
							opt = opt->next;
						}
						tmp = tmp->next;
					}
					if(match1 == 0) {
						logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" has no variable \"%s\"", obj->nr, device, name);
					} else if(match2 == 0) {
						logprintf(LOG_ERR, "rule #%d invalid: variable \"%s\" of device \"%s\" cannot be used in event rules", obj->nr, name, device);
					} else if(match3 == 0) {
						logprintf(LOG_ERR, "rule #%d invalid: trying to compare a integer variable \"%s.%s\" to a string", obj->nr, device, name);
					}
					if(match1 == 0 || match2 == 0 || match3 == 0) {
						varcont->string_ = NULL;
						varcont->number_ = 0;
						varcont->decimals_ = 0;
						*rtype = -1;
						return -1;
					}
				}
				struct devices_settings_t *tmp_settings = dev->settings;
				while(tmp_settings) {
					if(strcmp(tmp_settings->name, name) == 0) {
						if(tmp_settings->values->type == JSON_STRING) {
							if(type == JSON_STRING || type == (JSON_NUMBER | JSON_STRING)) {
								/* Cache values for faster future lookup */
								if(obj != NULL) {
									event_store_val_ptr(obj, device, name, tmp_settings);
								}
								varcont->string_ = tmp_settings->values->string_;
								*rtype = JSON_STRING;
								return 0;
							} else {
								logprintf(LOG_ERR, "rule #%d invalid: trying to compare integer variable \"%s.%s\" to a string", obj->nr, device, name);
								varcont->string_ = NULL;
								*rtype = -1;
								return -1;
							}
						} else if(tmp_settings->values->type == JSON_NUMBER) {
							if(type == JSON_NUMBER || type == (JSON_NUMBER | JSON_STRING)) {
								/* Cache values for faster future lookup */
								if(obj != NULL) {
									event_store_val_ptr(obj, device, name, tmp_settings);
								}
								varcont->number_ = tmp_settings->values->number_;
								varcont->decimals_ = tmp_settings->values->decimals;
								*rtype = JSON_NUMBER;
								return 0;
							} else {
								logprintf(LOG_ERR, "rule #%d invalid: trying to compare string variable \"%s.%s\" to an integer", obj->nr, device, name);
								varcont->number_ = 0;
								varcont->decimals_ = 0;
								*rtype = -1;
								return -1;
							}
						}
					}
					tmp_settings = tmp_settings->next;
				}
				logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" has no variable \"%s\"", obj->nr, device, name);
				varcont->string_ = NULL;
				varcont->number_ = 0;
				varcont->decimals_ = 0;
				*rtype = -1;
				return -1;
			} /*else {
				logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" does not exist in the config", obj->nr, device);
				varcont->string_ = NULL;
				varcont->number_ = 0;
				varcont->decimals_ = 0;
				return -1;
			}*/
		}
	}/* else if(nrdots > 2) {
		logprintf(LOG_ERR, "rule #%d invalid: variable \"%s\" is invalid", obj->nr, var);
		varcont->string_ = NULL;
		varcont->number_ = 0;
		varcont->decimals_ = 0;
		return -1;
	} */

	if(type == JSON_STRING) {
		if(isNumeric(var) == 0) {
			varcont->string_ = NULL;
			logprintf(LOG_ERR, "rule #%d invalid: trying to compare integer variable \"%s\" to a string", obj->nr, var);
			*rtype = -1;
			return -1;
		} else {
			varcont->string_ = var;
			*rtype = JSON_STRING;
		}
	} else if(type == JSON_NUMBER) {
		if(isNumeric(var) == 0) {
			varcont->number_ = atof(var);
			varcont->decimals_ = nrDecimals(var);
			*rtype = JSON_NUMBER;
		} else {
			logprintf(LOG_ERR, "rule #%d invalid: trying to compare string variable \"%s\" to an integer", obj->nr, var);
			varcont->number_ = 0;
			varcont->decimals_ = 0;
			*rtype = -1;
			return -1;
		}
	} else if(type == (JSON_STRING | JSON_NUMBER)) {
		*rtype = -1;
		return 1;
	}
	return 0;
}

static int event_parse_hooks(char **rule, struct rules_t *obj, int depth, unsigned short validate) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	int hooks = 0, res = 0;
	unsigned long buflen = MEMBUFFER, len = 0, i = 0;
	char *subrule = MALLOC(buflen);
	char *tmp = *rule;

	if(subrule == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	while(tmp[i] != '\0') {
		if(tmp[i] == '(') {
			hooks++;
		}
		if(tmp[i] == ')') {
			hooks--;
		}

		if(hooks > 0) {
			subrule[len++] = tmp[i+1];
			if(buflen <= len) {
				buflen *= 2;
				if((subrule = REALLOC(subrule, buflen)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				memset(&subrule[len], '\0', buflen-(unsigned long)len);
			}
		} else {
			if(len > 0) {
				unsigned long z = 1;
				char *replace = MALLOC(len+3);

				subrule[len-1] = '\0';

				sprintf(replace, "(%s)", subrule);
				replace[len+2] = '\0';
				/* If successful, the variable subrule will be
				   replaced with the outcome of the formula */
				if((res = event_parse_rule(subrule, obj, depth, validate)) > -1) {
					z = strlen(subrule);
					/* Remove the hooks and the content within.
						 Replace this content with the result of the sum.

					   e.g.: ((1 + 2) + (3 + 4))
									 (   3    +    7   )
					*/
					if(pilight.debuglevel == 1) {
						fprintf(stderr, "replace %s with %s in %s\n", replace, subrule, tmp);
					}
					str_replace(replace, subrule, &tmp);

					FREE(replace);
					break;
				} else {
					FREE(subrule);
					FREE(replace);
					return -1;
				}
				memset(subrule, '\0', (unsigned long)len);
				i -= (len-z);
				len = 0;
			}
		}
		i++;
	}
	FREE(subrule);
	return 0;
}


static int event_remove_hooks(char **rule, struct rules_t *obj, int depth) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	int hooks = 0;
	char *tmp = *rule;
	unsigned long len = strlen(tmp), i = 0;

	while(tmp[i] != '\0') {
		if(tmp[i] == '(') {
			hooks++;
		}
		if(tmp[i] == ')') {
			hooks--;
		}

		if(hooks == 0) {
			memmove(&tmp[0], &tmp[i], len-i);
			tmp[0] = '0';
			tmp[len-i] = '\0';
			break;
		}
		i++;
	}
	return 0;
}

static int event_parse_function(char **rule, struct rules_t *obj, unsigned short validate, enum origin_t origin) {
	struct JsonNode *arguments = NULL;
	struct event_functions_t *tmp_function = event_functions;
	char *tmp = *rule, *ca = NULL;
	char *ohook = strstr(tmp, "("), *chook = strstr(tmp, ")");
	char *subfunction = NULL, *function = NULL, *name = NULL, *output = NULL;
	size_t pos1 = 0, pos2 = 0, pos3 = 0, pos4 = 0, pos5 = 0;
	unsigned long buflen = MEMBUFFER;
	int	error = 0, i = 0, fl = 0, nl = 0;
	int hooks = 0, len = 0, nested = 0;

	if(ohook == NULL && chook == NULL) {
		return 1;
	} else if(!(ohook != NULL && chook != NULL)) {
		return -1;
	}

	pos1 = pos3 = (size_t)(ohook-tmp);
	pos2 = (size_t)(chook-tmp);
	while(pos1 > 0 && tmp[pos1-1] != ' ') {
		pos1--;
	}

	/*
		Check if we have nested function.
		We validate these function by checking if the
		opening hook is within the first closing hook.
		E.g.

		Nested:
		DATE_FORMAT(DATE_ADD(2014-01-01 23:59:59, INTERVAL 1 SECOND), "%H.%s)

		Not nested:
		DATE_FORMAT(2014-01-01, %Y-%m-%d, %d-%m-%Y) == DATE_FORMAT(2014-01-01, %Y-%m-%d, %d-%m-%Y)
	*/
	if((ca = strstr(&tmp[pos3+1], "(")) != NULL) {
		pos5 = (ca-&tmp[pos3+1]);
		if((pos3+1+pos5) < pos2) {
			nested = 1;
		}
	}
	while(nested == 1) {
		if((subfunction = MALLOC(buflen)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}

		hooks = 0;
		i = 0;
		buflen = BUFFER_SIZE;
		len = 0;

		while(tmp[i] != '\0') {
			if(tmp[i] == '(') {
				hooks++;
			}
			if(tmp[i] == ')') {
				hooks--;
			}
			if(hooks > 0) {
				subfunction[len++] = tmp[i+1];
				if(buflen <= len) {
					buflen *= 2;
					subfunction = REALLOC(subfunction, buflen);
					memset(&subfunction[len], '\0', buflen-(unsigned long)len);
				}
			} else {
				if(len > 0) {
					while(tmp[i-1] != ')') {
						i--;
						len--;
					}
					unsigned long z = 1;
					char *replace = MALLOC(len+1);

					subfunction[len-1] = '\0';
					strcpy(replace, subfunction);

					if(event_parse_function(&subfunction, obj, validate, origin) > -1) {
						z = strlen(subfunction);
						/* Remove the nested function and the content within.
							 Replace this function with the result of the function.

							 e.g.: DATE_FORMAT(DATE_ADD(2014-01-01 23:59:59, INTERVAL 1 SECOND), "%H.%s)
										 DATE_FORMAT(2014-01-02 00:00:00, %H.%s)
						*/

						if(pilight.debuglevel == 1) {
							fprintf(stderr, "replace %s with %s in %s\n", replace, subfunction, tmp);
						}
						str_replace(replace, subfunction, &tmp);

						chook = strstr(tmp, ")");
						pos2 = (size_t)(chook-tmp);

						FREE(replace);
					} else {
						FREE(subfunction);
						FREE(replace);
						return -1;
					}
					memset(subfunction, '\0', (unsigned long)len);
					i -= (len-z);
					len = 0;
				}
			}
			i++;
		}
		nested = 0;
		if((ca = strstr(&tmp[pos3+1], "(")) != NULL) {
			pos5 = (ca-&tmp[pos3+1]);
			if((pos3+1+pos5) < pos2) {
				nested = 1;
			}
		}
		FREE(subfunction);
	}

	fl = (pos2-pos1)+1;
	nl = (pos3-pos1);

	if((function = MALLOC(fl+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	memset(function, '\0', fl+1);
	strncpy(function, &tmp[pos1], fl);
	function[fl] = '\0';

	if((name = MALLOC(nl+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	memset(name, '\0', nl+1);
	strncpy(name, &tmp[pos1], nl);
	name[nl] = '\0';

	arguments = json_mkarray();
	pos4 = pos3+1;
	for(i=pos3+1;i<(pos3+(fl-nl));i++) {
		if(tmp[i] == ',') {
			while(tmp[i] == ' ') {
				i++;
			}
			tmp[i] = '\0';
			json_append_element(arguments, json_mkstring(&tmp[pos4]));
			tmp[i] = ',';
			i++;
			while(tmp[i] == ' ') {
				i++;
			}
			pos4 = i;
		}
	}
	int t = tmp[i-1];
	tmp[i-1] = '\0';
	json_append_element(arguments, json_mkstring(&tmp[pos4]));
	tmp[i-1] = t;

	output = MALLOC(BUFFER_SIZE);
	memset(output, '\0', BUFFER_SIZE);

	int match = 0;
	while(tmp_function) {
		if(error == 0) {
			if(strcmp(name, tmp_function->name) == 0) {
				if(tmp_function->run != NULL) {
					error = tmp_function->run(obj, arguments, &output, origin);
					match = 1;
					break;
				}
			}
		} else {
			goto close;
		}
		tmp_function = tmp_function->next;
	}
	if(match == 0) {
		logprintf(LOG_ERR, "rule #%d invalid: function \"%s\" does not exist", obj->nr, name);
		error = -1;
		goto close;
	}

	if(strlen(output) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "replace %s with %s in %s\n", function, output, *rule);
		}
		str_replace(function, output, rule);
	}

close:
	json_delete(arguments);
	FREE(output);
	FREE(name);
	FREE(function);
	return error;
}

static int event_parse_formula(char **rule, struct rules_t *obj, int depth, unsigned short validate) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct varcont_t v1;
	struct varcont_t v2;
	char *var1 = NULL, *func = NULL, *var2 = NULL, *tmp = *rule, *search = NULL;
	int element = 0, i = 0, match = 0, error = 0, hasquote = 0, hadquote = 0, rtype1 = 0, rtype2 = 0;
	char var1quotes[2], var2quotes[2], funcquotes[2];
	unsigned long len = strlen(tmp), pos = 0, word = 0;
	char *res = MALLOC(255);

	memset(res, '\0', 255);
	var1quotes[0] = '\0';
	var2quotes[0] = '\0';
	funcquotes[0] = '\0';

	/* If there are no hooks, we are going to solve the whole (sub)formula.
	   This formula can be as simple as:
	   e.g.: 1 + 2
	   or more complex as:
	   e.g.: 1 AND 1 AND 0 AND 1
			 1 AND location.device.state IS on
	*/
	if(event_parse_function(&tmp, obj, validate, RULE) == -1) {
		return -1;
	}

	while(pos <= len) {
		/* If we encounter a space we have the formula part */
		if(tmp[pos] == '"') {
			hasquote ^= 1;
			if(hasquote == 1) {
				word++;
			} else {
				hadquote = 1;
			}
		}
		if(hasquote == 0 && (tmp[pos] == ' ' || pos == len)) {
			switch(element) {
				/* The first value of three is always the first variable.
				   The second value is always the function.
				   The third value of the three is always the second variable (or second formula).
				*/
				case 0:
					if(hadquote == 1) {
						strcpy(var1quotes, "\"");
					}
					var1 = REALLOC(var1, ((pos-word)+1));
					memset(var1, '\0', ((pos-word)+1));
					strncpy(var1, &tmp[word], (pos-word)-hadquote);
				break;
				case 1:
					if(hadquote == 1) {
						strcpy(funcquotes, "\"");
					}
					func = REALLOC(func, ((pos-word)+1));
					memset(func, '\0', ((pos-word)+1));
					strncpy(func, &tmp[word], (pos-word)-hadquote);
				break;
				case 2:
					if(hadquote == 1) {
						strcpy(var2quotes, "\"");
					}
					var2 = REALLOC(var2, ((pos-word)+1));
					memset(var2, '\0', ((pos-word)+1));
					strncpy(var2, &tmp[word], (pos-word)-hadquote);

					unsigned long l = (strlen(var1)+strlen(var2)+strlen(func))+2;
					/* search parameter, null-terminator, and potential quotes */
					search = REALLOC(search, (l+1+6));
					memset(search, '\0', l+1);
					l = (unsigned long)sprintf(search, "%s%s%s %s%s%s %s%s%s",
						var1quotes, var1, var1quotes,
						funcquotes, func, funcquotes,
						var2quotes, var2, var2quotes);
				break;
				default:;
			}
			hadquote = 0;
			word = pos+1;
			element++;
			if(element > 2) {
				element = 0;
			}
		}

		if(func != NULL && strlen(func) > 0 &&
		   var1 != NULL  && strlen(var1) > 0 &&
		   var2 != NULL  && strlen(var2) > 0) {
				var1quotes[0] = '\0';
				var2quotes[0] = '\0';
				funcquotes[0] = '\0';
			// printf("%s / %s / %s\n", var1, func, var2);
			i = 0;
			/* Check if the operator exists */
			match = 0;
			struct event_operators_t *tmp_operator = event_operators;
			while(tmp_operator) {
				int type = 0;
				if(tmp_operator->callback_number) {
					type = JSON_NUMBER;
				} else {
					type = JSON_STRING;
				}

				if(strcmp(func, tmp_operator->name) == 0) {
					match = 1;
					int ret1 = 0, ret2 = 0;
					if(tmp_operator->callback_string != NULL) {
						ret1 = event_lookup_variable(var1, obj, type, &v1, &rtype1, validate, RULE);
						ret2 = event_lookup_variable(var2, obj, type, &v2, &rtype2, validate, RULE);
						if(rtype1 != type || rtype2 != type) {
							if(ret1 == 1 || ret2 == 1) {
								error = 0;
							} else {
								error = -1;
							}
							goto close;
						} else if(ret1 == -1 || ret2 == -1) {
							error = -1;
							goto close;
						} else if(v1.string_ != NULL && v2.string_ != NULL) {
							/* Solve the formula */
							tmp_operator->callback_string(v1.string_, v2.string_, &res);
						} else {
							error = 0;
							goto close;
						}
					} else if(tmp_operator->callback_number != NULL) {
						/* Continue with regular numeric operator parsing */
						ret1 = event_lookup_variable(var1, obj, type, &v1, &rtype1, validate, RULE);
						ret2 = event_lookup_variable(var2, obj, type, &v2, &rtype2, validate, RULE);
						if(rtype1 != type || rtype2 != type) {
							if(ret1 == 1 || ret2 == 1) {
								error = 0;
							} else {
								error = -1;
							}
							goto close;
						} else if(ret1 == -1 || ret2 == -1) {
							error = -1;
							goto close;
						} else if(ret1 == 1 || ret2 == 1) {
							error = 0;
							goto close;
						} else {
							/* Solve the formula */
							tmp_operator->callback_number(v1.number_, v2.number_, &res);
						}
					}
					if(res != 0) {
						/* Replace the subpart of the formula with the solutions in case of simple AND's
						   e.g.: 0 AND 1 AND 1 AND 0
								     0 AND 1 AND 0
						*/
						char *p = MALLOC(strlen(res)+1);
						strcpy(p, res);
						unsigned long r = 0;
						// printf("replace %s with %s in %s\n", search, p, tmp);
						if((r = (unsigned long)str_replace(search, p, &tmp)) == -1) {
							logprintf(LOG_ERR, "rule #%d: an unexpected error occured while parsing", obj->nr);
							FREE(p);
							error = -1;
							goto close;
						}
						len = r;
						FREE(p);
					}
					pos = 0, word = 0, element = 0;
					break;
				}
				i++;
				tmp_operator = tmp_operator->next;
			}
			if(match == 0) {
				logprintf(LOG_ERR, "rule #%d invalid: operator \"%s\" does not exist", obj->nr, func);
				error = -1;
				goto close;
			}
			if(var1 != NULL ) {
				memset(var1, '\0', strlen(var1));
			} if(func != NULL ) {
				memset(func, '\0', strlen(func));
			} if(var2 != NULL ) {
				memset(var2, '\0', strlen(var2));
			} if(search != NULL ) {
				memset(search, '\0', strlen(search));
			}
		}
		pos++;
	}
	if(element == 2) {
		logprintf(LOG_ERR, "rule #%d invalid: could not parse \"%s\"", obj->nr, *rule);
		error = -1;
		goto close;
	}

close:
	FREE(res);
	if(var1 != NULL ) {
		FREE(var1);
		var1 = NULL;
	} if(func != NULL ) {
		FREE(func);
		func = NULL;
	} if(var2 != NULL ) {
		FREE(var2);
		var2 = NULL;
	} if(search != NULL ) {
		FREE(search);
		search = NULL;
	}

	return error;
}

static int event_parse_action_arguments(char *arguments, struct rules_t *obj, int validate) {
	struct varcont_t v;
	char *tmp = NULL;
	int i = 0, error = 0, a = 0, b = 0, len = strlen(arguments);
	int vallen = 0, ret = 0, nlen = 0, type = 0;

	while(arguments[i] != '\0' && i < len) {
		if(arguments[i] == '(') {
			if(!(arguments[i-1] == '(' || arguments[i-1] == ' ' || i == 0)) {
				/* Function */
				error = event_parse_function(&arguments, obj, validate, ACTION);
				int z = strlen(arguments);
				len -= (len-z);
			}
		}
		if(arguments[i] == '.') {
			a = i;
			b = i;
			while(a > 0 && arguments[a-1] != ' ') {
				a--;
			}
			while(b < len && arguments[b] != ' ') {
				b++;
			}
			if(a < b) {
				if((tmp = REALLOC(tmp, (b-a)+1)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				strncpy(tmp, &arguments[a], (b-a));
				tmp[(b-a)] = '\0';

				memset(&v, 0, sizeof(v));
				ret = event_lookup_variable(tmp, obj, JSON_STRING | JSON_NUMBER, &v, &type, validate, ACTION);

				if(ret == 0) {
					if(type == JSON_NUMBER) {
						vallen = snprintf(NULL, 0, "%.*f", v.decimals_, v.number_);
					} else if(type == JSON_STRING) {
						vallen = strlen(v.string_);
					} else {
						error = -1;
						break;
					}
					/*
					 * We need to store larger values then we have space for
					 */
					nlen = (len-(b-a))+vallen;
					if(len < nlen) {
						if((arguments = REALLOC(arguments, nlen)) == NULL) {
							fprintf(stderr, "out of memory\n");
							exit(EXIT_FAILURE);
						}
					}
					memmove(&arguments[a+vallen], &arguments[b], nlen-(a+vallen));

					if(type == JSON_NUMBER) {
						snprintf(&arguments[a], vallen+1, "%.*f", v.decimals_, v.number_);
					} else if(type == JSON_STRING) {
						snprintf(&arguments[a], vallen+1, "%s", v.string_);
					}

					i = a+vallen;
					len = nlen;


					/*
					 * snprintf adds an unwanted null terminator
					 * that we need to remove.
					 */
					arguments[a+vallen] = ' ';
					arguments[len] = '\0';
				} else if(ret == -1) {
					error = -1;
					break;
				}
			}
		}
		if(error == -1) {
			break;
		}
		i++;
	}
	if(tmp != NULL) {
		FREE(tmp);
	}
	return error;
}

static int event_parse_action(char *action, struct rules_t *obj, int validate) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct JsonNode *jvalue = NULL;
	struct options_t *opt = NULL;
	struct event_actions_t *tmp_actions = NULL;
	struct rules_actions_t *node = NULL;
	char *tmp = action, *search = NULL, *output = NULL;
	char *p = NULL, *name = NULL, *value = NULL, **array = NULL, *func = NULL;
	unsigned long len = strlen(tmp), pos = 0, pos1 = 0, offset = 0;
	int error = 0, order = 1, l = 0, i = 0, x = 0, nractions = 1, match = 0;

	while(pos < len) {
		if(strncmp(&tmp[pos], " AND ", 5) == 0) {
			l = explode(&tmp[pos+5], " ", &array);
			match = 0;
			if(l >= 1) {;
				tmp_actions = event_actions;
				while(tmp_actions) {
					if(strcmp(tmp_actions->name, array[0]) == 0) {
						nractions++;
						match = 1;
						break;
					}
					tmp_actions = tmp_actions->next;
				}
			}
			array_free(&array, l);
			if(match == 1) {
				memmove(&tmp[pos], &tmp[pos+4], len-pos-4);
				tmp[pos] = '\0';
				len -= 4;
				tmp[len] = '\0';
			}
		}
		pos++;
	}

	pos = 0;
	len = 0;
	for(x=0;x<nractions;x++) {
		match = 0;
		node = obj->actions;
		while(node) {
			if(node->nr == x) {
				match = 1;
				break;
			}
			node = node->next;
		}

		if(match == 0) {
			if((node = MALLOC(sizeof(struct rules_actions_t))) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			node->nr = x;
			node->rule = obj;
			node->arguments = NULL;
			node->parsedargs = NULL;
			node->next = obj->actions;
			obj->actions = node;
		}

		order = 1;
		/* First extract the action name */
		if((p = strstr(&tmp[offset], " ")) != NULL) {
			pos1 = p-tmp;
			if((func = REALLOC(func, (pos1-offset)+1)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			strncpy(func, &tmp[offset], (pos1-offset));
			func[(pos1-offset)] = '\0';
		}

		pos1 = pos;
		match = 0;
		node->action = event_actions;
		while(node->action) {
			if(strcmp(node->action->name, func) == 0) {
				match = 1;
				break;
			}
			node->action = node->action->next;
		}

		if(match == 0) {
			logprintf(LOG_ERR, "action \"%s\" doesn't exists", func);
			error = 1;
			break;
		}

		memset(func, '\0', strlen(func));
		name = NULL;
		if(node->arguments == NULL && error == 0) {
			node->arguments = json_mkobject();
			len += strlen(&tmp[offset])+1;
			while(pos < len) {
				opt = node->action->options;
				while(opt) {
					if((search = REALLOC(search, strlen(opt->name)+3)) == NULL) {
						fprintf(stderr, "out of memory\n");
						exit(EXIT_FAILURE);
					}
					memset(search, '\0', strlen(opt->name)+3);
					sprintf(search, " %s ", opt->name);

					if(strncmp(&tmp[pos], search, strlen(search)) == 0) {
						pos++;
						if(pos1 < pos) {
							if((value = REALLOC(value, (pos-pos1)+1)) == NULL) {
								fprintf(stderr, "out of memory\n");
								exit(EXIT_FAILURE);
							}
							strncpy(value, &tmp[pos1], (pos-pos1));
							value[(pos-pos1)-1] = '\0';

							if(name != NULL && strlen(name) > 0 && strlen(value) > 0) {
								str_replace("\\", "", &value);

								struct JsonNode *jobj = json_mkobject();
								struct JsonNode *jvalues = json_mkarray();
								if(strstr(value, " AND ") != NULL) {
									l = explode(value, " AND ", &array), i = 0;
									for(i=0;i<l;i++) {
										if(isNumeric(array[i]) == 0) {
											json_append_element(jvalues, json_mknumber(atof(array[i]), 0));
										} else {
											json_append_element(jvalues, json_mkstring(array[i]));
										}
									}
									array_free(&array, l);
								} else {
									if(isNumeric(value) == 0) {
										json_append_element(jvalues, json_mknumber(atof(value), 0));
									} else {
										json_append_element(jvalues, json_mkstring(value));
									}
								}
								json_append_member(jobj, "value", jvalues);
								json_append_member(jobj, "order", json_mknumber(order, 0));
								json_append_member(node->arguments, name, jobj);
								order++;
							}
							memset(value, '\0', strlen(value));

							name = opt->name;
						}
						pos += strlen(search)-1;
						pos1 = pos;
						break;
					}
					opt = opt->next;
				}
				pos++;
			}

			if(search != NULL) {
				FREE(search);
			}
			if((value = REALLOC(value, (pos-pos1)+1)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}

			strncpy(value, &tmp[pos1], (pos-pos1));
			value[(pos-pos1)] = '\0';
			if(name != NULL && strlen(name) > 0 && strlen(value) > 0) {
				struct JsonNode *jobj = json_mkobject();
				struct JsonNode *jvalues = json_mkarray();
				if(strstr(value, " AND ") != NULL) {
					char **array = NULL;
					unsigned int l = explode(value, " AND ", &array), i = 0;
					for(i=0;i<l;i++) {
						if(isNumeric(array[i]) == 0) {
							json_append_element(jvalues, json_mknumber(atof(array[i]), 0));
						} else {
							json_append_element(jvalues, json_mkstring(array[i]));
						}
					}
					array_free(&array, l);
				} else {
					if(isNumeric(value) == 0) {
						json_append_element(jvalues, json_mknumber(atof(value), 0));
					} else {
						json_append_element(jvalues, json_mkstring(value));
					}
				}
				json_append_member(jobj, "value", jvalues);
				json_append_member(jobj, "order", json_mknumber(order, 0));
				json_append_member(node->arguments, name, jobj);
				order++;
			}
			memset(value, '\0', strlen(value));
			offset = len;
			if(error == 0) {
				output = json_stringify(node->arguments, NULL);
				if(node->parsedargs != NULL) {
					json_delete(node->parsedargs);
					node->parsedargs = NULL;
				}
				node->parsedargs = json_decode(output);
				jchild = json_first_child(node->parsedargs);
				while(jchild) {
					if((jvalue = json_find_member(jchild, "value")) != NULL) {
						jchild1 = json_first_child(jvalue);
						while(jchild1) {
							if(jchild1->tag == JSON_STRING) {
								if((error = event_parse_action_arguments(jchild1->string_, obj, validate)) == 0) {
									if(isNumeric(jchild1->string_) == 0) {
										int dec = nrDecimals(jchild1->string_);
										int nr = atof(jchild1->string_);
										json_free(jchild1->string_);
										jchild1->tag = JSON_NUMBER;
										jchild1->number_ = nr;
										jchild1->decimals_ = dec;
									}
								} else {
									break;
								}
							}
							jchild1 = jchild1->next;
						}
					}
					jchild = jchild->next;
				}
				if(error == 0) {
					struct options_t *opt = node->action->options;
					struct JsonNode *joption = NULL;
					struct JsonNode *jchild = NULL;
					while(opt) {
						if((joption = json_find_member(node->parsedargs, opt->name)) == NULL) {
							if(opt->conftype == DEVICES_VALUE && opt->argtype == OPTION_HAS_VALUE) {
								logprintf(LOG_ERR, "action \"%s\" is missing option \"%s\"", node->action->name, opt->name);
								error = 1;
								break;
							}
						} else {
							if((jvalue = json_find_member(joption, "value")) != NULL) {
								if(jvalue->tag == JSON_ARRAY) {
									jchild = json_first_child(jvalue);
									while(jchild) {
										if(opt->vartype != (JSON_NUMBER | JSON_STRING)) {
											if(jchild->tag != JSON_NUMBER && opt->vartype == JSON_NUMBER) {
												logprintf(LOG_ERR, "action \"%s\" option \"%s\" only accepts numbers", node->action->name, opt->name);
												error = 1;
												break;
											}
											if(jchild->tag != JSON_STRING && opt->vartype == JSON_STRING) {
												logprintf(LOG_ERR, "action \"%s\" option \"%s\" only accepts strings", node->action->name, opt->name);
												error = 1;
												break;
											}
										}
										jchild = jchild->next;
									}
								}
							}
						}
						opt = opt->next;
					}
					if(error == 0) {
						jchild = json_first_child(node->parsedargs);
						while(jchild) {
							jchild1 = json_first_child(node->parsedargs);
							match = 0;
							while(jchild1) {
								if(strcmp(jchild->key, jchild1->key) == 0) {
									match++;
								}
								jchild1 = jchild1->next;
							}
							if(match > 1) {
								logprintf(LOG_ERR, "action \"%s\" has duplicate \"%s\" arguments", node->action->name, jchild->key);
								error = 1;
								break;
							}
							match = 0;
							opt = node->action->options;
							while(opt) {
								if(strcmp(jchild->key, opt->name) == 0) {
									match = 1;
									break;
								}
								opt = opt->next;
							}
							if(match == 0) {
								logprintf(LOG_ERR, "action \"%s\" doesn't accept option \"%s\"", node->action->name, jchild->key);
								error = 1;
								break;
							}
							jchild = jchild->next;
						}
					}
				}
				json_free(output);
			}

			if(error != 0) {
				break;
			}
		} else {
			offset += strlen(&tmp[offset])+1;
		}
		if(error != 0) {
			break;
		} else {
			output = json_stringify(node->arguments, NULL);
			if(node->parsedargs != NULL) {
				json_delete(node->parsedargs);
				node->parsedargs = NULL;
			}
			node->parsedargs = json_decode(output);
			jchild = json_first_child(node->parsedargs);
			while(jchild) {
				if((jvalue = json_find_member(jchild, "value")) != NULL) {
					jchild1 = json_first_child(jvalue);
					while(jchild1) {
						if(jchild1->tag == JSON_STRING) {
							if((error = event_parse_action_arguments(jchild1->string_, obj, validate)) == 0) {
								if(isNumeric(jchild1->string_) == 0) {
									int dec = nrDecimals(jchild1->string_);
									int nr = atof(jchild1->string_);
									json_free(jchild1->string_);
									jchild1->tag = JSON_NUMBER;
									jchild1->number_ = nr;
									jchild1->decimals_ = dec;
								}
							} else {
								break;
							}
						}
						jchild1 = jchild1->next;
					}
				}
				jchild = jchild->next;
			}

			if(error == 0) {
				if(validate == 1) {
					if(node->action != NULL) {
						if(node->action->checkArguments != NULL) {
							error = node->action->checkArguments(node);
						}
					}
				} else {
					if(node->action != NULL) {
						if(node->action->run != NULL) {
							error = node->action->run(node);
						}
					}
				}
			}
			json_free(output);
		}
		if(error != 0) {
			break;
		}
	}
	if(value != NULL) {
		FREE(value);
	}
	if(func != NULL) {
		FREE(func);
	}

	return error;
}

int event_parse_condition(char **rule, struct rules_t *obj, int depth, unsigned short validate) {
	char *tmp = *rule;
	char *and = strstr(tmp, "AND");
	char *or = strstr(tmp, "OR");
	char *subrule = NULL;
	size_t pos = 0;
	int	type = 0, error = 0/*, i = 0, nrspaces = 0, hasquote = 0*/;

	if(or == NULL) {
		type = AND;
	} else if(and == NULL) {
		type = OR;
	} else if((and-tmp) < (or-tmp)) {
		type = AND;
	} else {
		type = OR;
	}
	if(type == AND) {
		pos = (size_t)(and-tmp);
	} else {
		pos = (size_t)(or-tmp);
	}

	if((subrule = MALLOC(pos+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	strncpy(subrule, tmp, pos);
	subrule[pos-1] = '\0';
	// for(i=0;i<pos;i++) {
		// if(subrule[i] == '"') {
			// hasquote ^= 1;
		// }
		// if(hasquote == 0 && (subrule[i] == ' ')) {
			// nrspaces++;
		// }
	// }
	/*
	 * Only "1", "0", "True", "False" or
	 * "1 == 1", "datetime.hour < 18.00" is valid here.
	 */
	// if(nrspaces != 2 && nrspaces != 0) {
		// logprintf(LOG_ERR, "rule #%d invalid: could not parse \"%s\"", obj->nr, subrule);
		// error = -1;
	// } else {
		if(event_parse_formula(&subrule, obj, depth, validate) == -1) {
			error = -1;
		} else {
			size_t len = strlen(tmp);
			if(type == AND) {
				pos += 4;
			} else {
				pos += 3;
			}
			memmove(tmp, &tmp[pos], len-pos);
			tmp[len-pos] = '\0';
			error = atoi(subrule);
		}
	// }
	FREE(subrule);
	return error;
}

int event_parse_rule(char *rule, struct rules_t *obj, int depth, unsigned short validate) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	char *tloc = 0, *condition = NULL, *action = NULL;
	unsigned int tpos = 0, rlen = strlen(rule), tlen = 0;
	int x = 0, nrhooks = 0, error = 0, i = 0;
	int type_or = 0, type_and = 0;

	/* Replace all dual+ spaces with a single space */
	rule = uniq_space(rule);
	while(x < rlen) {
		// if(strncmp(&rule[x], "  ", 2) == 0) {
			// logprintf(LOG_ERR, "rule #%d invalid: multiple spaces in a row", nr);
			// error = -1;
			// goto close;
		// }
		if(rule[x] == '(') {
			nrhooks++;
		}
		if(rule[x] == ')') {
			nrhooks--;
		}
		x++;
	}

	if(depth == 0) {
		if(strncmp(&rule[0], "IF ", 3) != 0) {
			logprintf(LOG_ERR, "rule #%d invalid: missing IF", obj->nr);
			error = -1;
			goto close;
		}

		if((tloc = strstr(rule, " THEN ")) == NULL) {
			logprintf(LOG_ERR, "rule #%d invalid: missing THEN", obj->nr);
			error = -1;
			goto close;
		}

		tpos = (size_t)(tloc-rule);

		if((action = MALLOC((rlen-tpos)+6+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}

		strncpy(action, &rule[tpos+6], rlen-tpos);
		action[rlen-tpos+6] = '\0';
		tlen = rlen-tpos+6;

		/* Extract the command part between the IF and THEN
		   ("IF " length = 3) */
		if((condition = MALLOC(rlen-tlen+3+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}

		strncpy(condition, &rule[3], rlen-tlen+3);
		condition[rlen-tlen+3] = '\0';
	}

	if(nrhooks > 0) {
		logprintf(LOG_ERR, "rule #%d invalid: missing one or more )", obj->nr);
		error = -1;
		goto close;
	} else if(nrhooks < 0) {
		logprintf(LOG_ERR, "rule #%d invalid: missing one or more (", obj->nr);
		error = -1;
		goto close;
	}

	if(depth > 0) {
		condition = rule;
	}

	int pass = 0, ltype = NONE, skip = 0;
	while(condition[i] != '\0') {
		if(condition[i] == '(') {
			/* Subcondition */
			if(i == 0 || condition[i-1] == '(' || condition[i-1] == ' ') {
				if(ltype == AND && skip == 1 && validate == 0) {
					error = event_remove_hooks(&condition, obj, depth+1);
				} else {
					error = event_parse_hooks(&condition, obj, depth+1, validate);
				}
			/* Function */
			} else {
				error = event_parse_function(&condition, obj, validate, RULE);
			}
		}
		if((type_and = strncmp(&condition[i], " AND ", 5)) == 0 || (type_or = strncmp(&condition[i], " OR ", 4)) == 0) {
			i--;
			ltype = (type_and == 0) ? AND : OR;
			if(skip == 0) {
				if(pilight.debuglevel == 1) {
					fprintf(stderr, "evaluate (%s) %s\n", (type_and == 0) ? "AND" : "OR", condition);
				}
				pass = event_parse_condition(&condition, obj, depth, validate);
				error = pass;
				i = -1;
			} else {
				if(pilight.debuglevel == 1) {
					fprintf(stderr, "skip (%s) %s\n", (type_and == 0) ? "AND" : "OR", condition);
				}
				size_t y = (type_and == 0) ? 6 : 5;
				size_t z = strlen(condition)-((size_t)i + (size_t)y);
				memmove(condition, &condition[(size_t)i + (size_t)y], (size_t)z);
				condition[z] = '\0';
				i = -1;
			}
			if(validate == 0) {
				if(pass == 1 && type_or == 0) {
					condition[0] = '1';
					condition[1] = '\0';
					break;
				} else if(pass == 0 && type_and == 0) {
					skip = 1;
				} else {
					skip = 0;
				}
			}
		}
		if(error == -1) {
			goto close;
		}
		i++;
	}
	if(error > 0) {
		error = 0;
	}

	if((ltype == AND && pass == 0) && validate == 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "skip (%s) %s\n", (ltype == AND) ? "AND" : "OR", condition);
		}
		condition[0] = '0';
		condition[1] = '\0';
	/* Skip this part when the condition only contains "1" or "0" */
	} else if(strlen(condition) > 1) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "evaluate (%s) %s\n", (ltype == AND) ? "AND" : "OR", condition);
		}
		if(event_parse_formula(&condition, obj, depth, validate) == -1) {
			error = -1;
			goto close;
		}
	} else {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "skip (%s) %s\n", (ltype == AND) ? "AND" : "OR", condition);
		}
	}
	obj->status = atoi(condition);
	if(validate == 1 && depth == 0 && event_parse_action(action, obj, validate) != 0) {
		logprintf(LOG_ERR, "rule #%d invalid: invalid action", obj->nr);
		error = -1;
		goto close;
	} else {
		if(obj->status > 0 && depth == 0) {
			if(validate == 0 && event_parse_action(action, obj, validate) != 0) {
				error = -1;
				goto close;
			}
			obj->status = 1;
		}
	}

close:
	if(error != 0 && depth == 0) {
		logprintf(LOG_INFO, "rule #%d was parsed until: ... %s THEN %s", obj->nr, condition, action);
	}
	if(depth == 0) {
		if(condition != NULL) {
			FREE(condition);
		}
	}
	if(action != NULL) {
		FREE(action);
	}

	return error;
}

void *events_loop(void *param) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(eventslock_init == 0) {
		pthread_mutexattr_init(&events_attr);
		pthread_mutexattr_settype(&events_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&events_lock, &events_attr);
		pthread_cond_init(&events_signal, NULL);
		eventslock_init = 1;
	}

	struct devices_t *dev = NULL;
	struct JsonNode *jdevices = NULL, *jchilds = NULL;
	struct rules_t *tmp_rules = NULL;
	char *str = NULL, *origin = NULL, *protocol = NULL;
	unsigned short match = 0;
	unsigned int i = 0;

	pthread_mutex_lock(&events_lock);
	while(loop) {
		if(eventsqueue_number > 0) {
			pthread_mutex_lock(&events_lock);

			logprintf(LOG_STACK, "%s::unlocked", __FUNCTION__);

			running = 1;

			jdevices = json_find_member(eventsqueue->jconfig, "devices");
			tmp_rules = rules_get();
			while(tmp_rules) {
				if(tmp_rules->active == 1) {
					if(eventsqueue->jconfig != NULL) {
						char *conf = json_stringify(eventsqueue->jconfig, NULL);
						tmp_rules->jtrigger = json_decode(conf);
						json_free(conf);
					}

					match = 0;
					if((str = MALLOC(strlen(tmp_rules->rule)+1)) == NULL) {
						fprintf(stderr, "out of memory\n");
						exit(EXIT_FAILURE);
					}
					strcpy(str, tmp_rules->rule);
					if(json_find_string(eventsqueue->jconfig, "origin", &origin) == 0 &&
					   json_find_string(eventsqueue->jconfig, "protocol", &protocol) == 0) {
						if(strcmp(origin, "sender") == 0 || strcmp(origin, "receiver") == 0) {
							for(i=0;i<tmp_rules->nrdevices;i++) {
								if(strcmp(tmp_rules->devices[i], protocol) == 0) {
									match = 1;
									break;
								}
							}
						}
					}
					/* Only run those events that affect the updates devices */
					if(jdevices != NULL && match == 0) {
						jchilds = json_first_child(jdevices);
						while(jchilds) {
							for(i=0;i<tmp_rules->nrdevices;i++) {
								if(jchilds->tag == JSON_STRING &&
								   strcmp(jchilds->string_, tmp_rules->devices[i]) == 0) {
									if(devices_get(jchilds->string_, &dev) == 0) {
										if(dev->lastrule == tmp_rules->nr &&
											 tmp_rules->nr == dev->prevrule &&
											 dev->lastrule == dev->prevrule) {
											logprintf(LOG_ERR, "skipped rule #%d because of an infinite loop triggered by device %s", tmp_rules->nr, jchilds->string_);
										} else {
											match = 1;
										}
									} else {
										match = 1;
									}
									break;
								}
							}
							jchilds = jchilds->next;
						}
					}
					if(match == 1 && tmp_rules->status == 0) {
						clock_gettime(CLOCK_MONOTONIC, &tmp_rules->timestamp.first);
						if(event_parse_rule(str, tmp_rules, 0, 0) == 0) {
							if(tmp_rules->status == 1) {
								logprintf(LOG_INFO, "executed rule: %s", tmp_rules->name);
							}
						}
						clock_gettime(CLOCK_MONOTONIC, &tmp_rules->timestamp.second);
						logprintf(LOG_DEBUG, "rule #%d %s was parsed in %.6f seconds", tmp_rules->nr, tmp_rules->name,
							((double)tmp_rules->timestamp.second.tv_sec + 1.0e-9*tmp_rules->timestamp.second.tv_nsec) -
							((double)tmp_rules->timestamp.first.tv_sec + 1.0e-9*tmp_rules->timestamp.first.tv_nsec));

						tmp_rules->status = 0;
					}
					FREE(str);
					if(tmp_rules->jtrigger != NULL) {
						json_delete(tmp_rules->jtrigger);
						tmp_rules->jtrigger = NULL;
					}
				}
				tmp_rules = tmp_rules->next;
			}
			struct eventsqueue_t *tmp = eventsqueue;
			json_delete(tmp->jconfig);
			eventsqueue = eventsqueue->next;
			FREE(tmp);
			eventsqueue_number--;
			pthread_mutex_unlock(&events_lock);
		} else {
			running = 0;
			pthread_cond_wait(&events_signal, &events_lock);
		}
	}
	return (void *)NULL;
}

int events_running(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	return (running == 1) ? 0 : -1;
}
static void events_queue(char *message) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(eventslock_init == 1) {
		pthread_mutex_lock(&events_lock);
	}
	if(eventsqueue_number < 1024) {
		struct eventsqueue_t *enode = MALLOC(sizeof(eventsqueue_t));
		if(enode == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		enode->jconfig = json_decode(message);

		if(eventsqueue_number == 0) {
			eventsqueue = enode;
			eventsqueue_head = enode;
		} else {
			eventsqueue_head->next = enode;
			eventsqueue_head = enode;
		}

		eventsqueue_number++;
	} else {
		logprintf(LOG_ERR, "event queue full");
	}
	if(eventslock_init == 1) {
		pthread_mutex_unlock(&events_lock);
		pthread_cond_signal(&events_signal);
	}
}

void *events_clientize(void *param) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct JsonNode *jclient = NULL;
	struct JsonNode *joptions = NULL;
	struct ssdp_list_t *ssdp_list = NULL;
	char *out = NULL;
	int standalone = 0;
	int client_loop = 0;
	settings_find_number("standalone", &standalone);

	while(loop) {

		if(client_loop == 1) {
			sleep(1);
		}
		client_loop = 1;

		ssdp_list = NULL;
		if(ssdp_seek(&ssdp_list) == -1 || standalone == 1) {
			logprintf(LOG_NOTICE, "no pilight ssdp connections found");
			char server[16] = "127.0.0.1";
			if((sockfd = socket_connect(server, (unsigned short)socket_get_port())) == -1) {
				logprintf(LOG_ERR, "could not connect to pilight-daemon");
				continue;
			}
		} else {
			if((sockfd = socket_connect(ssdp_list->ip, ssdp_list->port)) == -1) {
				logprintf(LOG_ERR, "could not connect to pilight-daemon");
				continue;
			}
		}

		if(ssdp_list != NULL) {
			ssdp_free(ssdp_list);
		}

		jclient = json_mkobject();
		joptions = json_mkobject();
		json_append_member(jclient, "action", json_mkstring("identify"));
		json_append_member(joptions, "config", json_mknumber(1, 0));
		json_append_member(joptions, "receiver", json_mknumber(1, 0));
		json_append_member(jclient, "options", joptions);
		json_append_member(jclient, "media", json_mkstring("all"));
		out = json_stringify(jclient, NULL);
		if(socket_write(sockfd, out) != (strlen(out)+strlen(EOSS))) {
			json_free(out);
			json_delete(jclient);
			continue;
		}
		json_free(out);
		json_delete(jclient);

		if(socket_read(sockfd, &recvBuff, 0) != 0
			 || strcmp(recvBuff, "{\"status\":\"success\"}") != 0) {
			continue;
		}

		while(client_loop) {
			if(sockfd <= 0) {
				break;
			}
			if(loop == 0) {
				client_loop = 0;
				break;
			}

			int z = socket_read(sockfd, &recvBuff, 1);
			if(z == -1) {
				sockfd = 0;
				break;
			} else if(z == 1) {
				continue;
			}

			char **array = NULL;
			unsigned int n = explode(recvBuff, "\n", &array), i = 0;
			for(i=0;i<n;i++) {
				events_queue(array[i]);
			}
			array_free(&array, n);
		}
	}

	if(recvBuff != NULL) {
		FREE(recvBuff);
		recvBuff = NULL;
	}
	if(sockfd > 0) {
		socket_close(sockfd);
	}
	return (void *)NULL;
}
