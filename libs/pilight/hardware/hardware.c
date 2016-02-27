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
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#ifndef _WIN32
	#include <regex.h>
	#include <sys/ioctl.h>
	#include <dlfcn.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pthread.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <libgen.h>
#include <dirent.h>

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/dso.h"
#include "hardware.h"

#ifndef _WIN32
	#include "../../wiringx/wiringX.h"
#endif

struct hardware_t *hardware = NULL;

#include "hardware_header.h"

#ifndef _WIN32
static void hardware_remove(char *name) {
	struct hardware_t *currP, *prevP;

	prevP = NULL;

	for(currP = hardware; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->id, name) == 0) {
			if(prevP == NULL) {
				hardware = currP->next;
			} else {
				prevP->next = currP->next;
			}

			logprintf(LOG_DEBUG, "removed config hardware module %s", currP->id);
			FREE(currP->id);
			options_delete(currP->options);
			FREE(currP);

			break;
		}
	}
}
#endif

void hardware_register(struct hardware_t **hw) {
	if((*hw = MALLOC(sizeof(struct hardware_t))) == NULL) {
		OUT_OF_MEMORY
	}
	(*hw)->options = NULL;
	// (*hw)->wait = 0;
	// (*hw)->stop = 0;
	// (*hw)->running = 0;
	(*hw)->minrawlen = 0;
	(*hw)->maxrawlen = 0;
	(*hw)->mingaplen = 0;
	(*hw)->maxgaplen = 0;

	(*hw)->init = NULL;
	(*hw)->deinit = NULL;
	(*hw)->receiveOOK = NULL;
	(*hw)->receivePulseTrain = NULL;
	(*hw)->receiveAPI = NULL;
	(*hw)->sendOOK = NULL;
	(*hw)->sendAPI = NULL;
	(*hw)->gc = NULL;
	(*hw)->settings = NULL;

	// pthread_mutexattr_init(&(*hw)->attr);
	// pthread_mutexattr_settype(&(*hw)->attr, PTHREAD_MUTEX_RECURSIVE);
	// pthread_mutex_init(&(*hw)->lock, &(*hw)->attr);
	// pthread_cond_init(&(*hw)->signal, NULL);

	(*hw)->next = hardware;
	hardware = (*hw);
}

void hardware_set_id(hardware_t *hw, const char *id) {
	if((hw->id = MALLOC(strlen(id)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(hw->id, id);
}

int hardware_gc(void) {
	struct hardware_t *htmp = hardware;

	while(hardware) {
		htmp = hardware;
		// htmp->stop = 1;
		// pthread_mutex_unlock(&htmp->lock);
		// pthread_cond_signal(&htmp->signal);
		// while(htmp->running == 1) {
			// usleep(10);
		// }
		// thread_stop(htmp->id);
		if(htmp->deinit != NULL) {
			htmp->deinit();
		}
		if(htmp->gc != NULL) {
			htmp->gc();
		}
		FREE(htmp->id);
		options_delete(htmp->options);
		hardware = hardware->next;
		FREE(htmp);
	}
	if(hardware != NULL) {
		FREE(hardware);
	}

	logprintf(LOG_DEBUG, "garbage collected config hardware library");
	return EXIT_SUCCESS;
}

void hardware_init(void) {
	#include "hardware_init.h"

#ifndef _WIN32
	void *handle = NULL;
	void (*init)(void);
	void (*compatibility)(struct module_t *module);
	char path[PATH_MAX];
	struct module_t module;
	char pilight_version[strlen(PILIGHT_VERSION)+1];
	char pilight_commit[3];
	char *hardware_root = NULL;
	int check1 = 0, check2 = 0, valid = 1, hardware_root_free = 0;
	strcpy(pilight_version, PILIGHT_VERSION);

	struct dirent *file = NULL;
	DIR *d = NULL;
	struct stat s;

	memset(pilight_commit, '\0', 3);

	if(settings_select_string(ORIGIN_MASTER, "hardware-root", &hardware_root) != 0) {
		/* If no hardware root was set, use the default hardware root */
		if((hardware_root = MALLOC(strlen(HARDWARE_ROOT)+2)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(hardware_root, HARDWARE_ROOT);
		hardware_root_free = 1;
	}
	size_t len = strlen(hardware_root);

	if(hardware_root[len-1] != '/') {
		strcat(hardware_root, "/");
	}

	if((d = opendir(hardware_root))) {
		while((file = readdir(d)) != NULL) {
			memset(path, '\0', PATH_MAX);
			sprintf(path, "%s%s", hardware_root, file->d_name);
			if(stat(path, &s) == 0) {
				/* Check if file */
				if(S_ISREG(s.st_mode)) {
					if(strstr(file->d_name, ".so") != NULL) {
						valid = 1;

						if((handle = dso_load(path)) != NULL) {
							init = dso_function(handle, "init");
							compatibility = dso_function(handle, "compatibility");
							if(init != NULL && compatibility != NULL ) {
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
										hardware_remove(tmp);
										init();
										logprintf(LOG_DEBUG, "loaded config hardware module %s v%s", file->d_name, module.version);
									} else {
										if(module.reqcommit != NULL) {
											logprintf(LOG_ERR, "config hardware module %s requires at least pilight v%s (commit %s)", file->d_name, module.reqversion, module.reqcommit);
										} else {
											logprintf(LOG_ERR, "config hardware module %s requires at least pilight v%s", file->d_name, module.reqversion);
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
	if(hardware_root_free) {
		FREE(hardware_root);
	}
#endif
}
