/*
	Copyright (C) 2014 1000io

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
#include "generic_webcam.h"

typedef struct generic_webcam_data_t {
	char *url;
	struct generic_webcam_data_t *next;
} generic_webcam_data_t;

unsigned short generic_webcam_loop = 1;
unsigned short generic_webcam_nrfree = 0;

void generic_webcamParseCleanUp(void *arg) {
	struct generic_webcam_data_t *generic_webcam_data = (struct generic_webcam_data_t *)arg;
	struct generic_webcam_data_t *wtmp = NULL;

	generic_webcam_loop = 0;

	while(generic_webcam_data) {
		wtmp = generic_webcam_data;
		sfree((void *)&generic_webcam_data->url);		
		generic_webcam_data = generic_webcam_data->next;
		sfree((void *)&wtmp);
	}
	sfree((void *)&generic_webcam_data);
}

void *generic_webcamParse(void *param) {

/*	
	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *node = NULL;
	struct generic_webcam_data_t *generic_webcam_data = NULL;
	struct generic_webcam_data_t *wtmp = NULL;
	struct timeval tp;
	struct timespec ts;		
	int interval = 900;

	char url[1024];
	char *filename = NULL, *data = NULL;
	char typebuf[70];
	char *stmp = NULL;
	double temp = 0;
	int humi = 0, rc = 0, lg = 0, ret = 0;
	int firstrun = 1;	
	JsonNode *jdata = NULL;
	JsonNode *jobs = NULL;	

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

	int has_country = 0, has_api = 0, has_location = 0;
	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			has_country = 0, has_api = 0, has_location = 0;
			jchild1 = json_first_child(jchild);
			struct generic_webcam_data_t* wnode = malloc(sizeof(struct generic_webcam_data_t));
			while(jchild1) {
				if(strcmp(jchild1->key, "api") == 0) {
					has_api = 1;
					wnode->api = malloc(strlen(jchild1->string_)+1);
					strcpy(wnode->api, jchild1->string_);
				}
				if(strcmp(jchild1->key, "location") == 0) {
					has_location = 1;
					wnode->location = malloc(strlen(jchild1->string_)+1);
					strcpy(wnode->location, jchild1->string_);
				}
				if(strcmp(jchild1->key, "country") == 0) {
					has_country = 1;
					wnode->country = malloc(strlen(jchild1->string_)+1);
					strcpy(wnode->country, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			if(has_country && has_api && has_location) {
				wnode->next = generic_webcam_data;
				generic_webcam_data = wnode;
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

	pthread_cleanup_push(generic_webcamParseCleanUp, (void *)generic_webcam_data);

	while(generic_webcam_loop) {
		wtmp = generic_webcam_data;
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
				sprintf(url, "http://api.generic_webcam.com/api/%s/geolookup/conditions/q/%s/%s.json", wtmp->api, wtmp->country, wtmp->location);	
				http_parse_url(url, &filename);
				ret = http_get(filename, &data, &lg, typebuf);

				if(ret == 200) {
					if(strcmp(typebuf, "application/json;") == 0) {
						if(json_validate(data) == true) {
							if((jdata = json_decode(data)) != NULL) {
								if((jobs = json_find_member(jdata, "current_observation")) != NULL) {
									if((node = json_find_member(jobs, "temp_c")) == NULL) {
										printf("api.generic_webcam.com json has no temp_c key");
									} else if(json_find_string(jobs, "relative_humidity", &stmp) != 0) {
										printf("api.generic_webcam.com json has no relative_humidity key");
									} else {
										if(node->tag != JSON_NUMBER) {
											printf("api.generic_webcam.com json has no temp_c key");
										} else {
											temp = node->number_;
											sscanf(stmp, "%d%%", &humi);
											generic_webcam->message = json_mkobject();

											JsonNode *code = json_mkobject();

											json_append_member(code, "api", json_mkstring(wtmp->api));
											json_append_member(code, "location", json_mkstring(wtmp->location));
											json_append_member(code, "country", json_mkstring(wtmp->country));
											json_append_member(code, "temperature", json_mknumber((int)(temp*100)));
											json_append_member(code, "humidity", json_mknumber((int)(humi*100)));

											json_append_member(generic_webcam->message, "code", code);
											json_append_member(generic_webcam->message, "origin", json_mkstring("receiver"));
											json_append_member(generic_webcam->message, "protocol", json_mkstring(generic_webcam->id));

											pilight.broadcast(generic_webcam->id, generic_webcam->message);
											json_delete(generic_webcam->message);
											generic_webcam->message = NULL;
										}
									}
								} else {
									logprintf(LOG_NOTICE, "api.generic_webcam.com json has no current_observation key");
								}
								json_delete(jdata);
							} else {
								logprintf(LOG_NOTICE, "api.generic_webcam.com json could not be parsed");
							}
						}  else {
							logprintf(LOG_NOTICE, "api.generic_webcam.com response was not in a valid json format");
						}
					} else {
						logprintf(LOG_NOTICE, "api.generic_webcam.com response was not in a valid json format");
					}
				} else {
					logprintf(LOG_NOTICE, "could not reach api.wundergrond.com");
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
	*/
	return (void *)NULL;
	
}

void generic_webcamInitDev(JsonNode *jdevice) {
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("generic_webcam", &generic_webcamParse, (void *)json);
	sfree((void *)&output);
}

int generic_webcamCheckValues(JsonNode *code) {
	int interval = 900;
	protocol_setting_get_number(generic_webcam, "interval", &interval);

	if(interval < 900) {
		return 1;
	}

	return 0;
}

void generic_webcamInit(void) {

	protocol_register(&generic_webcam);
	protocol_set_id(generic_webcam, "generic_webcam");
	protocol_device_add(generic_webcam, "generic_webcam", "Generic Webcam");
	generic_webcam->devtype = WEBCAM;
	generic_webcam->hwtype = API;

	options_add(&generic_webcam->options, 'u', "url", has_value, config_id, "^[a-z0-9]+$");
	protocol_setting_add_number(generic_webcam, "interval", 900);

	generic_webcam->initDev=&generic_webcamInitDev;
	generic_webcam->checkValues=&generic_webcamCheckValues;
}
