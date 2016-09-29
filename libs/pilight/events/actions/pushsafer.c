/*
	Copyright (C) 2013 - 2016 CurlyMo & Pushsafer.com Kevin Siml

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

#include "../../core/threads.h"
#include "../action.h"
#include "../../core/options.h"
#include "../../config/devices.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/http.h"
#include "../../core/common.h"
#include "pushsafer.h"

static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jtitle = NULL;
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jicon = NULL;
	struct JsonNode *jsound = NULL;
	struct JsonNode *jvibration = NULL;
	struct JsonNode *jprivatekey = NULL;
	struct JsonNode *jvalues = NULL;
	struct JsonNode *jchild = NULL;
	int nrvalues = 0;

	jtitle = json_find_member(obj->arguments, "TITLE");
	jmessage = json_find_member(obj->arguments, "MESSAGE");
	jprivatekey = json_find_member(obj->arguments, "PRIVATEKEY");
	jdevice = json_find_member(obj->arguments, "DEVICE");
	jicon = json_find_member(obj->arguments, "ICON");
	jsound = json_find_member(obj->arguments, "SOUND");
	jvibration = json_find_member(obj->arguments, "VIBRATION");

	if(jmessage == NULL) {
		logprintf(LOG_ERR, "pushsafer action is missing a \"MESSAGE\"");
		return -1;
	}
	if(jprivatekey == NULL) {
		logprintf(LOG_ERR, "pushsafer action is missing a \"PRIVATEKEY\"");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jmessage, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "pushsafer action \"MESSAGE\" only takes one argument");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jprivatekey, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "pushsafer action \"PRIVATEKEY\" only takes one argument");
		return -1;
	}
	return 0;
}

static void *thread(void *param) {
	struct rules_actions_t *pth = (struct rules_actions_t *)param;
	struct JsonNode *arguments = pth->parsedargs;
	struct JsonNode *jtitle = NULL;
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jicon = NULL;
	struct JsonNode *jsound = NULL;
	struct JsonNode *jvibration = NULL;
	struct JsonNode *jprivatekey = NULL;
	struct JsonNode *jvalues1 = NULL;
	struct JsonNode *jvalues2 = NULL;
	struct JsonNode *jvalues3 = NULL;
	struct JsonNode *jvalues4 = NULL;
	struct JsonNode *jvalues5 = NULL;
	struct JsonNode *jvalues6 = NULL;
	struct JsonNode *jvalues7 = NULL;
	struct JsonNode *jval1 = NULL;
	struct JsonNode *jval2 = NULL;
	struct JsonNode *jval3 = NULL;
	struct JsonNode *jval4 = NULL;
	struct JsonNode *jval5 = NULL;
	struct JsonNode *jval6 = NULL;
	struct JsonNode *jval7 = NULL;

	action_pushsafer->nrthreads++;

	char url[1024], typebuf[70];
	char *data = NULL, *tp = typebuf;
	int ret = 0, size = 0;

	jtitle = json_find_member(arguments, "TITLE");
	jmessage = json_find_member(arguments, "MESSAGE");
	jprivatekey = json_find_member(arguments, "PRIVATEKEY");
	jdevice = json_find_member(arguments, "DEVICE");
	jicon = json_find_member(arguments, "ICON");
	jsound = json_find_member(arguments, "SOUND");
	jvibration = json_find_member(arguments, "VIBRATION");

	if(jmessage != NULL && jprivatekey != NULL) {
		jvalues1 = json_find_member(jtitle, "value");
		jvalues2 = json_find_member(jmessage, "value");
		jvalues3 = json_find_member(jprivatekey, "value");
		jvalues4 = json_find_member(jdevice, "value");
		jvalues5 = json_find_member(jicon, "value");
		jvalues6 = json_find_member(jsound, "value");
		jvalues7 = json_find_member(jvibration, "value");
		if(jvalues2 != NULL && jvalues3 != NULL) {
			jval1 = json_find_element(jvalues1, 0);
			jval2 = json_find_element(jvalues2, 0);
			jval3 = json_find_element(jvalues3, 0);
			jval4 = json_find_element(jvalues4, 0);
			jval5 = json_find_element(jvalues5, 0);
			jval6 = json_find_element(jvalues6, 0);
			jval7 = json_find_element(jvalues7, 0);
			if(jval2 != NULL && jval3 != NULL &&
			   jval2->tag == JSON_STRING && jval3->tag == JSON_STRING) {
				data = NULL;
				strcpy(url, "https://www.pushsafer.com/api");
				char *message = jval2->string_;
				char *privatekey = jval3->string_;
				char *device = jval4->string_;
				char *icon = jval5->string_;
				char *sound = jval6->string_;
				char *vibration = jval7->string_;
				char *title = jval1->string_;
				size_t l = strlen(message)+strlen(privatekey);
				l += strlen(device)+strlen(title);
				l += strlen(icon)+strlen(sound)+strlen(vibration);
				l += strlen("k=")+strlen("&d=");
				l += strlen("&m=")+strlen("&t=");
				l += strlen("&i=")+strlen("&s=")+strlen("&v=");
				char content[l+2];
				sprintf(content, "k=%s&d=%s&i=%s&s=%s&v=%s&t=%s&m=%s", privatekey, device, icon, sound, vibration, title, message);
				data = http_post_content(url, &tp, &ret, &size, "application/x-www-form-urlencoded", urlencode(content));
				if(ret == 200) {
					logprintf(LOG_DEBUG, "pushsafer action succeeded with message: %s", data);
				} else {
					logprintf(LOG_NOTICE, "pushsafer action failed (%d) with message: %s", ret, data);
				}
				FREE(message);
				FREE(privatekey);
				FREE(device);
				FREE(icon);
				FREE(sound);
				FREE(vibration);
				FREE(title);
				if(data != NULL) {
					FREE(data);
				}
			}
		}
	}

	action_pushsafer->nrthreads--;

	return (void *)NULL;
}

static int run(struct rules_actions_t *obj) {
	pthread_t pth;
	threads_create(&pth, NULL, thread, (void *)obj);
	pthread_detach(pth);
	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionPushsaferInit(void) {
	event_action_register(&action_pushsafer, "pushsafer");

	options_add(&action_pushsafer->options, 'a', "TITLE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushsafer->options, 'b', "MESSAGE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushsafer->options, 'c', "PRIVATEKEY", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushsafer->options, 'd', "DEVICE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushsafer->options, 'e', "ICON", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushsafer->options, 'f', "SOUND", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushsafer->options, 'g', "VIBRATION", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_pushsafer->run = &run;
	action_pushsafer->checkArguments = &checkArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "pushsafer";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	actionPushsaferInit();
}
#endif
