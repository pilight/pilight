/*
	Copyright (C) 2013 CurlyMo

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
