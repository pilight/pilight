/*
	Copyright (C) 2013 - 2015 CurlyMo

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
#include <sys/time.h>
#ifndef __USE_XOPEN
	#define __USE_XOPEN
#endif
#include <time.h>

#ifdef _WIN32
#include "../../core/strptime.h"
#endif

#include "../function.h"
#include "../events.h"
#include "../../config/devices.h"
#include "../../core/options.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/datetime.h"
#include "received.h"

static int run(struct rules_t *obj, struct JsonNode *arguments, char **ret, enum origin_t origin) {
	struct JsonNode *jchilds = json_first_child(arguments);
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jnode = NULL;
	struct protocols_t *tmp_protocols = protocols;
	int error = 0, match1 = 0, match2 = 0;
	
	strcpy(*ret, "0");

	if(jchilds == NULL || jchilds->tag != JSON_STRING) {
		logprintf(LOG_ERR, "RECEIVED requires at least a valid protocol e.g. RECEIVED(generic_switch)");
		error = -1;
		goto close;
	}
	
	match1 = 0;
	while(tmp_protocols) {
		if(protocol_device_exists(tmp_protocols->listener, jchilds->string_) == 0) {
			match1 = 1;
			break;
		}
		tmp_protocols = tmp_protocols->next;
	}
	if(match1 == 0) {
		logprintf(LOG_ERR, "RECEIVED requires at least a valid protocol e.g. RECEIVED(generic_switch)");
		error = -1;
		goto close;
	}
	
	if(origin == RULE) {
		event_cache_device(obj, jchilds->string_);
	}	

	match1 = 0, match2 = 0;
	jchilds = jchilds->next;
	if(jchilds == NULL) {
		strcpy(*ret, "1");
		error = 0;
		goto close;
	}
	while(jchilds) {
		if(jchilds != NULL &&  jchilds->tag != JSON_STRING) {
			logprintf(LOG_ERR, "RECEIVED match arguments must be formatted like RECEIVED(generic_switch, id=12)");
			error = -1;
			goto close;
		}
		char **array = NULL;
		int n = explode(jchilds->string_, "=", &array);
		if(n != 2) {
			logprintf(LOG_ERR, "RECEIVED match arguments must be formatted like RECEIVED(generic_switch, id=12)");
			error = -1;
			array_free(&array, n);
			goto close;
		}
		if(obj->jtrigger != NULL && (jmessage = json_find_member(obj->jtrigger, "message")) != NULL) {
			if((jnode = json_find_member(jmessage, array[0])) != NULL) {
				if(isNumeric(array[1]) == 0 && jnode->tag == JSON_NUMBER) {
					match1++;
					if(atoi(array[1]) == (int)jnode->number_) {
						match2++;
					}
				} else if(isNumeric(array[1]) != 0 && jnode->tag == JSON_STRING) {
					match1++;
					if(strcmp(array[1], jnode->string_) == 0) {
						match2++;
					}
				}
			}
		}
		array_free(&array, n);
		jchilds = jchilds->next;
	}
	if(match1 == match2) {
		strcpy(*ret, "1");
		error = 0;
		goto close;
	}	
	if(!(obj->jtrigger != NULL && (jmessage = json_find_member(obj->jtrigger, "message")) != NULL)) {
		logprintf(LOG_ERR, "RECEIVED match arguments must be formatted like RECEIVED(generic_switch, id=12)");
		error = -1;
		goto close;
	}
		

close:
	return error;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void functionReceivedInit(void) {
	event_function_register(&function_received, "RECEIVED");

	function_received->run = &run;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "RECEIVED";
	module->version = "1.0";
	module->reqversion = "7.0";
	module->reqcommit = "21";
}

void init(void) {
	functionReceivedInit();
}
#endif
