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

#include "../function.h"
#include "../../core/options.h"
#include "../../config/devices.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
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
		min = atoi(childs->string_);
	}
	childs = childs->next;
	if(childs == NULL) {
		logprintf(LOG_ERR, "RANDOM requires a minimum and maximum value e.g. RANDOM(1, 10)");
		return -1;
	}
	if(childs->tag == JSON_STRING) {
		max = atoi(childs->string_);
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
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "94";
}

void init(void) {
	functionRandomInit();
}
#endif
