/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <libgen.h>
#include <dirent.h>
#include <dlfcn.h>

#include "../../pilight.h"
#include "common.h"
#include "settings.h"
#include "options.h"
#include "dso.h"
#include "log.h"

#include "switch.h"
#include "toggle.h"

#include "action.h"
#include "action_header.h"

void event_action_remove(char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

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
			sfree((void *)&currP->name);
			sfree((void *)&currP);

			break;
		}
	}
}

void event_action_init(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	#include "action_init.h"
	void *handle = NULL;
	void (*init)(void);
	void (*compatibility)(struct module_t *module);
	char path[PATH_MAX];
	struct module_t module;
	char pilight_version[strlen(VERSION)+1];
	char pilight_commit[3];
	char *action_root = NULL;
	int check1 = 0, check2 = 0, valid = 1, action_root_free = 0;
	strcpy(pilight_version, VERSION);

	struct dirent *file = NULL;
	DIR *d = NULL;
	struct stat s;

	memset(pilight_commit, '\0', 3);

	if(settings_find_string("action-root", &action_root) != 0) {
		/* If no action root was set, use the default action root */
		if(!(action_root = malloc(strlen(ACTION_ROOT)+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
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
			stat(file->d_name, &s);
			/* Check if file */
			if(S_ISREG(s.st_mode) == 0) {
				if(strstr(file->d_name, ".so") != NULL) {
					valid = 1;
					memset(path, '\0', PATH_MAX);
					sprintf(path, "%s%s", action_root, file->d_name);

					if((handle = dso_load(path))) {
						init = dso_function(handle, "init");
						compatibility = dso_function(handle, "compatibility");
						if(init && compatibility) {
							compatibility(&module);
							if(module.name && module.version && module.reqversion) {
								char ver[strlen(module.reqversion)+1];
								strcpy(ver, module.reqversion);

								if((check1 = vercmp(ver, pilight_version)) > 0) {
									valid = 0;
								}

								if(check1 == 0 && module.reqcommit) {
									char com[strlen(module.reqcommit)+1];
									strcpy(com, module.reqcommit);
									sscanf(HASH, "v%*[0-9].%*[0-9]-%[0-9]-%*[0-9a-zA-Z\n\r]", pilight_commit);

									if(strlen(pilight_commit) > 0 && (check2 = vercmp(com, pilight_commit)) > 0) {
										valid = 0;
									}
								}
								if(valid) {
									char tmp[strlen(module.name)+1];
									strcpy(tmp, module.name);
									event_action_remove(tmp);
									init();
									logprintf(LOG_DEBUG, "loaded event action %s", file->d_name);
								} else {
									if(module.reqcommit) {
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
		closedir(d);
	}
	if(action_root_free) {
		sfree((void *)&action_root);
	}
}

void event_action_register(struct event_actions_t **act, const char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(!(*act = malloc(sizeof(struct event_actions_t)))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if(!((*act)->name = malloc(strlen(name)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy((*act)->name, name);

	(*act)->options = NULL;
	(*act)->run = NULL;
	(*act)->checkArguments = NULL;

	(*act)->next = event_actions;
	event_actions = (*act);
}

int event_action_gc(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct event_actions_t *tmp_action = NULL;
	while(event_actions) {
		tmp_action = event_actions;
		sfree((void *)&tmp_action->name);
		options_delete(tmp_action->options);
		event_actions = event_actions->next;
		sfree((void *)&tmp_action);
	}
	sfree((void *)&event_actions);
	logprintf(LOG_DEBUG, "garbage collected event action library");
	return 0;
}
