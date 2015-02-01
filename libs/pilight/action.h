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

#ifndef _ACTION_H_
#define _ACTION_H_

#include "json.h"

typedef struct event_actions_t {
	char *name;
	int (*run)(struct JsonNode *arguments);
	int (*checkArguments)(struct JsonNode *arguments);
	struct options_t *options;

	struct event_actions_t *next;
} event_actions_t;

struct event_actions_t *event_actions;

void event_action_init(void);
void event_action_register(struct event_actions_t **act, const char *name);
int event_action_gc(void);

#endif
