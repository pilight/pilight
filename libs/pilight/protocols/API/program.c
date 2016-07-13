/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <math.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <sys/wait.h>
#endif
#include <pthread.h>

#include "../../core/threadpool.h"
#include "../../core/eventpool.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "program.h"

#ifndef _WIN32
typedef struct data_t {
	char *dev;
	char *name;
	char *arguments;
	char *program;
	char *start;
	char *stop;
	int currentstate;
	int laststate;
	int interval;
	int wait;
	int thread;
	pthread_t pth;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;
	int pid = 0;

	if(settings->wait == 0) {
		return NULL;
	}

	if(settings->currentstate != settings->laststate) {

		struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
		if(data == NULL) {
			OUT_OF_MEMORY
		}
		memset(data, 0, sizeof(struct reason_code_received_t));
		if((pid = (int)findproc(settings->program, settings->arguments, 0)) > 0) {
			settings->currentstate = 1;
			snprintf(data->message, 1024, "{\"name\":\"%s\",\"state\":\"running\",\"pid\":%d}", settings->name, pid);
		} else {
			settings->currentstate = 0;
			snprintf(data->message, 1024, "{\"name\":\"%s\",\"state\":\"stopped\",\"pid\":%d}", settings->name, pid);
		}

		settings->laststate = settings->currentstate;

		strncpy(data->origin, "receiver", 255);
		data->protocol = program->id;
		if(strlen(pilight_uuid) > 0) {
			data->uuid = pilight_uuid;
		} else {
			data->uuid = NULL;
		}
		eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
	}

	struct timeval tv;
	tv.tv_sec = settings->interval;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(settings->dev, thread, tv, (void *)settings);

	return NULL;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	char *stmp = NULL;
	double itmp = 0.0;
	int match = 0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(program->id, jchild->string_) == 0) {
				match = 1;
				break;
		}
			jchild = jchild->next;
		}
	}

	if(match == 0) {
		return NULL;
	}

	if((node = MALLOC(sizeof(struct data_t)))== NULL) {
		OUT_OF_MEMORY
	}

	memset(node, '\0', sizeof(struct data_t));
	node->interval = 1;
	node->laststate = -1;
	node->wait = 0;
	node->thread = 0;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "name") == 0) {
					if((node->name = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(node->name, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		node->interval = (int)round(itmp);

	if(json_find_string(jdevice, "program", &stmp) == 0) {
		if((node->program = MALLOC(strlen(stmp)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->program, stmp);
	}
	if(json_find_string(jdevice, "arguments", &stmp) == 0) {
		if((node->arguments = MALLOC(strlen(stmp)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->arguments, stmp);
	}
	if(json_find_string(jdevice, "stop-command", &stmp) == 0) {
		if((node->stop = MALLOC(strlen(stmp)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->stop, stmp);
	}
	if(json_find_string(jdevice, "start-command", &stmp) == 0) {
		if((node->start = MALLOC(strlen(stmp)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->start, stmp);
	}

	if((node->dev = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->dev, jdevice->key);

	node->next = data;
	data = node;

	tv.tv_sec = node->interval;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);

	return NULL;
}

static void *execute(void *param) {
	struct data_t *p = param;
	int pid = 0;
	int result = 0;
	if((pid = (int)findproc(p->program, p->arguments, 0)) > 0) {
		if(strlen(p->stop) == 0) {
			kill(pid, SIGKILL);
		} else {
			result = system(p->stop);
		}
	} else {
		result = system(p->start);
	}

	/* Check of the user wanted to stop pilight */
	if(WIFSIGNALED(result)) {
		int ppid = 0;
		/* Find the pilight daemon pid */
		if((ppid = (int)findproc(progname, NULL, 0)) > 0) {
			/* Send a sigint to ourself */
			kill(ppid, SIGINT);
		}
	}

	p->wait = 0;
	p->thread = 0;

	p->laststate = -1;

	return NULL;
}

static int createCode(struct JsonNode *code, char *message) {
	char *name = NULL;
	double itmp = -1;
	int state = -1;
	int pid = 0;

	if(json_find_string(code, "name", &name) == 0) {
		if(pilight.process == PROCESS_DAEMON) {
			struct data_t *tmp = data;
			while(tmp) {
				if(strcmp(tmp->name, name) == 0) {
					if(tmp->wait == 0) {
						if(tmp->name != NULL && tmp->stop != NULL && tmp->start != NULL) {
							if(json_find_number(code, "running", &itmp) == 0) {
								state = 1;
							} else if(json_find_number(code, "stopped", &itmp) == 0) {
								state = 0;
							}

							if((pid = (int)findproc(tmp->program, tmp->arguments, 0)) > 0 && state == 1) {
								logprintf(LOG_INFO, "program \"%s\" already running", tmp->name);
								break;
							} else if(pid == -1 && state == 0) {
								logprintf(LOG_INFO, "program \"%s\" already stopped", tmp->name);
								break;
							} else {
								struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
								if(data == NULL) {
									OUT_OF_MEMORY
								}
								data->repeat = 1;
								snprintf(data->message, 1024, "{\"name\":\"%s\",\"state\":\"pending\"}", name);
								strncpy(message, data->message, 255);

								strcpy(data->origin, "receiver");
								data->protocol = program->id;
								if(strlen(pilight_uuid) > 0) {
									data->uuid = pilight_uuid;
								} else {
									data->uuid = NULL;
								}
								eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

								tmp->wait = 1;
								pthread_create(&tmp->pth, NULL, execute, (void *)tmp);
								tmp->thread = 1;
							}
						} else {
							logprintf(LOG_NOTICE, "program \"%s\" cannot be controlled", tmp->name);
						}
					} else {
						logprintf(LOG_NOTICE, "please wait for program \"%s\" to finish it's state change", tmp->name);
					}
					break;
				}
				tmp = tmp->next;
			}
		} else {
			int x = snprintf(message, 255, "{\"name\":\"%s\",", name);
			if(json_find_number(code, "running", &itmp) == 0) {
				x += snprintf(&message[x], 255-x, "\"state\":\"running\"");
			} else if(json_find_number(code, "stopped", &itmp) == 0) {
				x += snprintf(&message[x], 255-x, "\"state\":\"stopped\"");
			}
			x += snprintf(&message[x], 255-x, "}");
		}
	} else {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		while(tmp->wait > 0) {
			logprintf(LOG_NOTICE, "waiting for program \"%s\" to finish it's state change", tmp->name);
#ifdef _WIN32
			SleepEx(1000, True);
#else
			sleep(1);
#endif
		}
		while(tmp->thread > 0) {
			logprintf(LOG_NOTICE, "waiting for program \"%s\" to finish it's state change", tmp->name);
#ifdef _WIN32
			SleepEx(1000, True);
#else
			sleep(1);
#endif
		}
		pthread_join(tmp->pth, NULL);
		FREE(tmp->name);
		FREE(tmp->dev);
		FREE(tmp->arguments);
		FREE(tmp->program);
		FREE(tmp->start);
		FREE(tmp->stop);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}

static void printHelp(void) {
	printf("\t -t --running\t\t\tstart the program\n");
	printf("\t -f --stopped\t\t\tstop the program\n");
	printf("\t -n --name=name\t\t\tname of the program\n");
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void programInit(void) {
	protocol_register(&program);
	protocol_set_id(program, "program");
	protocol_device_add(program, "program", "Start / Stop / State of a program");
	program->devtype = PENDINGSW;
	program->hwtype = API;
	program->multipleId = 0;

	options_add(&program->options, 'n', "name", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'x', "start-command", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'y', "stop-command", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'p', "program", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'i', "pid", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&program->options, 'a', "arguments", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&program->options, 't', "running", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'd', "pending", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'f', "stopped", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&program->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&program->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&program->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");

#ifndef _WIN32
	program->createCode=&createCode;
	program->printHelp=&printHelp;
	// program->initDev=&initDev;
	program->gc=&gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "program";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	programInit();
}
#endif
