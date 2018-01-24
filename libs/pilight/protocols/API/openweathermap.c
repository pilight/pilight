/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <assert.h>
#include <sys/stat.h>
#ifndef _WIN32
	#include <unistd.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/datetime.h" // Full path because we also have a datetime protocol
#include "../../core/log.h"
#include "../../core/http.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "openweathermap.h"

#define INTERVAL	600
static int min_interval = INTERVAL;
static int time_override = -1;
static char url[1024] = "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&APPID=%s";

typedef struct data_t {
	char *name;
	char *country;
	char *location;
	char *api;
	uv_timer_t *enable_timer_req;
	uv_timer_t *update_timer_req;

	int interval;
	double temp_offset;

	time_t update;
	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *update(void *param);
static void *enable(void *param);

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void callback(int code, char *data, int size, char *type, void *userdata) {
	struct JsonNode *jdata = NULL;
	struct JsonNode *jmain = NULL;
	struct JsonNode *jsys = NULL;
	struct JsonNode *node = NULL;
	struct data_t *settings = userdata;
	time_t timenow = 0;
	struct tm tm_rise, tm_set;
	double sunset = 0.0, sunrise = 0.0, temp = 0.0, humi = 0.0;

	memset(&tm_rise, 0, sizeof(struct tm));
	memset(&tm_set, 0, sizeof(struct tm));

	if(code == 200) {
		if(strstr(type, "application/json") != NULL) {
			if(json_validate(data) == true) {
				if((jdata = json_decode(data)) != NULL) {
					if((jmain = json_find_member(jdata, "main")) != NULL
						 && (jsys = json_find_member(jdata, "sys")) != NULL) {
						if((node = json_find_member(jmain, "temp")) == NULL) {
							logprintf(LOG_NOTICE, "api.openweathermap.org json has no temp key");
						} else if(json_find_number(jmain, "humidity", &humi) != 0) {
							logprintf(LOG_NOTICE, "api.openweathermap.org json has no humidity key");
						} else if(json_find_number(jsys, "sunrise", &sunrise) != 0) {
							logprintf(LOG_NOTICE, "api.openweathermap.org json has no sunrise key");
						} else if(json_find_number(jsys, "sunset", &sunset) != 0) {
							logprintf(LOG_NOTICE, "api.openweathermap.org json has no sunset key");
						} else {
							if(node->tag != JSON_NUMBER) {
								logprintf(LOG_NOTICE, "api.openweathermap.org json has no temp key");
							} else {
								temp = node->number_-273.15;

								if(time_override > -1) {
									timenow = time_override;
								} else {
									timenow = time(NULL);
								}

								struct tm current;
								memset(&current, '\0', sizeof(struct tm));
#ifdef _WIN32
								struct tm *tmp = gmtime(&timenow);
								memcpy(&current, tmp, sizeof(struct tm));
#else
								gmtime_r(&timenow, &current);
#endif

								int month = current.tm_mon+1;
								int mday = current.tm_mday;
								int year = current.tm_year+1900;

								time_t midnight = (datetime2ts(year, month, mday, 23, 59, 59)+1);

								char *_sun = NULL;

								time_t a = (time_t)sunrise;
								memset(&tm_rise, '\0', sizeof(struct tm));
#ifdef _WIN32
								tmp = gmtime(&a);
								memcpy(&tm_rise, tmp, sizeof(struct tm));
#else
								gmtime_r(&a, &tm_rise);
#endif
								a = (time_t)sunset;
								memset(&tm_set, '\0', sizeof(struct tm));
#ifdef _WIN32
								tmp = gmtime(&a);
								memcpy(&tm_set, tmp, sizeof(struct tm));
#else
								gmtime_r(&a, &tm_set);
#endif
								if(timenow > (int)round(sunrise) && timenow < (int)round(sunset)) {
									_sun = "rise";
								} else {
									_sun = "set";
								}

								struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
								if(data == NULL) {
									OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
								};
								snprintf(data->message, 1024,
									"{\"api\":\"%s\",\"location\":\"%s\",\"country\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"update\":0,\"sunrise\":%.2f,\"sunset\":%.2f,\"sun\":\"%s\"}",
									settings->api, settings->location, settings->country, temp, humi, ((double)((tm_rise.tm_hour*100)+tm_rise.tm_min)/100), ((double)((tm_set.tm_hour*100)+tm_set.tm_min)/100), _sun
								);
								strncpy(data->origin, "receiver", 255);
								data->protocol = openweathermap->id;
								if(strlen(pilight_uuid) > 0) {
									data->uuid = pilight_uuid;
								} else {
									data->uuid = NULL;
								}
								data->repeat = 1;

								eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

								/* Send message when sun rises */
								if((int)round(sunrise) > timenow) {
									if(((int)round(sunrise)-timenow) < settings->interval) {
										settings->interval = (int)((int)round(sunrise)-timenow);
									}
								/* Send message when sun sets */
								} else if((int)round(sunset) > timenow) {
									if(((int)round(sunset)-timenow) < settings->interval) {
										settings->interval = (int)((int)round(sunset)-timenow);
									}
								/* Update all values when a new day arrives */
								} else {
									if((midnight-timenow) < settings->interval) {
										settings->interval = (int)(midnight-timenow);
									}
								}

								if(time_override > -1) {
									settings->update = time_override;
								} else {
									settings->update = time(NULL);
								}

								/*
								 * Update all values on next event as described above
								 */
								assert(settings->interval > 0);
								uv_timer_start(settings->update_timer_req, (void (*)(uv_timer_t *))update, settings->interval*1000, 0);
								/*
								 * Allow updating the values customly after INTERVAL seconds
								 */
								assert(min_interval > 0);
								uv_timer_start(settings->enable_timer_req, (void (*)(uv_timer_t *))enable, min_interval*1000, 0);
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
	return;
}

static void thread_free(uv_work_t *req, int status) {
	FREE(req);
}

static void thread(uv_work_t *req) {
	struct data_t *settings = req->data;
	char parsed[1024];
	char *enc = urlencode(settings->location);

	memset(parsed, '\0', 1024);
	snprintf(parsed, 1024, url, enc, settings->country, settings->api);
	FREE(enc);

	http_get_content(parsed, callback, settings);
	return;
}

static void *update(void *param) {
	uv_timer_t *timer_req = param;
	struct data_t *settings = timer_req->data;

	uv_work_t *work_req = NULL;
	if((work_req = MALLOC(sizeof(uv_work_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	work_req->data = settings;
	uv_queue_work(uv_default_loop(), work_req, "openweathermap", thread, thread_free);
	if(time_override > -1) {
		settings->update = time_override;
	} else {
		settings->update = time(NULL);
	}
	return NULL;
}

static void *enable(void *param) {
	uv_timer_t *timer_req = param;
	struct data_t *settings = timer_req->data;

	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	};
	snprintf(data->message, 1024,
		"{\"api\":\"%s\",\"location\":\"%s\",\"country\":\"%s\",\"update\":1}",
		settings->api, settings->location, settings->country
	);
	strncpy(data->origin, "receiver", 255);
	data->protocol = openweathermap->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

	return NULL;
}

static void *adaptDevice(int reason, void *param) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jchild = NULL;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if(strcmp(jdevice->key, "openweathermap") != 0) {
		return NULL;
	}

	if((jchild = json_find_member(jdevice, "url")) != NULL) {
		if(jchild->tag == JSON_STRING && strlen(jchild->string_) < 1024) {
			strcpy(url, jchild->string_);
		}
	}

	if((jchild = json_find_member(jdevice, "time-override")) != NULL) {
		if(jchild->tag == JSON_NUMBER) {
			time_override = jchild->number_;
		}
	}

	if((jchild = json_find_member(jdevice, "min-interval")) != NULL) {
		if(jchild->tag == JSON_NUMBER) {
			min_interval = jchild->number_;
		}
	}

	return NULL;
}

static void *addDevice(int reason, void *param) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct data_t *node = NULL;
	double itmp = 0;
	int match = 0;

	if(param == NULL) {
		return NULL;
	}

	if(!(openweathermap->masterOnly == 0 || pilight.runmode == STANDALONE)) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(openweathermap->id, jchild->string_) == 0) {
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
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(node, '\0', sizeof(struct data_t));

	node->interval = min_interval;
	node->country = NULL;
	node->location = NULL;
	node->update = 0;
	node->temp_offset = 0;

	int has_country = 0, has_location = 0, has_api = 0;
	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			has_country = 0, has_location = 0, has_api = 0;
			jchild1 = json_first_child(jchild);

			while(jchild1) {
				if(strcmp(jchild1->key, "api") == 0) {
					has_api = 1;
					if((node->api = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					strcpy(node->api, jchild1->string_);
				}
				if(strcmp(jchild1->key, "location") == 0) {
					has_location = 1;
					if((node->location = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					strcpy(node->location, jchild1->string_);
				}
				if(strcmp(jchild1->key, "country") == 0) {
					has_country = 1;
					if((node->country = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jdevice->key);

	if((node->enable_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	if((node->update_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	node->enable_timer_req->data = node;
	node->update_timer_req->data = node;

	uv_timer_init(uv_default_loop(), node->enable_timer_req);
	uv_timer_init(uv_default_loop(), node->update_timer_req);

	assert(node->interval > 0);
	uv_timer_start(node->enable_timer_req, (void (*)(uv_timer_t *))enable, node->interval*1000, 0);
	/*
	 * Do an update when this device is added after 3 seconds
	 * to make sure pilight is actually running.
	 */
	uv_timer_start(node->update_timer_req, (void (*)(uv_timer_t *))update, 3000, 0);
	return NULL;
}

static int checkValues(struct JsonNode *code) {
	double dbl = min_interval;

	json_find_number(code, "poll-interval", &dbl);

	if((int)round(dbl) < min_interval) {
		logprintf(LOG_ERR, "openweathermap poll-interval cannot be lower than %d", min_interval);
		return 1;
	}

	return 0;
}

static int createCode(struct JsonNode *code, char **message) {
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

		time_t currenttime = time(NULL);
		if(time_override > -1) {
			currenttime = time_override;
		}

		while(tmp) {
			if(strcmp(tmp->country, country) == 0
			   && strcmp(tmp->location, location) == 0
				 && strcmp(tmp->api, api) == 0) {
				if((currenttime-tmp->update) > min_interval) {

					uv_work_t *work_req = NULL;
					if((work_req = MALLOC(sizeof(uv_work_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					work_req->data = tmp;
					uv_queue_work(uv_default_loop(), work_req, "openweathermap", thread, thread_free);

					if(time_override > -1) {
						tmp->update = time_override;
					} else {
						tmp->update = time(NULL);
					}
				}
			}
			tmp = tmp->next;
		}
	} else {
		logprintf(LOG_ERR, "openweathermap: insufficient number of arguments");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->country);
		FREE(tmp->location);
		FREE(tmp->name);
		FREE(tmp->api);
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
void openweathermapInit(void) {

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
	options_add(&openweathermap->options, 'a', "api", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[a-zA-Z0-9]+$");
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
	// openweathermap->initDev=&initDev;
	openweathermap->checkValues=&checkValues;
	openweathermap->gc=&gc;
	openweathermap->printHelp=&printHelp;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
	eventpool_callback(REASON_DEVICE_ADAPT, adaptDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "openweathermap";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	openweathermapInit();
}
#endif
