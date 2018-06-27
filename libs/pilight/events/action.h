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

struct event_action_args_t {
	char *key;
	struct varcont_t **var;
	int nrvalues;
	struct event_action_args_t *next;
} event_action_args_t;

struct event_action_args_t *event_action_add_argument(struct event_action_args_t *, char *, struct varcont_t *);
int event_action_check_arguments(char *, struct event_action_args_t *);
int event_action_get_parameters(char *, int *, char ***);
int event_action_run(char *, struct event_action_args_t *);
void event_action_free_argument(struct event_action_args_t *);
int event_action_get_execution_id(char *name, unsigned long *ret);
unsigned long event_action_set_execution_id(char *name);
int event_action_gc(void);
void event_action_init(void);

#endif
