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
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "../../pilight.h"
#include "common.h"
#include "settings.h"
#include "devices.h"
#include "config.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "json.h"
#include "ssdp.h"

#include "protocol.h"
#include "operator.h"
#include "action.h"
#include "rules.h"

static unsigned short loop = 1;

typedef union varcont_t {
	char *string_;
	double number_;
} varcont_t;

static pthread_mutex_t events_lock;
static pthread_cond_t events_signal;
static pthread_mutexattr_t events_attr;

static unsigned short update = 0;
static struct JsonNode *jconfig = NULL;

int event_parse_rule(char *rule, struct rules_t *obj, int depth, unsigned int nr, unsigned short validate);

int events_gc(void) {
	loop = 0;
	pthread_mutex_unlock(&events_lock);
	pthread_cond_signal(&events_signal);

	event_operator_gc();
	event_action_gc();
	logprintf(LOG_DEBUG, "garbage collected events library");
	return 1;
}

static int event_store_val_ptr(struct rules_t *obj, char *device, char *name, struct devices_settings_t *settings) {
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
		if(!(tmp_values = malloc(sizeof(rules_values_t)))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		tmp_values->next = NULL;
		if(!(tmp_values->name = malloc(strlen(name)+1))) {
			logprintf(LOG_ERR, "out of memory");
			sfree((void *)&tmp_values);
			exit(EXIT_FAILURE);
		}
		strcpy(tmp_values->name, name);
		if(!(tmp_values->device = malloc(strlen(device)+1))) {
			logprintf(LOG_ERR, "out of memory");
			sfree((void *)&tmp_values->name);
			sfree((void *)&tmp_values);
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
static int event_lookup_variable(char *var, struct rules_t *obj, unsigned int nr, int type, union varcont_t *varcont, unsigned short validate) {
	int cached = 0;
	if(strcmp(var, "1") == 0 || strcmp(var, "0") == 0 || isNumeric(var) == 0) {
		if(type == JSON_NUMBER) {
			varcont->number_ = atof(var);
		} else if(type == JSON_STRING) {
			varcont->string_ = var;
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
		char *device = strtok(var, ".");
		char *name = strtok(NULL, ".");

		/* Check if the values are not already cached */
		struct rules_values_t *tmp_values = obj->values;
		while(tmp_values) {
			if(strcmp(tmp_values->device, device) == 0 &&
				 strcmp(tmp_values->name, name) == 0) {
					if(tmp_values->settings->values->type == JSON_STRING) {
						varcont->string_ = tmp_values->settings->values->string_;
					} else if(tmp_values->settings->values->type == JSON_NUMBER) {
						varcont->number_ = tmp_values->settings->values->number_;
					}
					cached = 1;
					break;
			}
			tmp_values = tmp_values->next;
		}

		struct devices_t *dev = NULL;
		if(cached == 0) {
			if(devices_get(device, &dev) == 0) {
				if(validate == 1) {
					int exists = 0;
					for(i=0;i<obj->nrdevices;i++) {
						if(strcmp(obj->devices[i], device) == 0) {
							exists = 1;
							break;
						}
					}
					if(exists == 0) {
						/* Store all devices that are present in this rule */					
						if(!(obj->devices = realloc(obj->devices, sizeof(char *)*(unsigned int)(obj->nrdevices+1)))) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						if(!(obj->devices[obj->nrdevices] = malloc(strlen(device)+1))) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						strcpy(obj->devices[obj->nrdevices], device);
						obj->nrdevices++;
					}
					struct protocols_t *tmp = dev->protocols;
					unsigned int match1 = 0, match2 = 0, match3 = 0;
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
								if(opt->conftype == DEVICES_VALUE || opt->conftype == DEVICES_STATE) {
									match2 = 1;
									if(opt->vartype == JSON_STRING && type == JSON_STRING) {
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
					if(!match1) {
						logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" has no variable \"%s\"", nr, device, name);
					} else if(!match2) {
						logprintf(LOG_ERR, "rule #%d invalid: variable \"%s\" of device \"%s\" cannot be used in event rules", nr, device, name);
					} else if(!match3) {
						logprintf(LOG_ERR, "rule #%d invalid: trying to compare a integer variable \"%s.%s\" to a string", nr, device, name);
					}
					if(!match1 || !match2 || !match3) {
						varcont->string_ = NULL;
						varcont->number_ = 0;
						return -1;
					}
				}
				struct devices_settings_t *tmp_settings = dev->settings;
				while(tmp_settings) {
					if(strcmp(tmp_settings->name, name) == 0) {
						if(tmp_settings->values->type == JSON_STRING) {
							if(type == JSON_STRING) {
								/* Cache values for faster future lookup */
								event_store_val_ptr(obj, device, name, tmp_settings);
								varcont->string_ = tmp_settings->values->string_;
								return 0;
							} else {
								logprintf(LOG_ERR, "rule #%d invalid: trying to compare integer variable \"%s.%s\" to a string", nr, device, name);
								varcont->string_ = NULL;
								return -1;
							}
						} else if(tmp_settings->values->type == JSON_NUMBER) {
							if(type == JSON_NUMBER) {
								/* Cache values for faster future lookup */
								event_store_val_ptr(obj, device, name, tmp_settings);
								varcont->number_ = tmp_settings->values->number_;
								return 0;
							} else {
								logprintf(LOG_ERR, "rule #%d invalid: trying to compare string variable \"%s.%s\" to an integer", nr, device, name);
								varcont->number_ = 0;
								return -1;
							}
						}
					}
					tmp_settings = tmp_settings->next;
				}
				logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" has no variable \"%s\"", nr, device, name);
				varcont->string_ = NULL;
				varcont->number_ = 0;
				return -1;
			} else {
				logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" does not exist in the config", nr, device);
				varcont->string_ = NULL;
				varcont->number_ = 0;
				return -1;
			}
		}
		return 1;
	} else if(nrdots > 2) {
		logprintf(LOG_ERR, "rule #%d invalid: variable \"%s\" is invalid", nr, var);
		varcont->string_ = NULL;
		varcont->number_ = 0;
		return -1;
	} else {
		if(type == JSON_STRING) {
			varcont->string_ = var;
		} else if(type == JSON_NUMBER) {
			varcont->number_ = atof(var);
		}
		return 0;
	}
}

static int event_parse_hooks(char **rule, struct rules_t *obj, int depth, unsigned int nr, int (func)(char *rule, struct rules_t *obj, int depth, unsigned int nr, unsigned short validate), unsigned short validate) {
	int hooks = 0, res = 0;
	unsigned long buflen = MEMBUFFER, len = 0, i = 0;
	char *subrule = malloc(buflen);
	char *tmp = *rule;

	if(!subrule) {
		logprintf(LOG_ERR, "out of memory");
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
				subrule = realloc(subrule, buflen);
				memset(&subrule[len], '\0', buflen-(unsigned long)len);
			}
		} else {
			if(len > 0) {
				unsigned long z = 1;
				char *replace = malloc(len+3);

				subrule[len-1] = '\0';

				sprintf(replace, "(%s)", subrule);
				replace[len+2] = '\0';
				/* If successful, the variable subrule will be 
				   replaced with the outcome of the formula */
				if((res = func(subrule, obj, depth, nr, validate)) > -1) {
					z = strlen(subrule);
					/* Remove the hooks and the content within.
						 Replace this content with the result of the sum.

					   e.g.: ((1 + 2) + (3 + 4))
									 (   3    +    7   )
					*/
					// printf("Replace \"%s\" with \"%s\" in \"%s\"\n", replace, subrule, tmp);
					str_replace(replace, subrule, &tmp);

					sfree((void *)&replace);
				} else {
					sfree((void *)&subrule);
					sfree((void *)&replace);
					return -1;
				}
				memset(subrule, '\0', (unsigned long)len);
				i -= (len-z);
				len = 0;			
			}
		}
		i++;
	}
	sfree((void *)&subrule);
	return 0;
}

static int event_parse_formula(char **rule, struct rules_t *obj, int depth, unsigned int nr, unsigned short validate) {
	union varcont_t v1;
	union varcont_t v2;
	char *var1 = NULL, *func = NULL, *var2 = NULL, *tmp = *rule, *search = NULL;
	int element = 0, i = 0, match = 0, error = 0;
	unsigned long len = strlen(tmp), pos = 0, word = 0;
	char *res = malloc(255);
	memset(res, '\0', 255);

	/* If there are no hooks, we are going to solve the whole (sub)formula.
	   This formula can be as simple as:
	   e.g.: 1 + 2
	   or more complex as:
	   e.g.: 1 AND 1 AND 0 AND 1
			 1 AND location.device.state IS on
	*/
	while(pos <= len) {
		/* If we encounter a space we have the formula part */
		if(tmp[pos] == ' ' || pos == len) {
			switch(element) {
				/* The first value of three is always the first variable.
				   The second value is always the function.
				   The third value of the three is always the second variable (or second formula).
				*/
				case 0:
					var1 = realloc(var1, ((pos-word)+1));
					memset(var1, '\0', ((pos-word)+1));
					strncpy(var1, &tmp[word], (pos-word));
				break;
				case 1: 
					func = realloc(func, ((pos-word)+1));
					memset(func, '\0', ((pos-word)+1));
					strncpy(func, &tmp[word], (pos-word));
				break;
				case 2: 
					var2 = realloc(var2, ((pos-word)+1));
					memset(var2, '\0', ((pos-word)+1));
					strncpy(var2, &tmp[word], (pos-word));

					unsigned long l = (strlen(var1)+strlen(var2)+strlen(func))+2;
					search = realloc(search, (l+1));
					memset(search, '\0', l+1);
					l = (unsigned long)sprintf(search, "%s %s %s", var1, func, var2);
					search[l] = '\0';
				break;
				default:;
			}
			word = pos+1;
			element++;
			if(element > 2) {
				element = 0;
			}
		}

		if(func && strlen(func) > 0 && 
		   var1 && strlen(var1) > 0 && 
		   var2 && strlen(var2) > 0) {	 

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
					if(tmp_operator->callback_string) {
						ret1 = event_lookup_variable(var1, obj, nr, type, &v1, validate);
						ret2 = event_lookup_variable(var2, obj, nr, type, &v2, validate);

						if(ret1 == -1 || ret2 == -1) {
							error = -1;
							goto close;
						} else if(v1.string_ != NULL && v2.string_ != NULL) {
							/* Solve the formula */		
							tmp_operator->callback_string(v1.string_, v2.string_, &res);
						} else {
							error = 0;
							goto close;
						}
					} else if(tmp_operator->callback_number) {
						/* Replace the (more complex) subpart of the formula with the solutions:
						   e.g.: 0 AND location.device.state IS on
								 0 AND location.device.state IS on AND location.device.state IS off
						   to:
								 0 AND 1
								 0 AND 1 AND 0
						   This function will solve all AND's from left to right.
						   
						   The more simple AND and OR operators will be handled by the rest of this function.
						*/
						if((strcmp(func, "AND") == 0 || strcmp(func, "OR") == 0) && (strcmp(var2, "1") != 0 && strcmp(var2, "0") != 0)) {
							int flen = (int)strlen(func)+3, rlen = (int)strlen(tmp), nrspaces = 0;
							char *subrule = NULL;
							unsigned long buflen = MEMBUFFER, part2len = 0, sublen = 0;
							if(!(subrule = malloc(buflen))) {
								logprintf(LOG_ERR, "out of memory");
								exit(EXIT_FAILURE);
							}
							while(flen <= rlen) {
								if(tmp[flen] == ' ') {
									nrspaces++;
								}
								if(nrspaces == 3) {
									break;
								}
								if(nrspaces >= 1) {
									part2len++;
								}
								subrule[sublen++] = tmp[flen];
								if(buflen <= sublen) {
									buflen *= 2;
									subrule = realloc(subrule, buflen);
									memset(&subrule[sublen], '\0', buflen-sublen);
								}
								flen++;
							}

							if(subrule != NULL) {
								subrule[sublen] = '\0';
								search = realloc(search, strlen(subrule)+1);
								strcpy(search, subrule);
								if(event_parse_formula(&subrule, obj, depth, nr, validate) == 0) {
									char *res1 = malloc(255);
									memset(res, '\0', 255);
									if(tmp_operator->callback_number) {
										tmp_operator->callback_number(atof(var1), atof(subrule), &res1);
									} else {
										char r[strlen(res)+1];
										strcpy(r, res);
										tmp_operator->callback_string(var1, r, &res1);
									}
									unsigned long r = 0;
									char *t = malloc(strlen(res1)+1);
									strcpy(t, res1);
									// printf("Replace \"%s\" with \"%s\" in \"%s\"\n", search, t, tmp);
									if((r = (unsigned long)str_replace(search, t, &tmp)) == -1) {
										logprintf(LOG_ERR, "rule #%d: an unexpected error occured while parsing", nr);
										error = -1;
										sfree((void *)&res1);
										sfree((void *)&t);
										sfree((void *)&subrule);
										goto close;
									} else {
										len = r;
									}
									var2 = realloc(var2, strlen(res1)+1);
									strcpy(var2, res1);
									sfree((void *)&res1);
									sfree((void *)&t);
								} else {
									logprintf(LOG_ERR, "rule #%d: an unexpected error occured while parsing", nr);
									error = -1;
									sfree((void *)&subrule);
									goto close;
								}
								sfree((void *)&subrule);
								subrule = NULL;
							} else {
								logprintf(LOG_ERR, "rule #%d: an unexpected error occured while parsing", nr);
								error = -1;
								goto close;
							}
							/* Adapt the search string to be replaced */
							unsigned long l = strlen(var1)+strlen(func)+strlen(var2)+2;
							search = realloc(search, l+1);
							sprintf(search, "%s %s %s", var1, func, var2);
						}

						/* Continue with regular numeric operator parsing */
						ret1 = event_lookup_variable(var1, obj, nr, type, &v1, validate);
						ret2 = event_lookup_variable(var2, obj, nr, type, &v2, validate);
						if(ret1 == -1 || ret2 == -1) {
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
					if(res) {
						/* Replace the subpart of the formula with the solutions in case of simple AND's
						   e.g.: 0 AND 1 AND 1 AND 0
								     0 AND 1 AND 0
						*/
						char *p = malloc(strlen(res)+1);
						strcpy(p, res);
						unsigned long r = 0;
						// printf("Replace \"%s\" with \"%s\" in \"%s\"\n", search, p, tmp);
						if((r = (unsigned long)str_replace(search, p, &tmp)) == -1) {
							logprintf(LOG_ERR, "rule #%d: an unexpected error occured while parsing", nr);
							sfree((void *)&p);
							error = -1;
							goto close;
						}
						len = r;
						sfree((void *)&p);
					}
					pos = 0, word = 0, element = 0;
					break;				
				}
				i++;
				tmp_operator = tmp_operator->next;
			}
			if(match == 0) {
				logprintf(LOG_ERR, "rule #%d invalid: operator %s does not exist", nr, func);
				error = -1;
				goto close;
			}
			if(var1) {
				memset(var1, '\0', strlen(var1));
			} if(func) {
				memset(func, '\0', strlen(func));
			} if(var2) {
				memset(var2, '\0', strlen(var2));
			} if(search) {
				memset(search, '\0', strlen(search));
			}
		}
		pos++;
	}

close:
	sfree((void *)&res);
	if(var1) {
		sfree((void *)&var1); 
		var1 = NULL;
	} if(func) { 
		sfree((void *)&func); 
		func = NULL;
	} if(var2) { 
		sfree((void *)&var2); 
		var2 = NULL;
	} if(search) { 
		sfree((void *)&search);
		search = NULL;
	}

	return error;
}

static int event_parse_action(char *action, int validate) {
	char tmp[strlen(action)+1];
	char *pch = NULL;
	char *func = NULL;
	int error = 0, match = 0, i = 0;
	struct JsonNode *jargs = json_mkobject();
	strcpy(tmp, action);

	pch = strtok(tmp, " ");
	if(pch) {
		func = malloc(strlen(pch)+1);
		strcpy(func, pch);
	}

	pch = strtok(NULL, " ");
	while(pch) {
		char name[strlen(pch)+1];
		strcpy(name, pch);
		if((pch = strtok(NULL, " "))) {
			i++;
			char val[strlen(pch)+1];
			strcpy(val, pch);
			struct JsonNode *jobj = json_mkobject();
			if(isNumeric(val) == 0) {
				json_append_member(jobj, "value", json_mknumber(atof(val), nrDecimals(val)));
				json_append_member(jobj, "order", json_mknumber(i, 0));
			} else {
				json_append_member(jobj, "value", json_mkstring(val));
				json_append_member(jobj, "order", json_mknumber(i, 0));
			}
			json_append_member(jargs, name, jobj);
		}
		pch = strtok(NULL, " ");	
	}

	struct event_actions_t *actions = event_actions;
	while(actions) {
		if(strcmp(actions->name, func) == 0) {
			match = 1;
			break;
		}
		actions = actions->next;
	}

	if(match == 1) {
		struct options_t *opt = actions->options;
		struct JsonNode *joption = NULL;
		struct JsonNode *jvalue = NULL;
		struct JsonNode *jchild = NULL;
		while(opt) {
			if((joption = json_find_member(jargs, opt->name)) == NULL) {
				if(opt->conftype == DEVICES_VALUE) {
					logprintf(LOG_ERR, "action \"%s\" is missing option \"%s\"", actions->name, opt->name);
					error = 1;
					break;
				}
			} else {
				if((jvalue = json_find_member(joption, "value")) != NULL) {
					if(jvalue->tag != JSON_NUMBER && opt->vartype == JSON_NUMBER) {
						logprintf(LOG_ERR, "action \"%s\" option \"%s\" only accepts numbers", actions->name, opt->name);
						error = 1;
						break;
					}
					if(jvalue->tag != JSON_STRING && opt->vartype == JSON_STRING) {
						logprintf(LOG_ERR, "action \"%s\" option \"%s\" only accepts strings", actions->name, opt->name);
						error = 1;
						break;
					}
				}
			}
			opt = opt->next;
		}
		if(error == 0) {
			jchild = json_first_child(jargs);
			while(jchild) {
				match = 0;
				opt = actions->options;
				while(opt) {
					if(strcmp(jchild->key, opt->name) == 0) {
						match = 1;
						break;
					}
					opt = opt->next;
				}
				if(match == 0) {
					logprintf(LOG_ERR, "action \"%s\" doesn't accept option \"%s\"", actions->name, jchild->key);
					error = 1;
					break;					
				}
				jchild = jchild->next;
			}
		}
		if(error == 0) {
			if(validate) {
				if(actions->checkArguments) {
					error = actions->checkArguments(jargs);
				}
			} else {
				if(actions->run) {
					error = actions->run(jargs);
				}
			}
		}
	}
	json_delete(jargs);
	if(func) {
		sfree((void *)&func);
	}
	return error;
}

int event_parse_rule(char *rule, struct rules_t *obj, int depth, unsigned int nr, unsigned short validate) {
	char *tloc = 0, *condition = NULL, *action = NULL;
	unsigned int tpos = 0, rlen = strlen(rule), tlen = 0;
	int x = 0, nrhooks = 0, has_hooks = 0, error = 0;
	/* Check if rule has more than one spaces in a row */
	while(x < rlen) {
		if(strncmp(&rule[x], "  ", 2) == 0) {
			logprintf(LOG_ERR, "rule #%d invalid: multiple spaces in a row", nr);
			error = -1;
			goto close;
		}
		if(rule[x] == '(') {
			nrhooks++;
			has_hooks = 1;
		}
		if(rule[x] == ')') {
			nrhooks--;
		}
		x++;
	}

	if(depth == 0) {
		if(strncmp(&rule[0], "IF ", 3) != 0) {
			logprintf(LOG_ERR, "rule #%d invalid: missing IF", nr);
			error = -1;
			goto close;
		}

		if((tloc = strstr(rule, " THEN ")) == NULL) {
			logprintf(LOG_ERR, "rule #%d invalid: missing THEN", nr);
			error = -1;
			goto close;
		}

		tpos = (size_t)(tloc-rule);

		if(!(action = malloc((rlen-tpos)+6+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}

		strncpy(action, &rule[tpos+6], rlen-tpos);
		action[rlen-tpos+6] = '\0';
		tlen = rlen-tpos+6;

		if(validate == 1 && event_parse_action(action, validate) != 0) {
			logprintf(LOG_ERR, "rule #%d invalid: invalid action", nr);
			error = -1;
			goto close;
		}
		
		/* Extract the command part between the IF and THEN
		   ("IF " length = 3) */
		if(!(condition = malloc(rlen-tlen+3+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}

		strncpy(condition, &rule[3], rlen-tlen+3);
		condition[rlen-tlen+3] = '\0';	
	}

	if(nrhooks > 0) {
		logprintf(LOG_ERR, "rule #%d invalid: missing one or more )", nr);
		error = -1;
		goto close;
	} else if(nrhooks < 0) {
		logprintf(LOG_ERR, "rule #%d invalid: missing one or more (", nr);
		error = -1;
		goto close;
	}
	
	if(depth > 0) {
		condition = rule;
	}	
	if(has_hooks == 1) {
		int res = 0;
		if((res = event_parse_hooks(&condition, obj, depth+1, nr, event_parse_rule, validate)) == -1) {
			error = -1;
			goto close;
		}
	}

	if(event_parse_formula(&condition, obj, depth, nr, validate) == -1) {
		error = -1;
		goto close;
	}

	obj->status = atoi(condition);
	if(obj->status > 0 && depth == 0) {
		if(validate == 0 && event_parse_action(action, validate) != 0) {
			error = -1;
			goto close;
		}
			
		obj->status = 1;
	}
	
close:
	if(depth == 0) {
		if(condition) sfree((void *)&condition);
	}
	if(action) {
		sfree((void *)&action);
	}

	return error;
}

void *events_loop(void *param) {
	pthread_mutexattr_init(&events_attr);
	pthread_mutexattr_settype(&events_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&events_lock, &events_attr);
	pthread_cond_init(&events_signal, NULL);

	struct JsonNode *jdevices = NULL, *jchilds = NULL;
	struct rules_t *tmp_rules = NULL;
	char *str = NULL;
	unsigned short match = 0;
	unsigned int i = 0;

	pthread_mutex_lock(&events_lock);
	while(loop) {
		if(update > 0) {
			pthread_mutex_lock(&events_lock);

			jdevices = json_find_member(jconfig, "devices");
			tmp_rules = rules_get();
			while(tmp_rules) {
				if(tmp_rules->active == 1) {
					match = 0;
					str = strdup(tmp_rules->rule);
					/* Only run those events that affect the updates devices */
					if(jdevices != NULL) {
						jchilds = json_first_child(jdevices);
						while(jchilds) {
							for(i=0;i<tmp_rules->nrdevices;i++) {
								if(jchilds->tag == JSON_STRING && 
								   strcmp(jchilds->string_, tmp_rules->devices[i]) == 0) {
									match = 1;
									break;
								}
							}
							if(match == 1) {
								break;
							}
							jchilds = jchilds->next;
						}
					}
					
					if(match == 1) {
						tmp_rules->status = 0;
						if(event_parse_rule(str, tmp_rules, 0, 1, 0) == 0) {
							if(tmp_rules->status) {
								logprintf(LOG_DEBUG, "executed rule: %s", tmp_rules->name);	
							}
						}
					}
					sfree((void *)&str);
				}
				tmp_rules = tmp_rules->next;
			}
			json_delete(jconfig);
			update = 0;
			pthread_mutex_unlock(&events_lock);
		} else {
			pthread_cond_wait(&events_signal, &events_lock);
		}
	}
	return (void *)NULL;
}

void events_update(struct JsonNode *root) {
	pthread_mutex_lock(&events_lock);
	update = 1;
	char *content = json_stringify(root, NULL);
	jconfig = json_decode(content);
	sfree((void *)&content);
	pthread_mutex_unlock(&events_lock);
	pthread_cond_signal(&events_signal);
	return;
}