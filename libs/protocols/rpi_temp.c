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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "json.h"
#include "gc.h"
#include "rpi_temp.h"

static unsigned short rpi_temp_loop = 1;
static unsigned short rpi_temp_threads = 0;
static char rpi_temp[] = "/sys/class/thermal/thermal_zone0/temp";

static pthread_mutex_t rpi_templock;
static pthread_mutexattr_t rpi_tempattr;

static void *rpiTempParse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct stat st;

	FILE *fp = NULL;
	double itmp = 0;
	int *id = malloc(sizeof(int));
	char *content = NULL;
	int interval = 10, temp_offset = 0;
	int nrloops = 0, nrid = 0, y = 0;
	size_t bytes = 0;

	rpi_temp_threads++;

	if(!id) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				id = realloc(id, (sizeof(int)*(size_t)(nrid+1)));
				id[nrid] = (int)round(itmp);
				nrid++;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	if(json_find_number(json, "device-temperature-offset", &itmp) == 0)
		temp_offset = (int)round(itmp);

	while(rpi_temp_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&rpi_templock);
			for(y=0;y<nrid;y++) {
				if((fp = fopen(rpi_temp, "rb"))) {
					fstat(fileno(fp), &st);
					bytes = (size_t)st.st_size;

					if(!(content = realloc(content, bytes+1))) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					memset(content, '\0', bytes+1);

					if(fread(content, sizeof(char), bytes, fp) == -1) {
						logprintf(LOG_ERR, "cannot read file: %s", rpi_temp);
						fclose(fp);
						break;
					} else {
						fclose(fp);
						int temp = atoi(content)+temp_offset;
						sfree((void *)&content);

						rpiTemp->message = json_mkobject();
						JsonNode *code = json_mkobject();
						json_append_member(code, "id", json_mknumber(id[y]));
						json_append_member(code, "temperature", json_mknumber(temp));

						json_append_member(rpiTemp->message, "message", code);
						json_append_member(rpiTemp->message, "origin", json_mkstring("receiver"));
						json_append_member(rpiTemp->message, "protocol", json_mkstring(rpiTemp->id));

						pilight.broadcast(rpiTemp->id, rpiTemp->message);
						json_delete(rpiTemp->message);
						rpiTemp->message = NULL;
					}
				} else {
					logprintf(LOG_ERR, "CPU RPI device %s does not exists", rpi_temp);
				}
			}
			pthread_mutex_unlock(&rpi_templock);
		}
	}

	sfree((void *)&id);
	rpi_temp_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *rpiTempInitDev(JsonNode *jdevice) {
	rpi_temp_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(rpiTemp, json);
	return threads_register("rpi_temp", &rpiTempParse, (void *)node, 0);
}

static void rpiTempThreadGC(void) {
	rpi_temp_loop = 0;
	protocol_thread_stop(rpiTemp);
	while(rpi_temp_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(rpiTemp);
}

#ifndef MODULE
__attribute__((weak))
#endif
void rpiTempInit(void) {
	pthread_mutexattr_init(&rpi_tempattr);
	pthread_mutexattr_settype(&rpi_tempattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&rpi_templock, &rpi_tempattr);

	protocol_register(&rpiTemp);
	protocol_set_id(rpiTemp, "rpi_temp");
	protocol_device_add(rpiTemp, "rpi_temp", "RPi CPU/GPU temperature sensor");
	rpiTemp->devtype = WEATHER;
	rpiTemp->hwtype = SENSOR;

	options_add(&rpiTemp->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&rpiTemp->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "[0-9]");

	options_add(&rpiTemp->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&rpiTemp->options, 0, "device-temperature-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&rpiTemp->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&rpiTemp->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&rpiTemp->options, 0, "poll-interval", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	rpiTemp->initDev=&rpiTempInitDev;
	rpiTemp->threadGC=&rpiTempThreadGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "rpi_temp";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	rpiTempInit();
}
#endif
