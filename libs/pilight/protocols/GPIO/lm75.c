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
#include <sys/time.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/threadpool.h"
#include "../../core/eventpool.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "../../core/json.h"
#ifndef _WIN32
	#include "../../../wiringx//wiringX.h"
#endif
#include "../protocol.h"
#include "lm75.h"

#if !defined(__FreeBSD__) && !defined(_WIN32) && !defined(__sun)
typedef struct data_t {
	char *name;

	unsigned int id;
	char path[PATH_MAX];
	int fd;
	int interval;

	double temp_offset;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;

	int raw = wiringXI2CReadReg16(settings->fd, 0x00);
	float temp = ((float)((raw&0x00ff)+((raw>>15)?0:0.5))*10);

	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	snprintf(data->message, 1024, "{\"id\":%d,\"temperature\":%.3f}", settings->id, ((temp+settings->temp_offset)/10));
	strncpy(data->origin, "receiver", 255);
	data->protocol = lm75->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

	struct timeval tv;
	tv.tv_sec = settings->interval;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);

	return (void *)NULL;
}

static void *addDevice(int reason, void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	char *stmp = NULL;
	int match = 0, interval = 10;
	double itmp = 0.0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(lm75->id, jchild->string_) == 0) {
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
	node->id = 0;
	node->temp_offset = 0;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				node->id = (int)itmp;
			}
			if(json_find_string(jchild, "i2c-path", &stmp) == 0) {
				strcpy(node->path, stmp);
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	json_find_number(jdevice, "temperature-offset", &node->temp_offset);

	node->fd = wiringXI2CSetup(node->path, node->id);
	if(node->fd <= 0) {
		logprintf(LOG_NOTICE, "error connecting to lm75");
		logprintf(LOG_DEBUG, "(probably i2c bus error from wiringXI2CSetup)");
		logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, jdevice->key);

	node->interval = interval;

	node->next = data;
	data = node;

	tv.tv_sec = interval;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}
#endif

#if !defined(MODULE) && !defined(_WIN32) && !defined(__sun)
__attribute__((weak))
#endif
void lm75Init(void) {

	protocol_register(&lm75);
	protocol_set_id(lm75, "lm75");
	protocol_device_add(lm75, "lm75", "TI I2C Temperature Sensor");
	lm75->devtype = WEATHER;
	lm75->hwtype = SENSOR;
	lm75->multipleId = 0;

	options_add(&lm75->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&lm75->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "0x[0-9a-f]{2}");
	options_add(&lm75->options, 'd', "i2c-path", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^/dev/i2c-[0-9]{1,2}%");

	// options_add(&lm75->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&lm75->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&lm75->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32) && !defined(__sun)
	// lm75->initDev=&initDev;
	lm75->gc=&gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
#endif
}

#if defined(MODULE) && !defined(_WIN32) && !defined(__sun)
void compatibility(struct module_t *module) {
	module->name = "lm75";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	lm75Init();
}
#endif
