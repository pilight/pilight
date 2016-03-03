/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
