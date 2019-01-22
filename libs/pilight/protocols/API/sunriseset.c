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
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef __USE_XOPEN
	#define __USE_XOPEN
#endif
#include <time.h>
#include <math.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/ntp.h"
#include "../../core/dso.h"
#include "../../core/datetime.h" // Full path because we also have a datetime protocol
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/json.h"
#include "sunriseset.h"

#define PI 3.1415926
#define PIX 57.29578049044297 // 180 / PI
#define ZENITH 90.83333333333333

static int time_override = -1;
static char UTC[] = "UTC";

typedef struct data_t {
	char *name;
	int interval;
	int target_offset;
	uv_timer_t *timer_req;

	double longitude;
	double latitude;

	char *tz;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static double calculate(int year, int month, int day, double lon, double lat, int rising) {
	int N = (int)((floor(275 * month / 9)) - ((floor((month + 9) / 12)) *
			((1 + floor((year - 4 * floor(year / 4) + 2) / 3)))) + (int)day - 30);

	double lngHour = lon / 15.0;
	double T = 0;

	if(rising) {
		T = N + ((6 - lngHour) / 24);
	} else {
		T = N + ((18 - lngHour) / 24);
	}

	double M = (0.9856 * T) - 3.289;
	double M1 = M * PI / 180;
	double L = fmod((M + (1.916 * sin(M1)) + (0.020 * sin(2 * M1)) + 282.634), 360.0);
	double L1 = L * PI / 180;
	double L2 = lat * PI / 180;
	double SD = 0.39782 * sin(L1);
	double CH = (cos(ZENITH * PI / 180)-(SD * sin(L2))) / (cos((PIX * asin((SD))) * PI / 180) * cos(L2));

	if(CH > 1) {
		return -1;
	} else if(CH < -1) {
		return -1;
	}

	double RA = fmod((PIX * atan((0.91764 * tan(L1)))), 360);
	double MQ = (RA + (((floor(L / 90)) * 90) - ((floor(RA / 90)) * 90))) / 15;
	double A;
	double B;
	if(rising == 0) {
		A = 0.06571;
		B = 6.595;
	} else {
		A = 0.06571;
		B = 6.618;
	}

	double t = ((rising ? 360 - PIX * acos(CH) : PIX * acos(CH)) / 15) + MQ - (A * T) - B;
	double UT = fmod((t - lngHour) + 24.0, 24.0);
	double min = (round(60*fmod(UT, 1))/100);

	if(min >= 0.60) {
		min -= 0.60;
	}

	double hour = UT-min;

	return ((round(hour)+min))*100;
}

#undef min
static unsigned long min(unsigned long a, unsigned long b, unsigned long c) {
	unsigned long m = a;
	if(m > b) {
		m = b;
	}
	if(m > c) {
		return c;
	}
	return m;
}

static void *thread(void *param) {
	uv_timer_t *task = param;
	struct data_t *settings = task->data;
	struct tm tm;
	struct tm rise;
	struct tm set;
	struct tm midnight;
	time_t t;
	int risetime = 0, settime = 0, hournow = 0;

	if(time_override > -1) {
		t = time_override;
	} else {
		t = time(NULL);
		if(isntpsynced() == 0) {
			t -= getntpdiff();
		}
	}

	if(localtime_l(t, &tm, settings->tz) == 0) {
		int year = tm.tm_year+1900;
		int month = tm.tm_mon+1;
		int day = tm.tm_mday;
		int hour = tm.tm_hour;
		int minute = tm.tm_min;

		risetime = (int)calculate(year, month, day, settings->longitude, settings->latitude, 1);
		settime = (int)calculate(year, month, day, settings->longitude, settings->latitude, 0);

		t = datetime2ts(year, month, day, risetime / 100, risetime % 100, 0);
		localtime_l(t, &rise, settings->tz);
		risetime = ((rise.tm_hour*100)+rise.tm_min);

		t = datetime2ts(year, month, day, settime / 100, settime % 100, 0);
		localtime_l(t, &set, settings->tz);
		settime = ((set.tm_hour*100)+set.tm_min);

		memcpy(&midnight, &tm, sizeof(struct tm));
		midnight.tm_hour = 23;
		midnight.tm_min = 59;
		midnight.tm_sec = 59;
		
		unsigned long tset = mktime(&set);
		unsigned long trise = mktime(&rise);
		unsigned long tmidnight = mktime(&midnight);
		unsigned long tnow = mktime(&tm);

		/* To make sure we are always calculating ahead */
		if(tset < tnow) {
			tset += 86400;
		}
		if(trise < tnow) {
			trise += 86400;
		}
		if(tmidnight < tnow) {
			tmidnight += 86400;
		}

		unsigned long time2set = tset-tnow;
		unsigned long time2rise = trise-tnow;
		unsigned long time2midnight = (tmidnight+1)-tnow;

		char *_sun = NULL;
		hournow = (hour*100)+minute;
		
		if(hournow >= risetime && hournow < settime) {
			_sun = "rise";
		} else {
			_sun = "set";
		}

		struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
		if(data == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		snprintf(data->message, 1024,
			"{\"longitude\":%.6f,\"latitude\":%.6f,\"sun\":\"%s\",\"sunrise\":%.2f,\"sunset\":%.2f}",
			settings->longitude, settings->latitude, _sun, (rise.tm_hour+(double)rise.tm_min/100), ((set.tm_hour+(double)set.tm_min/100))
		);
		strncpy(data->origin, "receiver", 255);
		data->protocol = sunriseset->id;
		if(strlen(pilight_uuid) > 0) {
			data->uuid = pilight_uuid;
		} else {
			data->uuid = NULL;
		}
		data->repeat = 1;
		eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

		unsigned long next = 0;
		if((next = min(time2set, time2rise, time2midnight)) == 0) {
			if(time2set == 0) {
				next = time2midnight;
			}
			if(time2rise == 0) {
				next = time2set;
			}
			if(time2midnight == 0) {
				next = time2rise;
			}
		}
		assert(next > 0);
		uv_timer_start(settings->timer_req, (void (*)(uv_timer_t *))thread, next*1000, 0);
	}

	return (void *)NULL;
}

static void *adaptDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jtime = NULL;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if(strcmp(jdevice->key, "sunriseset") != 0) {
		return NULL;
	}

	if((jtime = json_find_member(jdevice, "time-override")) != NULL) {
		if(jtime->tag == JSON_NUMBER) {
			time_override = jtime->number_;
		}
	}

	return NULL;
}

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct data_t *node = NULL;
	int match = 0;

	if(param == NULL) {
		return NULL;
	}

	if(!(sunriseset->masterOnly == 0 || pilight.runmode == STANDALONE)) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(sunriseset->id, jchild->string_) == 0) {
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

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "longitude") == 0) {
					node->longitude = jchild1->number_;
				}
				if(strcmp(jchild1->key, "latitude") == 0) {
					node->latitude = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}

	if((node->tz = coord2tz(node->longitude, node->latitude)) == NULL) {
		logprintf(LOG_INFO, "sunriseset %s, could not determine timezone", jdevice->key);
		node->tz = UTC;
	} else {
		logprintf(LOG_INFO, "sunriseset %s %.6f:%.6f seems to be in timezone: %s", jdevice->key, node->longitude, node->latitude, node->tz);
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jdevice->key);

	node->next = data;
	data = node;

	if((node->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}	
	node->timer_req->data = node;
	
	uv_timer_init(uv_default_loop(), node->timer_req);

	uv_timer_start(node->timer_req, (void (*)(uv_timer_t *))thread, 1, 0);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->name);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}

