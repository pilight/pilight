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
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../core/threads.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/datetime.h" // Full path because we also have a datetime protocol
#include "../../core/log.h"
#include "../../core/http.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "openweathermap.h"

#define INTERVAL 	600

typedef struct settings_t {
	char *country;
	char *location;
	protocol_threads_t *thread;
	time_t update;
	struct settings_t *next;
} settings_t;

static pthread_mutex_t lock;
static pthread_mutexattr_t attr;

static struct settings_t *settings;
static unsigned short loop = 1;
static unsigned short threads = 0;

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
	struct settings_t *wnode = MALLOC(sizeof(struct settings_t));

	int interval = 86400, ointerval = 86400, event = 0;
	int firstrun = 1, nrloops = 0, timeout = 0;
	double itmp = 0;

	char url[1024];
	char *data = NULL;
	char typebuf[255], *tp = typebuf;
	double temp = 0, sunrise = 0, sunset = 0, humi = 0;
	int ret = 0, size = 0;

	time_t timenow = 0;
	struct tm tm;

	threads++;

	memset(&typebuf, '\0', 255);

	if(wnode == NULL) {
		fprintf(stderr, "out of memory\n");
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
					if((wnode->location = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						fprintf(stderr, "out of memory\n");
						exit(EXIT_FAILURE);
					}
					strcpy(wnode->location, jchild1->string_);
				}
				if(strcmp(jchild1->key, "country") == 0) {
					has_country = 1;
					if((wnode->country = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						fprintf(stderr, "out of memory\n");
						exit(EXIT_FAILURE);
					}
					strcpy(wnode->country, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			if(has_country == 1 && has_location == 1) {
				wnode->thread = thread;
				wnode->next = settings;
				settings = wnode;
			} else {
				if(has_country == 1) {
					FREE(wnode->country);
				}
				if(has_location == 1) {
					FREE(wnode->location);
				}
				FREE(wnode);
				wnode = NULL;
			}
			jchild = jchild->next;
		}
	}

	if(wnode == NULL) {
		return 0;
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	ointerval = interval;

	while(loop) {
		event = protocol_thread_wait(thread, INTERVAL, &nrloops);
		pthread_mutex_lock(&lock);
		if(loop == 0) {
			break;
		}
		timeout += INTERVAL;
		if(timeout >= interval || event != ETIMEDOUT || firstrun == 1) {
			timeout = 0;
			interval = ointerval;

			data = NULL;
			sprintf(url, "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&APPID=8db24c4ac56251371c7ea87fd3115493", wnode->location, wnode->country);
			data = http_get_content(url, &tp, &ret, &size);
			if(ret == 200) {
				if(strstr(typebuf, "application/json") != NULL) {
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
										struct tm current;
										memset(&current, '\0', sizeof(struct tm));
#ifdef _WIN32
										localtime(&timenow);
#else
										localtime_r(&timenow, &current);
#endif

										int month = current.tm_mon+1;
										int mday = current.tm_mday;
										int year = current.tm_year+1900;

										time_t midnight = (datetime2ts(year, month, mday, 23, 59, 59, 0)+1);

										openweathermap->message = json_mkobject();

										JsonNode *code = json_mkobject();

										json_append_member(code, "location", json_mkstring(wnode->location));
										json_append_member(code, "country", json_mkstring(wnode->country));
										json_append_member(code, "temperature", json_mknumber(temp, 2));
										json_append_member(code, "humidity", json_mknumber(humi, 2));
										json_append_member(code, "update", json_mknumber(0, 0));

										time_t a = (time_t)sunrise;
										memset(&tm, '\0', sizeof(struct tm));
#ifdef _WIN32
										localtime(&a);
#else
										localtime_r(&a, &tm);
#endif
										json_append_member(code, "sunrise", json_mknumber((double)((tm.tm_hour*100)+tm.tm_min)/100, 2));

										a = (time_t)sunset;
										memset(&tm, '\0', sizeof(struct tm));
#ifdef _WIN32
										localtime(&a);
#else
										localtime_r(&a, &tm);
#endif
										json_append_member(code, "sunset", json_mknumber((double)((tm.tm_hour*100)+tm.tm_min)/100, 2));
										if(timenow > (int)round(sunrise) && timenow < (int)round(sunset)) {
											json_append_member(code, "sun", json_mkstring("rise"));
										} else {
											json_append_member(code, "sun", json_mkstring("set"));
										}

										json_append_member(openweathermap->message, "message", code);
										json_append_member(openweathermap->message, "origin", json_mkstring("receiver"));
										json_append_member(openweathermap->message, "protocol", json_mkstring(openweathermap->id));

										if(pilight.broadcast != NULL) {
											pilight.broadcast(openweathermap->id, openweathermap->message, PROTOCOL);
										}
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
				FREE(data);
			}
		} else {
			openweathermap->message = json_mkobject();
			JsonNode *code = json_mkobject();
			json_append_member(code, "location", json_mkstring(wnode->location));
			json_append_member(code, "country", json_mkstring(wnode->country));
			json_append_member(code, "update", json_mknumber(1, 0));
			json_append_member(openweathermap->message, "message", code);
			json_append_member(openweathermap->message, "origin", json_mkstring("receiver"));
			json_append_member(openweathermap->message, "protocol", json_mkstring(openweathermap->id));
			if(pilight.broadcast != NULL) {
				pilight.broadcast(openweathermap->id, openweathermap->message, PROTOCOL);
			}
			json_delete(openweathermap->message);
			openweathermap->message = NULL;
		}
		firstrun = 0;

		pthread_mutex_unlock(&lock);
	}
	pthread_mutex_unlock(&lock);

	threads--;
	return (void *)NULL;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	json_free(output);

	struct protocol_threads_t *node = protocol_thread_init(openweathermap, json);
	return threads_register("openweathermap", &openweathermapParse, (void *)node, 0);
}

static int checkValues(JsonNode *code) {
	double interval = INTERVAL;

	json_find_number(code, "poll-interval", &interval);

	if((int)round(interval) < INTERVAL) {
		logprintf(LOG_ERR, "openweathermap poll-interval cannot be lower than %d", INTERVAL);
		return 1;
	}

	return 0;
}

static int createCode(JsonNode *code) {
	struct settings_t *wtmp = settings;
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
				if((currenttime-wtmp->update) > INTERVAL) {
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

static void threadGC(void) {
	loop = 0;
	protocol_thread_stop(openweathermap);
	while(threads > 0) {
		usleep(10);
	}
	protocol_thread_free(openweathermap);

	struct settings_t *wtmp = NULL;
	while(settings) {
		wtmp = settings;
		FREE(settings->country);
		FREE(settings->location);
		settings = settings->next;
		FREE(wtmp);
	}
	if(settings != NULL) {
		FREE(settings);
	}
}

static void printHelp(void) {
	printf("\t -c --country=country\t\tupdate an entry with this country\n");
	printf("\t -l --location=location\t\tupdate an entry with this location\n");
	printf("\t -u --update\t\t\tupdate the defined weather entry\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void openweathermapInit(void) {
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &attr);

	protocol_register(&openweathermap);
	protocol_set_id(openweathermap, "openweathermap");
	protocol_device_add(openweathermap, "openweathermap", "Open Weather Map API");
	openweathermap->devtype = WEATHER;
	openweathermap->hwtype = API;
	openweathermap->multipleId = 0;
	openweathermap->masterOnly = 1;

	options_add(&openweathermap->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&openweathermap->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&openweathermap->options, 'l', "location", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^([a-zA-Z-]|[[:space:]])+$");
	options_add(&openweathermap->options, 'c', "country", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[a-zA-Z]+$");
	options_add(&openweathermap->options, 'x', "sunrise", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&openweathermap->options, 'y', "sunset", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&openweathermap->options, 's', "sun", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&openweathermap->options, 'u', "update", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	// options_add(&openweathermap->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&openweathermap->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&openweathermap->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&openweathermap->options, 0, "sunriseset-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&openweathermap->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "show-sunriseset", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "show-update", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&openweathermap->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)86400, "[0-9]");

	openweathermap->createCode=&createCode;
	openweathermap->initDev=&initDev;
	openweathermap->checkValues=&checkValues;
	openweathermap->threadGC=&threadGC;
	openweathermap->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "openweathermap";
	module->version = "1.12";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	openweathermapInit();
}
#endif
