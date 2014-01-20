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
#define __USE_GNU
#include <pthread.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "json.h"
#include "lm75.h"
#include "../pilight/wiringPi.h"
#include "../pilight/wiringPiI2C.h"

unsigned short lm75_loop = 1;
int lm75_nrfree = 0;

typedef struct lm75data_t {
	char **id;
	int nrid;
	int *fd;
} lm75data_t;

void lm75ParseCleanUp(void *arg) {
	struct lm75data_t *lm75data = (struct lm75data_t *)arg;
	int i = 0;

	if(lm75data->id) {
		for(i=0;i<lm75data->nrid;i++) {
			sfree((void *)&lm75data->id[i]);
		}
		sfree((void *)&lm75data->id);
	}
	if(lm75data->fd) {
		for(i=0;i<lm75data->nrid;i++) {
			if(lm75data->fd[i] > 0) {
				close(lm75data->fd[i]);
			}
		}
		sfree((void *)&lm75data->fd);
	}
	sfree((void *)&lm75data);
	
	lm75_loop = 0;
}

void *lm75Parse(void *param) {
	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct lm75data_t *lm75data = malloc(sizeof(struct lm75data_t));
	struct timeval tp;
	struct timespec ts;	
	int y = 0, interval = 10, temp_corr = 0, rc = 0;
	char *stmp;
	int firstrun = 1;

	lm75data->nrid = 0;
	
	pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;        
    pthread_cond_t cond;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				lm75data->id = realloc(lm75data->id, (sizeof(char *)*(size_t)(lm75data->nrid+1)));
				lm75data->id[lm75data->nrid] = malloc(strlen(stmp)+1);
				strcpy(lm75data->id[lm75data->nrid], stmp);
				lm75data->nrid++;
			}
			jchild = jchild->next;
		}
	}
	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
		json_find_number(jsettings, "temp-corr", &temp_corr);
	}
	json_delete(json);
	
	lm75data->fd = realloc(lm75data->fd, (sizeof(int)*(size_t)(lm75data->nrid+1)));
	for(y=0;y<lm75data->nrid;y++) {
		lm75data->fd[y] = wiringPiI2CSetup((int)strtol(lm75data->id[y], NULL, 16));
	}

	pthread_cleanup_push(lm75ParseCleanUp, (void *)lm75data);	
	
	while(lm75_loop) {
		rc = gettimeofday(&tp, NULL);
		ts.tv_sec = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;
		if(firstrun) {
			ts.tv_sec += 1;
			firstrun = 0;
		} else {
			ts.tv_sec += interval;
		}

		pthread_mutex_lock(&mutex);
		rc = pthread_cond_timedwait(&cond, &mutex, &ts);
		if(rc == ETIMEDOUT) {		
			for(y=0;y<lm75data->nrid;y++) {
				if(lm75data->fd[y] > 0) {
					int raw = wiringPiI2CReadReg16(lm75data->fd[y], 0x00);            
					float temp = ((float)((raw&0x00ff)+((raw>>15)?0:0.5))*10);

					lm75->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mkstring(lm75data->id[y]));
					json_append_member(code, "temperature", json_mknumber((int)temp+temp_corr));

					json_append_member(lm75->message, "code", code);
					json_append_member(lm75->message, "origin", json_mkstring("receiver"));
					json_append_member(lm75->message, "protocol", json_mkstring(lm75->id));

					pilight.broadcast(lm75->id, lm75->message);
					json_delete(lm75->message);
					lm75->message = NULL;
				} else {
					logprintf(LOG_DEBUG, "error connecting to lm75");
					logprintf(LOG_DEBUG, "(probably i2c bus error from wiringPiI2CSetup)");
					logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
					sleep(1);
				}
			}
		}
		pthread_mutex_unlock(&mutex);
	}
	
	pthread_cleanup_pop(1);

	return (void *)NULL;
}

void lm75InitDev(JsonNode *jdevice) {
	wiringPiSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("lm75", &lm75Parse, (void *)json);
	sfree((void *)&output);
}

void lm75Init(void) {
	protocol_register(&lm75);
	protocol_set_id(lm75, "lm75");
	protocol_device_add(lm75, "lm75", "TI I2C Temperature Sensor");
	lm75->devtype = WEATHER;
	lm75->hwtype = SENSOR;

    options_add(&lm75->options, 't', "temperature", has_value, config_value, "^[0-9]{1,3}$");
    options_add(&lm75->options, 'i', "id", has_value, config_id, "0x[0-9a-f]{2}");

	protocol_setting_add_number(lm75, "decimals", 1);
	protocol_setting_add_number(lm75, "humidity", 0);
	protocol_setting_add_number(lm75, "temperature", 1);
	protocol_setting_add_number(lm75, "battery", 0);
	protocol_setting_add_number(lm75, "interval", 10);

	lm75->initDev=&lm75InitDev;
}
