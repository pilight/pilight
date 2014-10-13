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
#include <regex.h>
#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/time.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "dso.h"
#include "options.h"
#include "settings.h"
#include "protocol.h"
#include "log.h"

#include "protocol_header.h"

void protocol_remove(char *name) {
	struct protocols_t *currP, *prevP;

	prevP = NULL;

	for(currP = protocols; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->listener->id, name) == 0) {
			if(prevP == NULL) {
				protocols = currP->next;
			} else {
				prevP->next = currP->next;
			}

			struct protocol_devices_t *dtmp;
			struct protocol_plslen_t *ttmp;
			logprintf(LOG_DEBUG, "removed protocol %s", currP->listener->id);
			if(currP->listener->threadGC) {
				currP->listener->threadGC();
				logprintf(LOG_DEBUG, "stopped protocol threads");
			}
			if(currP->listener->gc) {
				currP->listener->gc();
				logprintf(LOG_DEBUG, "ran garbage collector");
			}
			sfree((void *)&currP->listener->id);
			sfree((void *)&currP->name);
			options_delete(currP->listener->options);
			if(currP->listener->plslen) {
				while(currP->listener->plslen) {
					ttmp = currP->listener->plslen;
					currP->listener->plslen = currP->listener->plslen->next;
					sfree((void *)&ttmp);
				}
			}
			sfree((void *)&currP->listener->plslen);
			if(currP->listener->devices) {
				while(currP->listener->devices) {
					dtmp = currP->listener->devices;
					sfree((void *)&dtmp->id);
					sfree((void *)&dtmp->desc);
					currP->listener->devices = currP->listener->devices->next;
					sfree((void *)&dtmp);
				}
			}
			sfree((void *)&currP->listener->devices);
			sfree((void *)&currP->listener);
			sfree((void *)&currP);

			break;
		}
	}
}

