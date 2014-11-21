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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>
#include <time.h>
#include <libgen.h>

#include "../../pilight.h"
#include "common.h"
#include "json.h"
#include "rules.h"
#include "http_lib.h"
#include "log.h"
#include "events.h"
#include "operator.h"
#include "action.h"

static struct rules_t *rules = NULL;

/*
	FIXME: duplicate rule names
*/

static int rules_parse(JsonNode *root) {
	int have_error = 0, i = 0, match = 0;
	struct JsonNode *jrules = NULL;
	char *rule = NULL;
	double active = 1.0;

	if(root->tag == JSON_OBJECT) {
		jrules = json_first_child(root);
		while(jrules) {
			i++;
			if(jrules->tag == JSON_OBJECT) {
				if(json_find_string(jrules, "rule", &rule) != 0) {
					logprintf(LOG_ERR, "config rules #%d \"%s\", missing \"rule\"", i, jrules->key);
					have_error = 1;
					break;
				} else {
					active = 1.0;
					json_find_number(jrules, "active", &active);

					struct rules_t *tmp = rules;
					match = 0;
					while(tmp) {
						if(strcmp(tmp->name, jrules->key) == 0) {
							match = 1;
							break;
						}
						tmp = tmp->next;
					}
					if(match == 1) {
						logprintf(LOG_ERR, "config rules #%d \"%s\" already exists", i, jrules->key);
						have_error = 1;
						break;					
					}
					struct rules_t *node = malloc(sizeof(struct rules_t));
					if(!node) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					node->nrdevices = 0;
					node->status = 0;
					node->devices = NULL;
					if(event_parse_rule(rule, node, 0, 1, 1) == -1) {
						sfree((void *)&node);
						have_error = 1;
						break;
					}
					node->rule = malloc(strlen(rule)+1);
					if(!node->rule) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(node->rule, rule);
					node->name = malloc(strlen(jrules->key)+1);
					if(!node->name) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(node->name, jrules->key);
					node->active = (unsigned short)active;

					tmp = rules;
					if(tmp) {
						while(tmp->next != NULL) {
							tmp = tmp->next;
						}
						tmp->next = node;
					} else {
						node->next = rules;
						rules = node;
					}
				}
			}
			jrules = jrules->next;
		}
	} else {
			logprintf(LOG_ERR, "config rules should be placed in an object");
			have_error = 1;
	}	

	return have_error;
}

static JsonNode *rules_sync(int level) {
	struct JsonNode *root = json_mkobject();
	struct JsonNode *rule = NULL;
	struct rules_t *tmp = NULL;

	tmp = rules;
	
	while(tmp) {
		rule = json_mkobject();
		json_append_member(rule, "rule", json_mkstring(tmp->rule));
		json_append_member(rule, "active", json_mknumber((double)tmp->active, 0));
		json_append_member(root, tmp->name, rule);
		tmp = tmp->next;
	}
	return root;
}

struct rules_t *rules_get(void) {
	return rules;
}

static int rules_gc(void) {
	struct rules_t *tmp = NULL;
	int i = 0;

	while(rules) {
		tmp = rules;
		sfree((void *)&tmp->name);
		sfree((void *)&tmp->rule);
		for(i=0;i<tmp->nrdevices;i++) {
			sfree((void *)&tmp->devices[i]);
		}
		sfree((void *)&tmp->devices);
		rules = rules->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&rules);
	rules = NULL;
	logprintf(LOG_DEBUG, "garbage collected config rules library");
	return 1;
}

void rules_init(void) {
	event_operator_init();
	event_action_init();

	/* Request rules json object in main configuration */
	config_register(&config_rules, "rules");
	config_rules->readorder = 2;
	config_rules->writeorder = 1;
	config_rules->parse=&rules_parse;
	config_rules->sync=&rules_sync;
	config_rules->gc=&rules_gc;
}
