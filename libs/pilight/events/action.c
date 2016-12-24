/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#ifndef _WIN32
	#include <dlfcn.h>
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
#endif

#include "../core/threadpool.h"
#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/options.h"
#include "../core/dso.h"
#include "../core/log.h"

#include "action.h"
#include "actions/action_header.h"

#ifdef _WIN32
	#include <windows.h>
#endif

typedef struct execution_t {
	char *name;
	unsigned long id;

	struct execution_t *next;
} execution_t;

static struct execution_t *executions = NULL;

#ifndef _WIN32
void event_action_remove(char *name) {

	struct event_actions_t *currP, *prevP;

	prevP = NULL;

	for(currP = event_actions; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->name, name) == 0) {
			if(prevP == NULL) {
				event_actions = currP->next;
			} else {
				prevP->next = currP->next;
			}

			logprintf(LOG_DEBUG, "removed event action %s", currP->name);
			FREE(currP->name);
			FREE(currP);

			break;
		}
	}
}
#endif

void event_action_init(void) {

	#include "actions/action_init.h"

#ifndef _WIN32
	void *handle = NULL;
	void (*init)(void);
	void (*compatibility)(struct module_t *module);
	char path[PATH_MAX];
	struct module_t module;
	char pilight_version[strlen(PILIGHT_VERSION)+1];
	char pilight_commit[3];
	char *action_root = NULL;
	int check1 = 0, check2 = 0, valid = 1, action_root_free = 0;
	strcpy(pilight_version, PILIGHT_VERSION);

	struct dirent *file = NULL;
	DIR *d = NULL;
	struct stat s;

	memset(pilight_commit, '\0', 3);

	if(settings_select_string(ORIGIN_MASTER, "actions-root", &action_root) != 0) {
		/* If no action root was set, use the default action root */
		if((action_root = MALLOC(strlen(ACTION_ROOT)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(action_root, ACTION_ROOT);
		action_root_free = 1;
	}
	size_t len = strlen(action_root);
	if(action_root[len-1] != '/') {
		strcat(action_root, "/");
	}

	if((d = opendir(action_root))) {
		while((file = readdir(d)) != NULL) {
			memset(path, '\0', PATH_MAX);
			sprintf(path, "%s%s", action_root, file->d_name);
			if(stat(path, &s) == 0) {
				/* Check if file */
				if(S_ISREG(s.st_mode)) {
					if(strstr(file->d_name, ".so") != NULL) {
						valid = 1;

						if((handle = dso_load(path)) != NULL) {
							init = dso_function(handle, "init");
							compatibility = dso_function(handle, "compatibility");
							if(init && compatibility) {
								compatibility(&module);
								if(module.name != NULL && module.version != NULL && module.reqversion != NULL) {
									char ver[strlen(module.reqversion)+1];
									strcpy(ver, module.reqversion);

									if((check1 = vercmp(ver, pilight_version)) > 0) {
										valid = 0;
									}

									if(check1 == 0 && module.reqcommit != NULL) {
										char com[strlen(module.reqcommit)+1];
										strcpy(com, module.reqcommit);
										sscanf(HASH, "v%*[0-9].%*[0-9]-%[0-9]-%*[0-9a-zA-Z\n\r]", pilight_commit);

										if(strlen(pilight_commit) > 0 && (check2 = vercmp(com, pilight_commit)) > 0) {
											valid = 0;
										}
									}
									if(valid == 1) {
										char tmp[strlen(module.name)+1];
										strcpy(tmp, module.name);
										event_action_remove(tmp);
										init();
										logprintf(LOG_DEBUG, "loaded event action %s v%s", file->d_name, module.version);
									} else {
										if(module.reqcommit != NULL) {
											logprintf(LOG_ERR, "event action %s requires at least pilight v%s (commit %s)", file->d_name, module.reqversion, module.reqcommit);
										} else {
											logprintf(LOG_ERR, "event action %s requires at least pilight v%s", file->d_name, module.reqversion);
										}
									}
								} else {
									logprintf(LOG_ERR, "invalid module %s", file->d_name);
								}
							}
						}
					}
				}
			}
		}
		closedir(d);
	}
	if(action_root_free) {
		FREE(action_root);
	}
#endif
}

void event_action_register(struct event_actions_t **act, const char *name) {

	if((*act = MALLOC(sizeof(struct event_actions_t))) == NULL) {
		OUT_OF_MEMORY
	}
	if(((*act)->name = MALLOC(strlen(name)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy((*act)->name, name);

	(*act)->options = NULL;
	(*act)->run = NULL;
	(*act)->gc = NULL;
	(*act)->nrthreads = 0;
	(*act)->checkArguments = NULL;

	(*act)->next = event_actions;
	event_actions = (*act);
}

int event_action_gc(void) {

	struct event_actions_t *tmp_action = NULL;
	while(event_actions) {
		tmp_action = event_actions;
		if(tmp_action->nrthreads > 0) {
			logprintf(LOG_DEBUG, "waiting for \"%s\" threads to finish", tmp_action->name);
		}
		FREE(tmp_action->name);
		options_delete(tmp_action->options);
		event_actions = event_actions->next;
		FREE(tmp_action);
	}
	if(event_actions != NULL) {
		FREE(event_actions);
	}

	logprintf(LOG_DEBUG, "garbage collected event action library");
	return 0;
}

unsigned long event_action_set_execution_id(char *name) {
	struct timeval tv;
#ifdef _WIN32
	SleepEx(1, TRUE);
#else
	usleep(1);
#endif
	gettimeofday(&tv, NULL);

	int match = 0;
	struct execution_t *tmp = executions;
	while(tmp) {
		if(strcmp(name, tmp->name) == 0) {
			tmp->id = (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;
			match = 1;
			break;
		}
		tmp = tmp->next;
	}
	if(match == 0) {
		if((tmp = MALLOC(sizeof(struct execution_t))) == NULL) {
			OUT_OF_MEMORY
		}
		if((tmp->name = MALLOC(strlen(name)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(tmp->name, name);
		tmp->id = (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

		tmp->next = executions;
		executions = tmp;
	}
	return tmp->id;
}

int event_action_get_execution_id(char *name, unsigned long *ret) {
	struct execution_t *tmp = executions;
	while(tmp) {
		if(strcmp(tmp->name, name) == 0) {
			*ret = tmp->id;
			return 0;
			break;
		}
		tmp = tmp->next;
	}
	return -1;
}

void event_action_thread_init(struct device_t *dev) {

	if((dev->action_thread = MALLOC(sizeof(struct event_action_thread_t))) == NULL) {
		OUT_OF_MEMORY
	}

	dev->action_thread->running = 0;
	dev->action_thread->obj = NULL;
	dev->action_thread->action = NULL;
}

void event_action_thread_start(struct device_t *dev, struct event_actions_t *action, void *(*func)(void *), struct rules_actions_t *obj) {
	struct event_action_thread_t *thread = dev->action_thread;

	if(thread->running == 1) {
		logprintf(LOG_DEBUG, "overriding previous \"%s\" action for device \"%s\"", thread->action->name, dev->id);
	}

	event_action_set_execution_id(dev->id);

	thread->obj = obj;
	thread->device = dev;
	thread->action = action;

	// threadpool_add_work(REASON_END, NULL, thread->action->name, 0, func, NULL, (void *)thread);
}

void event_action_thread_stop(struct device_t *dev) {

	struct event_action_thread_t *thread = NULL;

	if(dev != NULL) {
		thread = dev->action_thread;
		if(thread->running == 1) {
			logprintf(LOG_DEBUG, "aborting running \"%s\" action for device \"%s\"", thread->action->name, dev->id);
			thread->action->gc((void *)thread);
			event_action_set_execution_id(dev->id);
		}
	}
}

void event_action_thread_free(struct device_t *dev) {

	struct event_action_thread_t *thread = NULL;

	if(dev != NULL) {
		thread = dev->action_thread;
		if(thread != NULL) {
			if(thread->running == 1) {
				event_action_set_execution_id(dev->id);
				logprintf(LOG_DEBUG, "aborted running actions for device \"%s\"", dev->id);
				thread->action->gc((void *)thread);
			}

			FREE(dev->action_thread);
		}
	}
}

void event_action_started(struct event_action_thread_t *thread) {
	thread->running = 1;
	logprintf(LOG_INFO, "started \"%s\" action for device \"%s\"", thread->action->name, thread->device->id);
}

void event_action_stopped(struct event_action_thread_t *thread) {
	thread->running = 0;
	logprintf(LOG_INFO, "stopped \"%s\" action for device \"%s\"", thread->action->name, thread->device->id);
}