void protocol_init(void) {
	#include "protocol_init.h"
	void *handle = NULL;
	void (*init)(void);
	void (*compatibility)(struct module_t *module);
	char path[255];
	struct module_t module;
	char pilight_version[strlen(VERSION)+1];
	char pilight_commit[3];
	char *protocol_root = NULL;
	int check1 = 0, check2 = 0, valid = 1, protocol_root_free = 0;
	strcpy(pilight_version, VERSION);

	struct dirent *file = NULL;
	DIR *d = NULL;

	memset(pilight_commit, '\0', 3);

	if(settings_find_string("protocol-root", &protocol_root) != 0) {
		/* If no webserver port was set, use the default webserver port */
		if(!(protocol_root = malloc(strlen(PROTOCOL_ROOT)+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(protocol_root, PROTOCOL_ROOT);
		protocol_root_free = 1;
	}
	size_t len = strlen(protocol_root);
	if(protocol_root[len-1] != '/') {
		strcat(protocol_root, "/");
	}

	if((d = opendir(protocol_root))) {
		while((file = readdir(d)) != NULL) {
			if(file->d_type == DT_REG) {
				if(strstr(file->d_name, ".so") != NULL) {
					valid = 1;
					memset(path, '\0', 255);
					sprintf(path, "%s%s", protocol_root, file->d_name);

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
									protocol_remove(tmp);
									init();
									logprintf(LOG_DEBUG, "loaded protocol %s", file->d_name);
								} else {
									if(module.reqcommit) {
										logprintf(LOG_ERR, "protocol %s requires at least pilight v%s (commit %s)", file->d_name, module.reqversion, module.reqcommit);
									} else {
										logprintf(LOG_ERR, "protocol %s requires at least pilight v%s", file->d_name, module.reqversion);
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
	if(protocol_root_free) {
		sfree((void *)&protocol_root);
	}
}

void protocol_register(protocol_t **proto) {
	if(!(*proto = malloc(sizeof(struct protocol_t)))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	(*proto)->options = NULL;
	(*proto)->devices = NULL;
	(*proto)->plslen = NULL;

	(*proto)->pulse = 0;
	(*proto)->rawlen = 0;
	(*proto)->minrawlen = 0;
	(*proto)->maxrawlen = 0;
	(*proto)->binlen = 0;
	(*proto)->lsb = 0;
	(*proto)->txrpt = 1;
	(*proto)->hwtype = 1;
	(*proto)->rxrpt = 1;
	(*proto)->multipleId = 1;
	(*proto)->config = 1;
	(*proto)->parseRaw = NULL;
	(*proto)->parseBinary = NULL;
	(*proto)->parseCode = NULL;
	(*proto)->createCode = NULL;
	(*proto)->checkValues = NULL;
	(*proto)->initDev = NULL;
	(*proto)->printHelp = NULL;
	(*proto)->threadGC = NULL;
	(*proto)->gc = NULL;
	(*proto)->message = NULL;
	(*proto)->threads = NULL;

	(*proto)->repeats = 0;
	(*proto)->first = 0;
	(*proto)->second = 0;

	memset(&(*proto)->raw[0], 0, sizeof((*proto)->raw));
	memset(&(*proto)->code[0], 0, sizeof((*proto)->code));
	memset(&(*proto)->pCode[0], 0, sizeof((*proto)->pCode));
	memset(&(*proto)->binary[0], 0, sizeof((*proto)->binary));

	struct protocols_t *pnode = malloc(sizeof(struct protocols_t));
	if(!pnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	pnode->listener = *proto;
	if(!(pnode->name = malloc(4))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	pnode->next = protocols;
	protocols = pnode;
}

struct protocol_threads_t *protocol_thread_init(protocol_t *proto, struct JsonNode *param) {
	struct protocol_threads_t *node = malloc(sizeof(struct protocol_threads_t));
	node->param = param;
	pthread_mutexattr_init(&node->attr);
	pthread_mutexattr_settype(&node->attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&node->mutex, &node->attr);
    pthread_cond_init(&node->cond, NULL);
	node->next = proto->threads;
	proto->threads = node;
	return node;
}

int protocol_thread_wait(struct protocol_threads_t *node, int interval, int *nrloops) {
	struct timeval tp;
	struct timespec ts;

	pthread_mutex_unlock(&node->mutex);

	gettimeofday(&tp, NULL);
	ts.tv_sec = tp.tv_sec;
	ts.tv_nsec = tp.tv_usec * 1000;

	if(*nrloops == 0) {
		ts.tv_sec += 1;
		*nrloops = 1;
	} else {
		ts.tv_sec += interval;
	}

	pthread_mutex_lock(&node->mutex);

	return pthread_cond_timedwait(&node->cond, &node->mutex, &ts);
}

void protocol_thread_stop(protocol_t *proto) {
	if(proto->threads) {
		struct protocol_threads_t *tmp = proto->threads;
		while(tmp) {
			pthread_mutex_unlock(&tmp->mutex);
			pthread_cond_broadcast(&tmp->cond);
			tmp = tmp->next;
		}
	}
}

void protocol_thread_free(protocol_t *proto) {
	if(proto->threads) {
		struct protocol_threads_t *tmp = proto->threads;
		while(tmp) {
			tmp = proto->threads;
			if(tmp->param) {
				json_delete(tmp->param);
			}
			tmp = tmp->next;
			sfree((void *)&tmp);
		}
		sfree((void *)&proto->threads);
	}
}

void protocol_set_id(protocol_t *proto, const char *id) {
	if(!(proto->id = malloc(strlen(id)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(proto->id, id);
}

void protocol_plslen_add(protocol_t *proto, int plslen) {
	struct protocol_plslen_t *pnode = malloc(sizeof(struct protocol_plslen_t));
	if(!pnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	pnode->length = plslen;
	pnode->next	= proto->plslen;
	proto->plslen = pnode;
}

void protocol_device_add(protocol_t *proto, const char *id, const char *desc) {
	struct protocol_devices_t *dnode = malloc(sizeof(struct protocol_devices_t));
	if(!dnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if(!(dnode->id = malloc(strlen(id)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(dnode->id, id);
	if(!(dnode->desc = malloc(strlen(desc)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(dnode->desc, desc);
	dnode->next	= proto->devices;
	proto->devices = dnode;
}

int protocol_device_exists(protocol_t *proto, const char *id) {
	struct protocol_devices_t *temp = proto->devices;

	while(temp) {
		if(strcmp(temp->id, id) == 0) {
			return 0;
		}
		temp = temp->next;
	}
	sfree((void *)&temp);
	return 1;
}

int protocol_gc(void) {
	struct protocols_t *ptmp;
	struct protocol_devices_t *dtmp;
	struct protocol_plslen_t *ttmp;

	while(protocols) {
		ptmp = protocols;
		logprintf(LOG_DEBUG, "protocol %s", ptmp->listener->id);
		if(ptmp->listener->threadGC) {
			ptmp->listener->threadGC();
			logprintf(LOG_DEBUG, "stopped protocol threads");
		}
		if(ptmp->listener->gc) {
			ptmp->listener->gc();
			logprintf(LOG_DEBUG, "ran garbage collector");
		}
		sfree((void *)&ptmp->listener->id);
		sfree((void *)&ptmp->name);
		options_delete(ptmp->listener->options);
		if(ptmp->listener->plslen) {
			while(ptmp->listener->plslen) {
				ttmp = ptmp->listener->plslen;
				ptmp->listener->plslen = ptmp->listener->plslen->next;
				sfree((void *)&ttmp);
			}
		}
		sfree((void *)&ptmp->listener->plslen);
		if(ptmp->listener->devices) {
			while(ptmp->listener->devices) {
				dtmp = ptmp->listener->devices;
				sfree((void *)&dtmp->id);
				sfree((void *)&dtmp->desc);
				ptmp->listener->devices = ptmp->listener->devices->next;
				sfree((void *)&dtmp);
			}
		}
		sfree((void *)&ptmp->listener->devices);
		sfree((void *)&ptmp->listener);
		protocols = protocols->next;
		sfree((void *)&ptmp);
	}
	sfree((void *)&protocols);

	logprintf(LOG_DEBUG, "garbage collected protocol library");
	return EXIT_SUCCESS;
}
