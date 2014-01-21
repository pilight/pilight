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
#include "json.h"
#include "gc.h"
#include "rpi_temp.h"

unsigned short rpi_temp_loop = 1;
unsigned short rpi_temp_nrfree = 0;
char *rpi_temp_temp = NULL;

void rpiTempParseCleanUp(void *arg) {
	sfree((void *)&arg);
	
	rpi_temp_loop = 0;
}

void *rpiTempParse(void *param) {
	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jsettings = NULL;
	struct timeval tp;
	struct timespec ts;	
	struct stat st;

	FILE *fp = NULL;
	int itmp = 0;
	int *id = malloc(sizeof(int));
	char *content = NULL;
	int interval = 10, temp_corr = 0, nrid = 0, y = 0, rc = 0;
	size_t bytes = 0;
	int firstrun = 1;

	pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;        
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				id = realloc(id, (sizeof(int)*(size_t)(nrid+1)));
				id[nrid] = itmp;
				nrid++;
			}
			jchild = jchild->next;
		}
	}

	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
		json_find_number(jsettings, "temp-corr", &temp_corr);
	}
	json_delete(json);

	pthread_cleanup_push(rpiTempParseCleanUp, (void *)id);

	while(rpi_temp_loop) {
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
			for(y=0;y<nrid;y++) {
				if((fp = fopen(rpi_temp_temp, "rb"))) {
					fstat(fileno(fp), &st);
					bytes = (size_t)st.st_size;

					if(!(content = calloc(bytes+1, sizeof(char)))) {
						logprintf(LOG_ERR, "out of memory");
						fclose(fp);
						break;
					}

					if(fread(content, sizeof(char), bytes, fp) == -1) {
						logprintf(LOG_ERR, "cannot read config file: %s", rpi_temp_temp);
						fclose(fp);
						sfree((void *)&content);
						break;
					}

					fclose(fp);
					int temp = atoi(content)+temp_corr;
					sfree((void *)&content);

					rpiTemp->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mknumber(id[y]));
					json_append_member(code, "temperature", json_mknumber(temp));
										
					json_append_member(rpiTemp->message, "code", code);
					json_append_member(rpiTemp->message, "origin", json_mkstring("receiver"));
					json_append_member(rpiTemp->message, "protocol", json_mkstring(rpiTemp->id));
										
					pilight.broadcast(rpiTemp->id, rpiTemp->message);
					json_delete(rpiTemp->message);
					rpiTemp->message = NULL;
				} else {
					logprintf(LOG_ERR, "CPU RPI device %s does not exists", rpi_temp_temp);
				}
			}
		}
		pthread_mutex_unlock(&mutex);
	}

	pthread_cleanup_pop(1);

	return (void *)NULL;
}

void rpiTempInitDev(JsonNode *jdevice) {
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("rpi_temp", &rpiTempParse, (void *)json);
	sfree((void *)&output);
}

int rpiTempGC(void) {
	sfree((void *)&rpi_temp_temp);
	return 1;
}

void rpiTempInit(void) {
	
	gc_attach(rpiTempGC);

	protocol_register(&rpiTemp);
	protocol_set_id(rpiTemp, "rpi_temp");
	protocol_device_add(rpiTemp, "rpi_temp", "RPi CPU/GPU temperature sensor");
	rpiTemp->devtype = WEATHER;
	rpiTemp->hwtype = SENSOR;

	options_add(&rpiTemp->options, 't', "temperature", has_value, config_value, "^[0-9]{1,5}$");
	options_add(&rpiTemp->options, 'i', "id", has_value, config_id, "[0-9]");

	protocol_setting_add_number(rpiTemp, "decimals", 3);
	protocol_setting_add_number(rpiTemp, "humidity", 0);
	protocol_setting_add_number(rpiTemp, "temperature", 1);
	protocol_setting_add_number(rpiTemp, "battery", 0);
	protocol_setting_add_number(rpiTemp, "interval", 10);

	rpi_temp_temp = malloc(38);
	memset(rpi_temp_temp, '\0', 38);
	strcpy(rpi_temp_temp, "/sys/class/thermal/thermal_zone0/temp");

	rpiTemp->initDev=&rpiTempInitDev;
}