static int checkValues(JsonNode *code) {
	char *_sun = NULL;

	if(json_find_string(code, "sun", &_sun) == 0) {
		if(strcmp(_sun, "rise") != 0 && strcmp(_sun, "set") != 0) {
			return 1;
		}
	}
	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void sunRiseSetInit(void) {

	protocol_register(&sunriseset);
	protocol_set_id(sunriseset, "sunriseset");
	protocol_device_add(sunriseset, "sunriseset", "Sunrise / Sunset Calculator");
	sunriseset->devtype = WEATHER;
	sunriseset->hwtype = API;
	sunriseset->multipleId = 0;
	sunriseset->masterOnly = 1;

	options_add(&sunriseset->options, "o", "longitude", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&sunriseset->options, "a", "latitude", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&sunriseset->options, "u", "sunrise", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&sunriseset->options, "d", "sunset", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&sunriseset->options, "s", "sun", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	// options_add(&sunriseset->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&sunriseset->options, "0", "sunriseset-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&sunriseset->options, "0", "show-sunriseset", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	// sunriseset->initDev=&initDev;
	sunriseset->gc=&gc;
	sunriseset->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
	eventpool_callback(REASON_DEVICE_ADAPT, adaptDevice, NULL);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "sunriseset";
	module->version = "4.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	sunRiseSetInit();
}
#endif
