/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../../core/options.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/http.h"
#include "../action.h"
#include "pushbullet.h"

static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jtitle = NULL;
	struct JsonNode *jbody = NULL;
	struct JsonNode *jtype = NULL;
	struct JsonNode *jtoken = NULL;
	struct JsonNode *jvalues = NULL;
	struct JsonNode *jchild = NULL;
	int nrvalues = 0;

	jtitle = json_find_member(obj->arguments, "TITLE");
	jbody = json_find_member(obj->arguments, "BODY");
	jtoken = json_find_member(obj->arguments, "TOKEN");
	jtype = json_find_member(obj->arguments, "TYPE");

	if(jtitle == NULL) {
		logprintf(LOG_ERR, "pushbullet action is missing a \"TITLE\"");
		return -1;
	}
	if(jbody == NULL) {
		logprintf(LOG_ERR, "pushbullet action is missing a \"BODY\"");
		return -1;
	}
	if(jtype == NULL) {
		logprintf(LOG_ERR, "pushbullet action is missing a \"TYPE\"");
		return -1;
	}
	if(jtoken == NULL) {
		logprintf(LOG_ERR, "pushbullet action is missing a \"TOKEN\"");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jtitle, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "pushbullet action \"TITLE\" only takes one argument");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jbody, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "pushbullet action \"BODY\" only takes one argument");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jtoken, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "pushbullet action \"TOKEN\" only takes one argument");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jtype, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "pushbullet action \"TYPE\" only takes one argument");
		return -1;
	}
	return 0;
}

// static void callback(int ret, char *data, int len, char *mime, void *userdata) {
	// if(ret == 200) {
		// logprintf(LOG_DEBUG, "pushbullet action succeeded with message: %s", data);
	// } else {
		// logprintf(LOG_NOTICE, "pushbullet action failed (%d) with message: %s", ret, data);
	// }
// }

// static void *thread(void *param) {
	// struct threadpool_tasks_t *task = param;
	// struct rules_actions_t *pth = task->userdata;
	// struct JsonNode *json = pth->arguments;

	// struct JsonNode *jtitle = NULL;
	// struct JsonNode *jbody = NULL;
	// struct JsonNode *jtype = NULL;
	// struct JsonNode *jtoken = NULL;
	// struct JsonNode *jvalues1 = NULL;
	// struct JsonNode *jvalues2 = NULL;
	// struct JsonNode *jvalues3 = NULL;
	// struct JsonNode *jvalues4 = NULL;
	// struct JsonNode *jval1 = NULL;
	// struct JsonNode *jval2 = NULL;
	// struct JsonNode *jval3 = NULL;
	// struct JsonNode *jval4 = NULL;

	// char url[1024];

	// jtitle = json_find_member(json, "TITLE");
	// jbody = json_find_member(json, "BODY");
	// jtoken = json_find_member(json, "TOKEN");
	// jtype = json_find_member(json, "TYPE");

	// if(jtitle != NULL && jbody != NULL && jtoken != NULL && jtype != NULL) {
		// jvalues1 = json_find_member(jtitle, "value");
		// jvalues2 = json_find_member(jbody, "value");
		// jvalues3 = json_find_member(jtoken, "value");
		// jvalues4 = json_find_member(jtype, "value");
		// if(jvalues1 != NULL && jvalues2 != NULL && jvalues3 != NULL && jvalues4 != NULL) {
			// jval1 = json_find_element(jvalues1, 0);
			// jval2 = json_find_element(jvalues2, 0);
			// jval3 = json_find_element(jvalues3, 0);
			// jval4 = json_find_element(jvalues4, 0);
			// if(jval1 != NULL && jval2 != NULL && jval3 != NULL && jval4 != NULL &&
			 // jval1->tag == JSON_STRING && jval2->tag == JSON_STRING &&
			 // jval3->tag == JSON_STRING && jval4->tag == JSON_STRING) {
				// snprintf(url, 1024, "https://%s@api.pushbullet.com/v2/pushes", jval3->string_);

				// struct JsonNode *code = json_mkobject();
				// json_append_member(code, "type", json_mkstring(jval4->string_));
				// json_append_member(code, "title", json_mkstring(jval1->string_));
				// json_append_member(code, "body", json_mkstring(jval2->string_));

				// char *content = json_stringify(code, "\t");
				// json_delete(code);

				// http_post_content(url, "application/json", content, callback, NULL);

				// json_free(content);
			// }
		// }
	// }

	// return (void *)NULL;
// }

static int run(struct rules_actions_t *obj) {
	// threadpool_add_work(REASON_END, NULL, action_pushbullet->name, 0, thread, NULL, (void *)obj);

	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionPushbulletInit(void) {
	event_action_register(&action_pushbullet, "pushbullet");

	options_add(&action_pushbullet->options, 'a', "TITLE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushbullet->options, 'b', "BODY", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushbullet->options, 'c', "TOKEN", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushbullet->options, 'd', "TYPE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_pushbullet->run = &run;
	action_pushbullet->checkArguments = &checkArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "pushbullet";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionPushbulletInit();
}
#endif
