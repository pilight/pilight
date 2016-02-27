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

typedef struct event_action_thread_t event_action_thread_t;
typedef struct event_actions_t event_actions_t;
typedef struct rules_actions_t rules_actions_t;

#include "../core/json.h"
#include "../core/common.h"
#include "../storage/storage.h"

struct event_actions_t {
	char *name;
	int nrthreads;
	int (*run)(struct rules_actions_t *);
	int (*checkArguments)(struct rules_actions_t *);
	void *(*gc)(void *);
	struct options_t *options;

	struct event_actions_t *next;
};

struct event_action_thread_t {
	int running;
	void *userdata;

	struct rules_actions_t *obj;
	struct event_actions_t *action;
	struct device_t *device;
};

struct event_actions_t *event_actions;

void event_action_init(void);
void event_action_register(struct event_actions_t **, const char *);
int event_action_gc(void);
unsigned long event_action_set_execution_id(char *);
int event_action_get_execution_id(char *, unsigned long *);
void event_action_thread_init(struct device_t *);
void event_action_thread_start(struct device_t *, struct event_actions_t *, void *(*)(void *), struct rules_actions_t *);
void event_action_thread_stop(struct device_t *);
void event_action_thread_free(struct device_t *);
void event_action_stopped(struct event_action_thread_t *);
void event_action_started(struct event_action_thread_t *);

#endif
