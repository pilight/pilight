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
#include "http_lib.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "json.h"
#include "gc.h"
#include "openweathermap.h"

typedef struct openweathermap_data_t {
	char *country;
	char *location;
	struct openweathermap_data_t *next;
} openweathermap_data_t;

unsigned short openweathermap_loop = 1;
unsigned short openweathermap_nrfree = 0;

void openweathermapParseCleanUp(void *arg) {
	struct openweathermap_data_t *openweathermap_data = (struct openweathermap_data_t *)arg;
	struct openweathermap_data_t *wtmp = NULL;

	openweathermap_loop = 0;
	
	while(openweathermap_data) {
		wtmp = openweathermap_data;
		sfree((void *)&openweathermap_data->country);
		sfree((void *)&openweathermap_data->location);
		openweathermap_data = openweathermap_data->next;
		sfree((void *)&wtmp);
	}
	sfree((void *)&openweathermap_data);
}

void *openweathermapParse(void *param) {
	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *node = NULL;
	struct JsonNode *jdata = NULL;
	struct JsonNode *jmain = NULL;
	struct openweathermap_data_t *openweathermap_data = NULL;
	struct openweathermap_data_t *wtmp = NULL;
	struct timeval tp;
	struct timespec ts;	
	int interval = 600;

	char url[1024];
	char *filename = NULL, *data = NULL;
	char typebuf[70];
	double temp = 0;
	int rc = 0, humi = 0, lg = 0, ret = 0;
	int firstrun = 1;
	
#ifndef __FreeBSD__
	pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;        
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
#else
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mutex, &attr);
#endif
	
    pthread_cond_init(&cond, NULL);

	int has_country = 0, has_location = 0;
	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			has_country = 0, has_location = 0;
			jchild1 = json_first_child(jchild);
			struct openweathermap_data_t* wnode = malloc(sizeof(struct openweathermap_data_t));
			if(!wnode) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			while(jchild1) {
				if(strcmp(jchild1->key, "location") == 0) {
					has_location = 1;
					wnode->location = malloc(strlen(jchild1->string_)+1);
					if(!wnode->location) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(wnode->location, jchild1->string_);
				}
				if(strcmp(jchild1->key, "country") == 0) {
					has_country = 1;
					wnode->country = malloc(strlen(jchild1->string_)+1);
					if(!wnode->country) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(wnode->country, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			if(has_country && has_location) {
				wnode->next = openweathermap_data;
				openweathermap_data = wnode;
			} else {
				sfree((void *)&wnode);
			}
			jchild = jchild->next;
		}
	}
	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
	}
	json_delete(json);

	pthread_cleanup_push(openweathermapParseCleanUp, (void *)openweathermap_data);
	
	while(openweathermap_loop) {
		wtmp = openweathermap_data;
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
			while(wtmp) {
				filename = NULL;
				data = NULL;
				sprintf(url, "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493", wtmp->location, wtmp->country);
				http_parse_url(url, &filename);
				ret = http_get(filename, &data, &lg, typebuf);
				if(ret == 200) {
					if(strcmp(typebuf, "application/json;") == 0) {
						if(json_validate(data) == true) {
							if((jdata = json_decode(data)) != NULL) {
								if((jmain = json_find_member(jdata, "main")) != NULL) {
									if((node = json_find_member(jmain, "temp")) == NULL) {
										printf("api.openweathermap.org json has no temp_c key");
									} else if(json_find_number(jmain, "humidity", &humi) != 0) {
										printf("api.openweathermap.org json has no humidity key");
									} else {
										if(node->tag != JSON_NUMBER) {
											printf("api.openweathermap.org json has no temp key");
										} else {
											temp = node->number_-273.15;

											openweathermap->message = json_mkobject();
											
											JsonNode *code = json_mkobject();
											
											json_append_member(code, "location", json_mkstring(wtmp->location));
											json_append_member(code, "country", json_mkstring(wtmp->country));
											json_append_member(code, "temperature", json_mknumber((int)(temp*100)));
											json_append_member(code, "humidity", json_mknumber((int)(humi*100)));
											
											json_append_member(openweathermap->message, "code", code);
											json_append_member(openweathermap->message, "origin", json_mkstring("receiver"));
											json_append_member(openweathermap->message, "protocol", json_mkstring(openweathermap->id));
											
											pilight.broadcast(openweathermap->id, openweathermap->message);
											json_delete(openweathermap->message);
											openweathermap->message = NULL;
										}
									}
								} else {
									logprintf(LOG_NOTICE, "api.openweathermap.org json has no current_observation key");
								}
								json_delete(jdata);
							} else {
								logprintf(LOG_NOTICE, "api.openweathermap.org json could not be parsed");
							}
						}  else {
							logprintf(LOG_NOTICE, "api.openweathermap.org response was not in a valid json format");
						}
					} else {
						logprintf(LOG_NOTICE, "api.openweathermap.org response was not in a valid json format");
					}
				} else {
					logprintf(LOG_NOTICE, "could not reach api.openweathermap.org");
				}
				if(data) {
					sfree((void *)&data);
				}
				if(filename) {
					sfree((void *)&filename);
				}
			wtmp = wtmp->next;
			}
		}
		pthread_mutex_unlock(&mutex);
	}
	
	pthread_cleanup_pop(1);

	return (void *)NULL;
}

void openweathermapInitDev(JsonNode *jdevice) {
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("openweathermap", &openweathermapParse, (void *)json);
	sfree((void *)&output);
}

int openweathermapCheckValues(JsonNode *code) {
	int interval = 600;
	protocol_setting_get_number(openweathermap, "interval", &interval);

	if(interval < 600) {
		return 1;
	}

	return 0;
}

void openweathermapInit(void) {

	protocol_register(&openweathermap);
	protocol_set_id(openweathermap, "openweathermap");
	protocol_device_add(openweathermap, "openweathermap", "Open Weather Map API");
	openweathermap->devtype = WEATHER;
	openweathermap->hwtype = API;

	options_add(&openweathermap->options, 't', "temperature", has_value, config_value, "^[0-9]{1,5}$");
	options_add(&openweathermap->options, 'h', "humidity", has_value, config_value, "^[0-9]{1,5}$");
	options_add(&openweathermap->options, 'l', "location", has_value, config_id, "^[a-z]+$");
	options_add(&openweathermap->options, 'c', "country", has_value, config_id, "^[a-z]+$");

	protocol_setting_add_number(openweathermap, "decimals", 2);
	protocol_setting_add_number(openweathermap, "humidity", 1);
	protocol_setting_add_number(openweathermap, "temperature", 1);
	protocol_setting_add_number(openweathermap, "battery", 0);
	protocol_setting_add_number(openweathermap, "interval", 600);

	openweathermap->initDev=&openweathermapInitDev;
	openweathermap->checkValues=&openweathermapCheckValues;
}
