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

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../core/threads.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "cpu_temp.h"

#ifndef _WIN32
static unsigned short loop = 1;
static unsigned short threads = 0;
static char cpu_path[] = "/sys/class/thermal/thermal_zone0/temp";

static pthread_mutex_t lock;
static pthread_mutexattr_t attr;

static void *thread(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct stat st;

	FILE *fp = NULL;
	double itmp = 0;
	int *id = MALLOC(sizeof(int));
	char *content = NULL;
	int nrloops = 0, nrid = 0, y = 0, interval = 0;
	double temp_offset = 0.0;
	size_t bytes = 0;

	threads++;

	if(id == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				id = REALLOC(id, (sizeof(int)*(size_t)(nrid+1)));
				id[nrid] = (int)round(itmp);
				nrid++;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	json_find_number(json, "temperature-offset", &temp_offset);

	while(loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&lock);
			for(y=0;y<nrid;y++) {
				if((fp = fopen(cpu_path, "rb"))) {
					fstat(fileno(fp), &st);
					bytes = (size_t)st.st_size;

					if((content = REALLOC(content, bytes+1)) == NULL) {
						fprintf(stderr, "out of memory\n");
						exit(EXIT_FAILURE);
					}
					memset(content, '\0', bytes+1);

					if(fread(content, sizeof(char), bytes, fp) == -1) {
						logprintf(LOG_NOTICE, "cannot read file: %s", cpu_path);
						fclose(fp);
						break;
					} else {
						fclose(fp);
						double temp = atof(content)+temp_offset;
						FREE(content);

						cpuTemp->message = json_mkobject();
						JsonNode *code = json_mkobject();
						json_append_member(code, "id", json_mknumber(id[y], 0));
						json_append_member(code, "temperature", json_mknumber((temp/1000), 3));

						json_append_member(cpuTemp->message, "message", code);
						json_append_member(cpuTemp->message, "origin", json_mkstring("receiver"));
						json_append_member(cpuTemp->message, "protocol", json_mkstring(cpuTemp->id));

						if(pilight.broadcast != NULL) {
							pilight.broadcast(cpuTemp->id, cpuTemp->message, PROTOCOL);
						}
						json_delete(cpuTemp->message);
						cpuTemp->message = NULL;
					}
				} else {
					logprintf(LOG_NOTICE, "CPU sysfs \"%s\" does not exist", cpu_path);
				}
			}
			pthread_mutex_unlock(&lock);
		}
	}
	pthread_mutex_unlock(&lock);

	FREE(id);
	threads--;
	return (void *)NULL;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	json_free(output);

	struct protocol_threads_t *node = protocol_thread_init(cpuTemp, json);
	return threads_register("cpu_temp", &thread, (void *)node, 0);
}

static void threadGC(void) {
	loop = 0;
	protocol_thread_stop(cpuTemp);
	while(threads > 0) {
		usleep(10);
	}
	protocol_thread_free(cpuTemp);
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void cpuTempInit(void) {
#ifndef _WIN32
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &attr);
#endif

	protocol_register(&cpuTemp);
	protocol_set_id(cpuTemp, "cpu_temp");
	protocol_device_add(cpuTemp, "cpu_temp", "CPU temperature sensor");
	cpuTemp->devtype = WEATHER;
	cpuTemp->hwtype = SENSOR;

	options_add(&cpuTemp->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&cpuTemp->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");

	// options_add(&cpuTemp->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&cpuTemp->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&cpuTemp->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&cpuTemp->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&cpuTemp->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

#ifndef _WIN32
	cpuTemp->initDev=&initDev;
	cpuTemp->threadGC=&threadGC;
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "cpu_temp";
	module->version = "1.6";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	cpuTempInit();
}
#endif
