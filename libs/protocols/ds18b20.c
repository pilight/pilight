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
#include "ds18b20.h"

unsigned short ds18b20_loop = 1;
unsigned short ds18b20_nrfree = 0;
char *ds18b20_path = NULL;

void ds18b20ParseCleanUp(void *arg) {
	sfree((void *)&arg);
	
	ds18b20_loop = 0;
}

void *ds18b20Parse(void *param) {
	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jsettings = NULL;
	struct dirent *file = NULL;
	struct stat st;
	struct timeval tp;
	struct timespec ts;		

	DIR *d = NULL;
	FILE *fp = NULL;
	char *ds18b20_sensor = NULL;
	char *stmp = NULL;
	char **id = NULL;
	char *content = NULL;
	int w1valid = 0, w1temp = 0, interval = 10, x = 0;
	int temp_corr = 0, rc = 0, nrid = 0, y = 0;
	size_t bytes = 0;
	int firstrun = 1;

	pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;        
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				id = realloc(id, (sizeof(char *)*(size_t)(nrid+1)));
				id[nrid] = malloc(strlen(stmp)+1);
				strcpy(id[nrid], stmp);
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

	pthread_cleanup_push(ds18b20ParseCleanUp, (void *)id);
	
	while(ds18b20_loop) {
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
				ds18b20_sensor = realloc(ds18b20_sensor, strlen(ds18b20_path)+strlen(id[y])+5);
				sprintf(ds18b20_sensor, "%s28-%s/", ds18b20_path, id[y]);
				if((d = opendir(ds18b20_sensor))) {
					while((file = readdir(d)) != NULL) {
						if(file->d_type == DT_REG) {
							if(strcmp(file->d_name, "w1_slave") == 0) {
								size_t w1slavelen = strlen(ds18b20_sensor)+10;
								char ds18b20_w1slave[w1slavelen];
								memset(ds18b20_w1slave, '\0', w1slavelen);
								strncpy(ds18b20_w1slave, ds18b20_sensor, strlen(ds18b20_sensor));
								strcat(ds18b20_w1slave, "w1_slave");

								if(!(fp = fopen(ds18b20_w1slave, "rb"))) {
									logprintf(LOG_ERR, "cannot read w1 file: %s", ds18b20_w1slave);
									break;
								}

								fstat(fileno(fp), &st);
								bytes = (size_t)st.st_size;

								if(!(content = calloc(bytes+1, sizeof(char)))) {
									logprintf(LOG_ERR, "out of memory");
									fclose(fp);
									break;
								}

								if(fread(content, sizeof(char), bytes, fp) == -1) {
									logprintf(LOG_ERR, "cannot read config file: %s", ds18b20_w1slave);
									fclose(fp);
									sfree((void *)&content);
									break;
								}
								fclose(fp);
								w1valid = 0;
								char *pch = strtok(content, "\n=: ");
								x = 0;
								while(pch) {
									if(strlen(pch) > 2) {
										if(x == 1 && strstr(pch, "YES")) {
											w1valid = 1;
										}
										if(x == 2) {	
											w1temp = atoi(pch)+temp_corr;
										}
										x++;
									}
									pch = strtok(NULL, "\n=: ");
								}
								sfree((void *)&content);
								if(w1valid) {
									ds18b20->message = json_mkobject();
									
									JsonNode *code = json_mkobject();
									
									json_append_member(code, "id", json_mkstring(id[y]));
									json_append_member(code, "temperature", json_mknumber(w1temp));
									
									json_append_member(ds18b20->message, "code", code);
									json_append_member(ds18b20->message, "origin", json_mkstring("receiver"));
									json_append_member(ds18b20->message, "protocol", json_mkstring(ds18b20->id));
									
									pilight.broadcast(ds18b20->id, ds18b20->message);
									json_delete(ds18b20->message);
									ds18b20->message = NULL;
								}
							}
						}
					}
					closedir(d);
				} else {
					logprintf(LOG_ERR, "1-wire device %s does not exists", ds18b20_sensor);
				}
			}
		}
		pthread_mutex_unlock(&mutex);
	}
	
	pthread_cleanup_pop(1);

	return (void *)NULL;
}

void ds18b20InitDev(JsonNode *jdevice) {
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("ds18b20", &ds18b20Parse, (void *)json);
	sfree((void *)&output);
}

int ds18b20GC(void) {
	sfree((void *)&ds18b20_path);
	return 1;
}

void ds18b20Init(void) {
	
	gc_attach(ds18b20GC);

	protocol_register(&ds18b20);
	protocol_set_id(ds18b20, "ds18b20");
	protocol_device_add(ds18b20, "ds18b20", "1-wire Temperature Sensor");
	ds18b20->devtype = WEATHER;
	ds18b20->hwtype = SENSOR;

	options_add(&ds18b20->options, 't', "temperature", has_value, config_value, "^[0-9]{1,5}$");
	options_add(&ds18b20->options, 'i', "id", has_value, config_id, "^[a-z0-9]{12}$");

	protocol_setting_add_number(ds18b20, "decimals", 3);
	protocol_setting_add_number(ds18b20, "humidity", 0);
	protocol_setting_add_number(ds18b20, "temperature", 1);
	protocol_setting_add_number(ds18b20, "battery", 0);
	protocol_setting_add_number(ds18b20, "interval", 10);

	ds18b20_path = malloc(21);
	memset(ds18b20_path, '\0', 21);	
	strcpy(ds18b20_path, "/sys/bus/w1/devices/");

	ds18b20->initDev=&ds18b20InitDev;
}
