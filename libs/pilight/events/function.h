/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _FUNCTION_H_
#define _FUNCTION_H_

#include "../core/json.h"
#include "../core/common.h"

struct event_functions_t {
	char *name;
	int (*run)(struct rules_t *obj, struct JsonNode *arguments, char **out, enum origin_t origin);

	struct event_functions_t *next;
};

struct event_functions_t *event_functions;

void event_function_init(void);
void event_function_register(struct event_functions_t **act, const char *name);
int event_function_gc(void);

#endif
