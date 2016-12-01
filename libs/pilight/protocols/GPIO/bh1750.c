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
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/threads.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "../../core/json.h"
#ifndef _WIN32
	#include "../../../wiringx/wiringX.h"
#endif
#include "../protocol.h"
#include "bh1750.h"

#if !defined(__FreeBSD__) && !defined(_WIN32)
typedef struct settings_t {
	char **id;
	int nrid;
	int *fd;
} settings_t;

static unsigned short loop = 1;
static int threads = 0;

static pthread_mutex_t lock;
static pthread_mutexattr_t attr;

static void *thread(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct settings_t *bh1750data = MALLOC(sizeof(struct settings_t));
	int y = 0, interval = 10, nrloops = 0 ;
	int data = 0 , lux = 0 , raw = 0;
	char *stmp = NULL;
	double itmp = -1 ;
	

	if(bh1750data == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	bh1750data->nrid = 0;
	bh1750data->id = NULL;
	bh1750data->fd = 0;

	threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				if((bh1750data->id = REALLOC(bh1750data->id, (sizeof(char *)*(size_t)(bh1750data->nrid+1)))) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				if((bh1750data->id[bh1750data->nrid] = MALLOC(strlen(stmp)+1)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				strcpy(bh1750data->id[bh1750data->nrid], stmp);
				bh1750data->nrid++;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);

	if((bh1750data->fd = REALLOC(bh1750data->fd, (sizeof(int)*(size_t)(bh1750data->nrid+1)))) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	for(y=0;y<bh1750data->nrid;y++) {
		bh1750data->fd[y] = wiringXI2CSetup((int)strtol(bh1750data->id[y], NULL, 16));
	}

	while(loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&lock);
			for(y=0;y<bh1750data->nrid;y++) {
				if(bh1750data->fd[y] > 0) {
					wiringXI2CWrite(bh1750data->fd[y],0x01);
					raw = wiringXI2CWrite(bh1750data->fd[y],0x10);
					usleep (0.5 * 1000000);
					data = wiringXI2CReadReg16(bh1750data->fd[y], 0x00);
					wiringXI2CWrite(bh1750data->fd[y],0x00);
					lux = (int) (((data & 0xff00)>>8 | (data & 0x00ff)<<8 )) / 1.2 ;

					bh1750->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mkstring(bh1750data->id[y]));
					json_append_member(code, "illuminance", json_mknumber(lux, 0));
					json_append_member(bh1750->message, "message", code);
					json_append_member(bh1750->message, "origin", json_mkstring("receiver"));
					json_append_member(bh1750->message, "protocol", json_mkstring(bh1750->id));
					if(pilight.broadcast != NULL) {
						pilight.broadcast(bh1750->id, bh1750->message, PROTOCOL);
					}
					json_delete(bh1750->message);
					bh1750->message = NULL;
				} else {
					logprintf(LOG_NOTICE, "error connecting to bh1750");
					logprintf(LOG_DEBUG, "(probably i2c bus error from wiringXI2CSetup)");
					logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
					protocol_thread_wait(node, 1, &nrloops);
				}
			}
			pthread_mutex_unlock(&lock);
		}
	}
	pthread_mutex_unlock(&lock);

	if(bh1750data->id) {
		for(y=0;y<bh1750data->nrid;y++) {
			FREE(bh1750data->id[y]);
		}
		FREE(bh1750->id);
	}
	if(bh1750data->fd) {
		for(y=0;y<bh1750data->nrid;y++) {
			if(bh1750data->fd[y] > 0) {
				close(bh1750data->fd[y]);
			}
		}
		FREE(bh1750data->fd);
	}
	FREE(bh1750data);
	threads--;

	return (void *)NULL;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	if(wiringXSupported() == 0 && wiringXSetup() == 0) {
		loop = 1;
		char *output = json_stringify(jdevice, NULL);
		JsonNode *json = json_decode(output);
		json_free(output);

		struct protocol_threads_t *node = protocol_thread_init(bh1750, json);
		return threads_register("bh1750", &thread, (void *)node, 0);
	} else {
		return NULL;
	}
}

static void threadGC(void) {
	loop = 0;
	protocol_thread_stop(bh1750);
	while(threads > 0) {
		usleep(10);
	}
	protocol_thread_free(bh1750);
}


#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void bh1750Init(void) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &attr);
#endif

	protocol_register(&bh1750);
	protocol_set_id(bh1750, "bh1750");
	protocol_device_add(bh1750, "bh1750", "ROHM I2C illuminance sensor");
	bh1750->devtype = WEATHER;
	bh1750->hwtype = SENSOR;

	options_add(&bh1750->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "0x[0-9a-fA-F]{2}");
	options_add(&bh1750->options, 'e', "illuminance", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1}$");
	options_add(&bh1750->options, 0, "poll-intervall", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");
	options_add(&bh1750->options, 0, "show-illuminance", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32)
	bh1750->initDev=&initDev;
	bh1750->threadGC=&threadGC;
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "bh1750";
	module->version = "1.0";
	module->reqversion = "7.0";
	module->reqcommit = "1";
}

void init(void) {
	bh1750Init();
}
#endif
