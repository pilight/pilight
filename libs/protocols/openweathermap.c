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
#include <pthread.h>
#include <sys/stat.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "../pilight/datetime.h" // Full path because we also have a datetime protocol
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
	protocol_threads_t *thread;
	time_t update;
	struct openweathermap_data_t *next;
} openweathermap_data_t;

static pthread_mutex_t openweathermaplock;
static pthread_mutexattr_t openweathermapattr;

static struct openweathermap_data_t *openweathermap_data;
static unsigned short openweathermap_loop = 1;
static unsigned short openweathermap_threads = 0;

static void *openweathermapParse(void *param) {
	struct protocol_threads_t *thread = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)thread->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct JsonNode *node = NULL;
	struct JsonNode *jdata = NULL;
	struct JsonNode *jmain = NULL;
	struct JsonNode *jsys = NULL;
	struct openweathermap_data_t *wnode = malloc(sizeof(struct openweathermap_data_t));

	int interval = 86400, nrloops = 0, ointerval = 86400;
	double itmp = 0;

	char url[1024];
	char *filename = NULL, *data = NULL;
	char typebuf[70];
	double temp = 0, sunrise = 0, sunset = 0, humi = 0;
	int lg = 0, ret = 0;

	time_t timenow = 0;
	struct tm *tm;

	openweathermap_threads++;

	if(!wnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	int has_country = 0, has_location = 0;
	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			has_country = 0, has_location = 0;
			jchild1 = json_first_child(jchild);

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
			if(has_country == 1 && has_location == 1) {
				wnode->thread = thread;
				wnode->next = openweathermap_data;
				openweathermap_data = wnode;
			} else {
				if(has_country == 1) {
					sfree((void *)&wnode->country);
				}
				if(has_location == 1) {
					sfree((void *)&wnode->location);
				}
				sfree((void *)&wnode);
				wnode = NULL;
			}
			jchild = jchild->next;
		}
	}

	if(!wnode) {
		return 0;
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	ointerval = interval;

	while(openweathermap_loop) {
		protocol_thread_wait(thread, interval, &nrloops);
		pthread_mutex_lock(&openweathermaplock);
		if(openweathermap_loop == 0) {
			break;
		}
		interval = ointerval;
		filename = NULL;
		data = NULL;
		sprintf(url, "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493", wnode->location, wnode->country);
		http_parse_url(url, &filename);
		ret = http_get(filename, &data, &lg, typebuf);
		if(ret == 200) {
			if(strcmp(typebuf, "application/json;") == 0) {
				if(json_validate(data) == true) {
					if((jdata = json_decode(data)) != NULL) {
						if((jmain = json_find_member(jdata, "main")) != NULL
						   && (jsys = json_find_member(jdata, "sys")) != NULL) {
							if((node = json_find_member(jmain, "temp")) == NULL) {
								printf("api.openweathermap.org json has no temp key");
							} else if(json_find_number(jmain, "humidity", &humi) != 0) {
								printf("api.openweathermap.org json has no humidity key");
							} else if(json_find_number(jsys, "sunrise", &sunrise) != 0) {
								printf("api.openweathermap.org json has no sunrise key");
							} else if(json_find_number(jsys, "sunset", &sunset) != 0) {
								printf("api.openweathermap.org json has no sunset key");
							} else {
								if(node->tag != JSON_NUMBER) {
									printf("api.openweathermap.org json has no temp key");
								} else {
									temp = node->number_-273.15;

									timenow = time(NULL);
									struct tm *current = localtime(&timenow);
									int month = current->tm_mon+1;
									int mday = current->tm_mday;
									int year = current->tm_year+1900;

									time_t midnight = (datetime2ts(year, month, mday, 23, 59, 59, 0)+1);

									openweathermap->message = json_mkobject();

									JsonNode *code = json_mkobject();

									json_append_member(code, "location", json_mkstring(wnode->location));
									json_append_member(code, "country", json_mkstring(wnode->country));
									json_append_member(code, "temperature", json_mknumber((int)(round(temp)*100)));
									json_append_member(code, "humidity", json_mknumber((int)(round(humi)*100)));
									time_t a = (time_t)sunrise;
									tm = localtime(&a);
									json_append_member(code, "sunrise", json_mknumber((tm->tm_hour*100)+tm->tm_min));
									time_t b = (time_t)sunset;
									tm = localtime(&b);
									json_append_member(code, "sunset", json_mknumber((tm->tm_hour*100)+tm->tm_min));
									if(timenow > (int)round(sunrise) && timenow < (int)round(sunset)) {
										json_append_member(code, "sun", json_mkstring("rise"));
									} else {
										json_append_member(code, "sun", json_mkstring("set"));
									}

									json_append_member(openweathermap->message, "message", code);
									json_append_member(openweathermap->message, "origin", json_mkstring("receiver"));
									json_append_member(openweathermap->message, "protocol", json_mkstring(openweathermap->id));

									pilight.broadcast(openweathermap->id, openweathermap->message);
									json_delete(openweathermap->message);
									openweathermap->message = NULL;

									/* Send message when sun rises */
									if((int)round(sunrise) > timenow) {
										if(((int)round(sunrise)-timenow) < ointerval) {
											interval = (int)((int)round(sunrise)-timenow);
										}
									/* Send message when sun sets */
									} else if((int)round(sunset) > timenow) {
										if(((int)round(sunset)-timenow) < ointerval) {
											interval = (int)((int)round(sunset)-timenow);
										}
									/* Update all values when a new day arrives */
									} else {
										if((midnight-timenow) < ointerval) {
											interval = (int)(midnight-timenow);
										}
									}

									wnode->update = time(NULL);
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
		pthread_mutex_unlock(&openweathermaplock);
	}

	struct openweathermap_data_t *wtmp = NULL;
	while(openweathermap_data) {
		wtmp = openweathermap_data;
		sfree((void *)&openweathermap_data->country);
		sfree((void *)&openweathermap_data->location);
		openweathermap_data = openweathermap_data->next;
		sfree((void *)&wtmp);
	}
	sfree((void *)&openweathermap_data);
	openweathermap_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *openweathermapInitDev(JsonNode *jdevice) {
	openweathermap_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(openweathermap, json);
	return threads_register("openweathermap", &openweathermapParse, (void *)node, 0);
}

static int openweathermapCheckValues(JsonNode *code) {
	double interval = 600;

	json_find_number(code, "poll-interval", &interval);

	if((int)round(interval) < 600) {
		return 1;
	}

	return 0;
}

static int openweathermapCreateCode(JsonNode *code) {
	struct openweathermap_data_t *wtmp = openweathermap_data;
	char *country = NULL;
	char *location = NULL;
	double itmp = 0;
	time_t currenttime = 0;

	if(json_find_number(code, "min-interval", &itmp) == 0) {
		logprintf(LOG_ERR, "you can't override the min-interval setting");
		return EXIT_FAILURE;
	}

	if(json_find_string(code, "country", &country) == 0 &&
	   json_find_string(code, "location", &location) == 0 &&
	   json_find_number(code, "update", &itmp) == 0) {
		currenttime = time(NULL);
		while(wtmp) {
			if(strcmp(wtmp->country, country) == 0
			   && strcmp(wtmp->location, location) == 0) {
				if((currenttime-wtmp->update) > 600) {
					pthread_mutex_unlock(&wtmp->thread->mutex);
					pthread_cond_signal(&wtmp->thread->cond);
					wtmp->update = time(NULL);
				}
			}
			wtmp = wtmp->next;
		}
	} else {
		logprintf(LOG_ERR, "openweathermap: insufficient number of arguments");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static void openweathermapThreadGC(void) {
	openweathermap_loop = 0;
	protocol_thread_stop(openweathermap);
	while(openweathermap_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(openweathermap);
}

static void openweathermapPrintHelp(void) {
	printf("\t -c --country=country\t\tupdate an entry with this country\n");
	printf("\t -l --location=location\t\tupdate an entry with this location\n");
	printf("\t -u --update\t\t\tupdate the defined weather entry\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void openweathermapInit(void) {
	pthread_mutexattr_init(&openweathermapattr);
	pthread_mutexattr_settype(&openweathermapattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&openweathermaplock, &openweathermapattr);

	protocol_register(&openweathermap);
	protocol_set_id(openweathermap, "openweathermap");
	protocol_device_add(openweathermap, "openweathermap", "Open Weather Map API");
	openweathermap->devtype = WEATHER;
	openweathermap->hwtype = API;
	openweathermap->multipleId = 0;

	options_add(&openweathermap->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&openweathermap->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&openweathermap->options, 'l', "location", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, "^[a-z]+$");
	options_add(&openweathermap->options, 'c', "country", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, "^[a-z]+$");
	options_add(&openweathermap->options, 'x', "sunrise", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&openweathermap->options, 'y', "sunset", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&openweathermap->options, 's', "sun", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_STRING, NULL, NULL);
	options_add(&openweathermap->options, 'u', "update", OPTION_NO_VALUE, CONFIG_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&openweathermap->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&openweathermap->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&openweathermap->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "gui-show-sunriseset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "gui-show-update", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "poll-interval", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)86400, "[0-9]");
	options_add(&openweathermap->options, 0, "min-interval", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)600, "[0-9]");

	openweathermap->createCode=&openweathermapCreateCode;
	openweathermap->initDev=&openweathermapInitDev;
	openweathermap->checkValues=&openweathermapCheckValues;
	openweathermap->threadGC=&openweathermapThreadGC;
	openweathermap->printHelp=&openweathermapPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "openweathermap";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	openweathermapInit();
}
#endif
