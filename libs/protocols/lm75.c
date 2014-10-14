/*
	Copyright (C) 2014 CurlyMo

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
#include <math.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "json.h"
#include "lm75.h"
#include "../pilight/wiringX.h"

typedef struct lm75data_t {
	char **id;
	int nrid;
	int *fd;
} lm75data_t;

static unsigned short lm75_loop = 1;
static int lm75_threads = 0;

static pthread_mutex_t lm75lock;
static pthread_mutexattr_t lm75attr;

static void *lm75Parse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct lm75data_t *lm75data = malloc(sizeof(struct lm75data_t));
	int y = 0, interval = 10;
	int temp_offset = 0, nrloops = 0;
	char *stmp = NULL;
	double itmp = -1;

	if(!lm75data) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	lm75data->nrid = 0;
	lm75data->id = NULL;
	lm75data->fd = 0;

	lm75_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				lm75data->id = realloc(lm75data->id, (sizeof(char *)*(size_t)(lm75data->nrid+1)));
				if(!lm75data->id) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				lm75data->id[lm75data->nrid] = malloc(strlen(stmp)+1);
				if(!lm75data->id[lm75data->nrid]) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(lm75data->id[lm75data->nrid], stmp);
				lm75data->nrid++;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	if(json_find_number(json, "device-temperature-offset", &itmp) == 0)
		temp_offset = (int)round(itmp);

#ifndef __FreeBSD__
	lm75data->fd = realloc(lm75data->fd, (sizeof(int)*(size_t)(lm75data->nrid+1)));
	if(!lm75data->fd) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	for(y=0;y<lm75data->nrid;y++) {
		lm75data->fd[y] = wiringXI2CSetup((int)strtol(lm75data->id[y], NULL, 16));
	}
#endif

	while(lm75_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
#ifndef __FreeBSD__
			pthread_mutex_lock(&lm75lock);
			for(y=0;y<lm75data->nrid;y++) {
				if(lm75data->fd[y] > 0) {
					int raw = wiringXI2CReadReg16(lm75data->fd[y], 0x00);
					float temp = ((float)((raw&0x00ff)+((raw>>15)?0:0.5))*10);

					lm75->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mkstring(lm75data->id[y]));
					json_append_member(code, "temperature", json_mknumber((int)temp+temp_offset));

					json_append_member(lm75->message, "message", code);
					json_append_member(lm75->message, "origin", json_mkstring("receiver"));
					json_append_member(lm75->message, "protocol", json_mkstring(lm75->id));

					pilight.broadcast(lm75->id, lm75->message);
					json_delete(lm75->message);
					lm75->message = NULL;
				} else {
					logprintf(LOG_DEBUG, "error connecting to lm75");
					logprintf(LOG_DEBUG, "(probably i2c bus error from wiringXI2CSetup)");
					logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
					protocol_thread_wait(node, 1, &nrloops);
				}
			}
			pthread_mutex_unlock(&lm75lock);
#endif
		}
	}

	if(lm75data->id) {
		for(y=0;y<lm75data->nrid;y++) {
			sfree((void *)&lm75data->id[y]);
		}
		sfree((void *)&lm75data->id);
	}
	if(lm75data->fd) {
		for(y=0;y<lm75data->nrid;y++) {
			if(lm75data->fd[y] > 0) {
				close(lm75data->fd[y]);
			}
		}
		sfree((void *)&lm75data->fd);
	}
	sfree((void *)&lm75data);
	lm75_threads--;

	return (void *)NULL;
}

static struct threadqueue_t *lm75InitDev(JsonNode *jdevice) {
	lm75_loop = 1;
	wiringXSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(lm75, json);
	return threads_register("lm75", &lm75Parse, (void *)node, 0);
}

static void lm75ThreadGC(void) {
	lm75_loop = 0;
	protocol_thread_stop(lm75);
	while(lm75_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(lm75);
}

#ifndef MODULE
__attribute__((weak))
#endif
void lm75Init(void) {
	pthread_mutexattr_init(&lm75attr);
	pthread_mutexattr_settype(&lm75attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lm75lock, &lm75attr);

	protocol_register(&lm75);
	protocol_set_id(lm75, "lm75");
	protocol_device_add(lm75, "lm75", "TI I2C Temperature Sensor");
	lm75->devtype = WEATHER;
	lm75->hwtype = SENSOR;

	options_add(&lm75->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&lm75->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, "0x[0-9a-f]{2}");

	options_add(&lm75->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&lm75->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&lm75->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	lm75->initDev=&lm75InitDev;
	lm75->threadGC=&lm75ThreadGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "lm75";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	lm75Init();
}
#endif
