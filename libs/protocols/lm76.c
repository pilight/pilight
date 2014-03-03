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
#include "lm76.h"
#include "../pilight/wiringPi.h"
#ifndef __FreeBSD__
#include "../pilight/wiringPiI2C.h"
#endif

unsigned short lm76_loop = 1;
int lm76_threads = 0;

typedef struct lm76data_t {
	char **id;
	int nrid;
	int *fd;
} lm76data_t;

void *lm76Parse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct lm76data_t *lm76data = malloc(sizeof(struct lm76data_t));
	int y = 0, interval = 10;
	int temp_corr = 0, nrloops = 0;
	char *stmp = NULL;

	if(!lm76data) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	lm76data->nrid = 0;
	lm76data->id = NULL;
	lm76data->fd = 0;

	lm76_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				lm76data->id = realloc(lm76data->id, (sizeof(char *)*(size_t)(lm76data->nrid+1)));
				if(!lm76data->id) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				lm76data->id[lm76data->nrid] = malloc(strlen(stmp)+1);
				if(!lm76data->id[lm76data->nrid]) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(lm76data->id[lm76data->nrid], stmp);
				lm76data->nrid++;
			}
			jchild = jchild->next;
		}
	}
	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
		json_find_number(jsettings, "temp-corr", &temp_corr);
	}

#ifndef __FreeBSD__	
	lm76data->fd = realloc(lm76data->fd, (sizeof(int)*(size_t)(lm76data->nrid+1)));
	if(!lm76data->fd) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	for(y=0;y<lm76data->nrid;y++) {
		lm76data->fd[y] = wiringPiI2CSetup((int)strtol(lm76data->id[y], NULL, 16));
	}
#endif
	
	while(lm76_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
#ifndef __FreeBSD__	
			for(y=0;y<lm76data->nrid;y++) {
				if(lm76data->fd[y] > 0) {		
					int raw = wiringPiI2CReadReg16(lm76data->fd[y], 0x00);            
					float temp = ((float)((raw&0x00ff)+((raw>>12)*0.0625))*1000);

					lm76->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mkstring(lm76data->id[y]));
					json_append_member(code, "temperature", json_mknumber((int)temp+temp_corr));

					json_append_member(lm76->message, "code", code);
					json_append_member(lm76->message, "origin", json_mkstring("receiver"));
					json_append_member(lm76->message, "protocol", json_mkstring(lm76->id));

					pilight.broadcast(lm76->id, lm76->message);
					json_delete(lm76->message);
					lm76->message = NULL;
				} else {
					logprintf(LOG_DEBUG, "error connecting to lm76");
					logprintf(LOG_DEBUG, "(probably i2c bus error from wiringPiI2CSetup)");
					logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
					sleep(1);
				}
			}
#endif
		}
	}

	if(lm76data->id) {
		for(y=0;y<lm76data->nrid;y++) {
			sfree((void *)&lm76data->id[y]);
		}
		sfree((void *)&lm76data->id);
	}
	if(lm76data->fd) {
		for(y=0;y<lm76data->nrid;y++) {
			if(lm76data->fd[y] > 0) {
				close(lm76data->fd[y]);
			}
		}
		sfree((void *)&lm76data->fd);
	}
	sfree((void *)&lm76data);
	lm76_threads--;

	return (void *)NULL;
}

void lm76InitDev(JsonNode *jdevice) {
	wiringPiSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);

	struct protocol_threads_t *node = protocol_thread_init(lm76, json);
	threads_register("lm76", &lm76Parse, (void *)node, 0);
	sfree((void *)&output);
}

void lm76GC(void) {
	lm76_loop = 0;
	protocol_thread_stop(lm76);
	while(lm76_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(lm76);
}

void lm76Init(void) {
	protocol_register(&lm76);
	protocol_set_id(lm76, "lm76");
	protocol_device_add(lm76, "lm76", "TI I2C Temperature Sensor");
	lm76->devtype = WEATHER;
	lm76->hwtype = SENSOR;

	options_add(&lm76->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, "^[0-9]{1,3}$");
	options_add(&lm76->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, "0x[0-9a-f]{2}");

	protocol_setting_add_number(lm76, "decimals", 3);
	protocol_setting_add_number(lm76, "humidity", 0);
	protocol_setting_add_number(lm76, "temperature", 1);
	protocol_setting_add_number(lm76, "battery", 0);
	protocol_setting_add_number(lm76, "interval", 10);

	lm76->initDev=&lm76InitDev;
	lm76->gc=&lm76GC;
}
