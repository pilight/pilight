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
	#include <sys/time.h>
#endif

#include "../../core/options.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../function.h"
#include "random.h"

static int run(struct rules_t *obj, struct JsonNode *arguments, char **ret, enum origin_t origin) {
	struct timeval t1;
	char *p = *ret;
	int r = 0, min = 1, max = 10;

	struct JsonNode *childs = json_first_child(arguments);
	if(childs == NULL) {
		logprintf(LOG_ERR, "RANDOM requires a minimum and maximum value e.g. RANDOM(1, 10)");
		return -1;
	}

	if(childs->tag == JSON_STRING) {
		if(isNumeric(childs->string_) == 0) {
			min = atoi(childs->string_);
		} else {
			logprintf(LOG_ERR, "RANDOM requires both parameters to be numeric values e.g. RANDOM(1, 10)");
			return -1;
		}
	} else if(childs->tag == JSON_NUMBER) {
		min = childs->number_;
	} else {
		logprintf(LOG_ERR, "RANDOM requires both parameters to be numeric values e.g. RANDOM(1, 10)");
		return -1;
	}

	childs = childs->next;
	if(childs == NULL) {
		logprintf(LOG_ERR, "RANDOM requires a minimum and maximum value e.g. RANDOM(1, 10)");
		return -1;
	}

	if(childs->tag == JSON_STRING) {
		if(isNumeric(childs->string_) == 0) {
			max = atoi(childs->string_);
		} else {
			logprintf(LOG_ERR, "RANDOM requires both parameters to be numeric values e.g. RANDOM(1, 10)");
			return -1;
		}
	} else if(childs->tag == JSON_NUMBER) {
		max = childs->number_;
	} else {
		logprintf(LOG_ERR, "RANDOM requires both parameters to be numeric values e.g. RANDOM(1, 10)");
		return -1;
	}

	if(childs->next != NULL) {
		logprintf(LOG_ERR, "RANDOM only takes two arguments e.g. RANDOM(1, 10)");
		return -1;
	}

	gettimeofday(&t1, NULL);
	srand(t1.tv_usec * t1.tv_sec);

	r = rand() / (RAND_MAX + 1.0) * (max - min + 1) + min;

	sprintf(p, "%d", r);

	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void functionRandomInit(void) {
	event_function_register(&function_random, "RANDOM");

	function_random->run = &run;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "random";
	module->version = "2.1";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	functionRandomInit();
}
#endif
