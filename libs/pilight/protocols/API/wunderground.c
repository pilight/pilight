/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

#include "../../core/threadpool.h"
#include "../../core/eventpool.h"
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
#include "wunderground.h"

#define INTERVAL	900

typedef struct data_t {
	char *country;
	char *location;
	char *key;
	char *api;

	double temp;
	int humi;

	int interval;
	double temp_offset;

	unsigned long sunrisesetid;
	unsigned long updateid;
	unsigned long initid;

	time_t update;
	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *thread(void *param);

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void callback2(int code, char *data, int size, char *type, void *userdata) {
	struct data_t *settings = userdata;
	struct JsonNode *jdata = NULL;
	struct JsonNode *jsun = NULL;
	struct JsonNode *jsunr = NULL;
	struct JsonNode *jsuns = NULL;
	char *shour = NULL, *smin = NULL;
	char *rhour = NULL, *rmin = NULL;
	struct tm tm;
	struct timeval tv;

	memset(&tm, 0, sizeof(struct tm));

	if(code == 200) {
		if(strstr(type, "application/json") != NULL) {
			if(json_validate(data) == true) {
				if((jdata = json_decode(data)) != NULL) {
					if((jsun = json_find_member(jdata, "sun_phase")) != NULL) {
						if((jsunr = json_find_member(jsun, "sunrise")) != NULL
							 && (jsuns = json_find_member(jsun, "sunset")) != NULL) {
							if(json_find_string(jsuns, "hour", &shour) != 0) {
								logprintf(LOG_NOTICE, "api.wunderground.com json has no sunset hour key");
							} else if(json_find_string(jsuns, "minute", &smin) != 0) {
								logprintf(LOG_NOTICE, "api.wunderground.com json has no sunset minute key");
							} else if(json_find_string(jsunr, "hour", &rhour) != 0) {
								logprintf(LOG_NOTICE, "api.wunderground.com json has no sunrise hour key");
							} else if(json_find_string(jsunr, "minute", &rmin) != 0) {
								logprintf(LOG_NOTICE, "api.wunderground.com json has no sunrise minute key");
							} else {
								time_t timenow = time(NULL);
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
								char *_sun = NULL;

								time_t midnight = (datetime2ts(year, month, mday, 23, 59, 59)+1);
								time_t sunset = 0;
								time_t sunrise = 0;

								if(timenow > sunrise && timenow < sunset) {
									_sun = "rise";
								} else {
									_sun = "set";
								}

								struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
								if(data == NULL) {
									OUT_OF_MEMORY
								}
								snprintf(data->message, 1024,
									"{\"api\":\"%s\",\"location\":\"%s\",\"country\":\"%s\",\"temperature\":%.2f,\"humidity\":%d,\"update\":0,\"sunrise\":%.2f,\"sunset\":%.2f,\"sun\":\"%s\"}",
									settings->api, settings->location, settings->country, settings->temp, settings->humi,
									((double)((atoi(rhour)*100)+atoi(rmin))/100), ((double)((atoi(shour)*100)+atoi(smin))/100), _sun
								);
								strncpy(data->origin, "receiver", 255);
								data->protocol = wunderground->id;
								if(strlen(pilight_uuid) > 0) {
									data->uuid = pilight_uuid;
								} else {
									data->uuid = NULL;
								}
								data->repeat = 1;
								eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

								/* Send message when sun rises */
								if(sunrise > timenow) {
									settings->interval = (int)(sunrise-timenow);
								/* Send message when sun sets */
								} else if(sunset > timenow) {
									settings->interval = (int)(sunset-timenow);
								/* Update all values when a new day arrives */
								} else {
									settings->interval = (int)(midnight-timenow);
								}

								settings->update = time(NULL);
								tv.tv_sec = settings->interval;
								tv.tv_usec = 0;
								threadpool_add_scheduled_work(settings->key, thread, tv, (void *)settings);
								tv.tv_sec = INTERVAL;
								threadpool_add_scheduled_work(settings->key, thread, tv, (void *)settings);
								tv.tv_sec = 86400;
								threadpool_add_scheduled_work(settings->key, thread, tv, (void *)settings);

								settings->update = time(NULL);
							}
						} else {
							logprintf(LOG_NOTICE, "api.wunderground.com json has no sunset and/or sunrise key");
						}
					} else {
						logprintf(LOG_NOTICE, "api.wunderground.com json has no sun_phase key");
					}
					json_delete(jdata);
				} else {
					logprintf(LOG_NOTICE, "api.wunderground.com json could not be parsed");
				}
			}  else {
				logprintf(LOG_NOTICE, "api.wunderground.com response was not in a valid json format");
			}
		} else {
			logprintf(LOG_NOTICE, "api.wunderground.com response was not in a valid json format");
		}
	} else {
		logprintf(LOG_NOTICE, "could not reach api.wunderground.com");
	}
	return;
}

static void callback1(int code, char *data, int size, char *type, void *userdata) {
	struct data_t *settings = userdata;
	struct JsonNode *jdata = NULL;
	struct JsonNode *jobs = NULL;
	struct JsonNode *node = NULL;
	char *stmp = NULL;
	char url[1024];

	if(code == 200) {
		if(strstr(type, "application/json") != NULL) {
			if(json_validate(data) == true) {
				if((jdata = json_decode(data)) != NULL) {
					if((jobs = json_find_member(jdata, "current_observation")) != NULL) {
						if((node = json_find_member(jobs, "temp_c")) == NULL) {
							logprintf(LOG_NOTICE, "api.wunderground.com json has no temp_c key");
						} else if(json_find_string(jobs, "relative_humidity", &stmp) != 0) {
							logprintf(LOG_NOTICE, "api.wunderground.com json has no relative_humidity key");
						} else {
							if(node->tag != JSON_NUMBER) {
								logprintf(LOG_NOTICE, "api.wunderground.com json has no temp_c key");
							} else {
								settings->temp = node->number_;
								sscanf(stmp, "%d%%", &settings->humi);

								memset(url, '\0', 1024);
								sprintf(url, "http://api.wunderground.com/api/%s/astronomy/q/%s/%s.json", settings->api, settings->country, settings->location);

								http_get_content(url, callback2, userdata);
							}
						}
					} else {
						logprintf(LOG_NOTICE, "api.wunderground.com json has no current_observation key");
					}
					json_delete(jdata);
				} else {
					logprintf(LOG_NOTICE, "api.wunderground.com json could not be parsed");
				}
			} else {
				logprintf(LOG_NOTICE, "api.wunderground.com response was not in a valid json format");
			}
		} else {
			logprintf(LOG_NOTICE, "api.wunderground.com response was not in a valid json format");
		}
	} else {
		logprintf(LOG_NOTICE, "could not reach api.wunderground.com");
	}
	return;
}

static void *update(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;

	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	snprintf(data->message, 1024,
		"{\"api\":\"%s\",\"location\":\"%s\",\"country\":\"%s\",\"updae\":1}",
		settings->api, settings->location, settings->country
	);
	strncpy(data->origin, "receiver", 255);
	data->protocol = wunderground->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
	return NULL;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;
	struct timeval tv;
	char url[1024];

	tv.tv_sec = INTERVAL;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(settings->key, thread, tv, (void *)settings);

	memset(url, '\0', 1024);
	sprintf(url, "http://api.wunderground.com/api/%s/geolookup/conditions/q/%s/%s.json", settings->api, settings->country, settings->location);

	http_get_content(url, callback1, task->userdata);
	return (void *)NULL;
}

static void *addDevice(int reason, void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	double itmp = 0;
	int match = 0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if(!(wunderground->masterOnly == 0 || pilight.runmode == STANDALONE)) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(wunderground->id, jchild->string_) == 0) {
				match = 1;
				break;
		}
			jchild = jchild->next;
		}
	}

	if(match == 0) {
		return NULL;
	}

	if((node = MALLOC(sizeof(struct data_t)))== NULL) {
		OUT_OF_MEMORY
	}
	memset(node, '\0', sizeof(struct data_t));

	node->interval = INTERVAL;
	node->country = NULL;
	node->location = NULL;
	node->update = 0;
	node->temp_offset = 0;

	int has_country = 0, has_location = 0, has_api = 0;
	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			has_country = 0, has_location = 0;
			jchild1 = json_first_child(jchild);

			while(jchild1) {
				if(strcmp(jchild1->key, "api") == 0) {
					has_api = 1;
					if((node->api = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(node->api, jchild1->string_);
				}
				if(strcmp(jchild1->key, "location") == 0) {
					has_location = 1;
					if((node->location = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(node->location, jchild1->string_);
				}
				if(strcmp(jchild1->key, "country") == 0) {
					has_country = 1;
					if((node->country = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(node->country, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			if(has_country == 1 && has_location == 1 && has_api == 1) {
				node->next = data;
				data = node;
			} else {
				if(has_country == 1) {
					FREE(node->country);
				}
				if(has_location == 1) {
					FREE(node->location);
				}
				FREE(node);
				node = NULL;
			}
			jchild = jchild->next;
		}
	}

	if(node == NULL) {
		return NULL;
	}
	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		node->interval = (int)round(itmp);
	json_find_number(jdevice, "temperature-offset", &node->temp_offset);

	if((node->key = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->key, jdevice->key);
	node->temp = 0.0;
	node->humi = 0;

	tv.tv_sec = INTERVAL;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, update, tv, (void *)node);
	threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);
	tv.tv_sec = 1;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);

	return NULL;
}

static int checkValues(struct JsonNode *code) {
	double interval = INTERVAL;

	json_find_number(code, "poll-interval", &interval);

	if((int)round(interval) < INTERVAL) {
		logprintf(LOG_ERR, "wunderground poll-interval cannot be lower than %d", INTERVAL);
		return 1;
	}

	return 0;
}

static int createCode(struct JsonNode *code, char *message) {
	struct data_t *tmp = data;
	char *country = NULL;
	char *location = NULL;
	char *api = NULL;
	double itmp = 0;

	if(json_find_number(code, "min-interval", &itmp) == 0) {
		logprintf(LOG_ERR, "you can't override the min-interval setting");
		return EXIT_FAILURE;
	}

	if(json_find_string(code, "country", &country) == 0 &&
	   json_find_string(code, "location", &location) == 0 &&
	   json_find_string(code, "api", &api) == 0 &&
	   json_find_number(code, "update", &itmp) == 0) {

		while(tmp) {
			if(strcmp(tmp->country, country) == 0
			   && strcmp(tmp->location, location) == 0
			   && strcmp(tmp->api, api) == 0) {
				time_t currenttime = time(NULL);
				if((currenttime-tmp->update) > INTERVAL) {
					threadpool_add_work(REASON_END, NULL, tmp->key, 0, thread, NULL, (void *)tmp);
					tmp->update = time(NULL);
				}
			}
			tmp = tmp->next;
		}
	} else {
		logprintf(LOG_ERR, "wunderground: insufficient number of arguments");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->api);
		FREE(tmp->country);
		FREE(tmp->location);
		FREE(tmp->key);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}

static void printHelp(void) {
	printf("\t -c --country=country\t\tupdate an entry with this country\n");
	printf("\t -l --location=location\t\tupdate an entry with this location\n");
	printf("\t -a --api=api\t\t\tupdate an entry with this api code\n");
	printf("\t -u --update\t\t\tupdate the defined weather entry\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void wundergroundInit(void) {
	protocol_register(&wunderground);
	protocol_set_id(wunderground, "wunderground");
	protocol_device_add(wunderground, "wunderground", "Weather Underground API");
	wunderground->devtype = WEATHER;
	wunderground->hwtype = API;
	wunderground->multipleId = 0;
	wunderground->masterOnly = 1;

	options_add(&wunderground->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&wunderground->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&wunderground->options, 'a', "api", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[a-z0-9]+$");
	options_add(&wunderground->options, 'l', "location", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[a-z_]+$");
	options_add(&wunderground->options, 'c', "country", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[a-z]+$");
	options_add(&wunderground->options, 'x', "sunrise", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&wunderground->options, 'y', "sunset", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&wunderground->options, 's', "sun", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&wunderground->options, 'u', "update", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	// options_add(&wunderground->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&wunderground->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&wunderground->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&wunderground->options, 0, "sunriseset-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&wunderground->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&wunderground->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&wunderground->options, 0, "show-sunriseset", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&wunderground->options, 0, "show-update", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&wunderground->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)86400, "[0-9]");

	wunderground->createCode=&createCode;
	// wunderground->initDev=&initDev;
	wunderground->checkValues=&checkValues;
	wunderground->gc=&gc;
	wunderground->printHelp=&printHelp;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "wunderground";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	wundergroundInit();
}
#endif
