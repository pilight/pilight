/*
	Copyright (C) 2013 - 2016 CurlyMo

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
#include <sys/stat.h>
#ifndef _WIN32
	#include <unistd.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "../../core/eventpool.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "ds18s20.h"

typedef struct data_t {
	char *name;

	char *id;
	char *sensor;
	char *w1slave;

	double temp_offset;
	int interval;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static char source_path[21];

#ifndef _WIN32
// static void *reason_code_received_free(void *param) {
	// struct reason_code_received_t *data = param;
	// FREE(data);
	// return NULL;
// }
#endif

// static void *thread(void *param) {

// #ifndef _WIN32
	// struct data_t *settings = param;
	// char *content = NULL;
	// struct stat st;

	// FILE *fp = NULL;
	// char crcVar[5];
	// int w1valid = 0;
	// double w1temp = 0.0;
	// size_t bytes = 0;


	// if((fp = fopen(settings->w1slave, "rb")) == NULL) {
		// logprintf(LOG_NOTICE, "cannot read w1 file: %s", settings->w1slave);
		// return NULL;
	// }

	// fstat(fileno(fp), &st);
	// bytes = (size_t)st.st_size;

	// if((content = REALLOC(content, bytes+1)) == NULL) {
		// OUT_OF_MEMORY
	// }
	// memset(content, '\0', bytes+1);

	// if(fread(content, sizeof(char), bytes, fp) == -1) {
		// logprintf(LOG_NOTICE, "cannot read config file: %s", settings->w1slave);
		// fclose(fp);
		// return NULL;
	// }
	// fclose(fp);

	// w1valid = 0;

	// char **array = NULL;
	// unsigned int n = explode(content, "\n", &array);
	// if(n > 0) {
		// sscanf(array[0], "%*x %*x %*x %*x %*x %*x %*x %*x %*x : crc=%*x %s", crcVar);
		// if(strncmp(crcVar, "YES", 3) == 0 && n > 1) {
			// w1valid = 1;
			// sscanf(array[1], "%*x %*x %*x %*x %*x %*x %*x %*x %*x t=%lf", &w1temp);
			// w1temp = (w1temp/1000)+settings->temp_offset;
		// }
	// }
	// array_free(&array, n);

	// if(w1valid == 1) {
		// struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
		// if(data == NULL) {
			// OUT_OF_MEMORY
		// }
		// snprintf(data->message, 1024, "{\"id\":\"%s\",\"temperature\":%.1f}", settings->id, w1temp);
		// strncpy(data->origin, "receiver", 255);
		// data->protocol = ds18s20->id;
		// if(strlen(pilight_uuid) > 0) {
			// data->uuid = pilight_uuid;
		// } else {
			// data->uuid = NULL;
		// }
		// data->repeat = 1;
		// eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
	// }

	// // struct timeval tv;
	// // tv.tv_sec = settings->interval;
	// // tv.tv_usec = 0;
	// // threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);
// #endif

	// return (void *)NULL;
// }

static void *addDevice(int reason, void *param) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	// struct timeval tv;
	char *stmp = NULL;
	int match = 0/*, interval = 10*/;
	// double itmp = 0.0;

#ifndef _WIN32
	struct dirent *file = NULL;
	DIR *d = NULL;
#endif

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(ds18s20->id, jchild->string_) == 0) {
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
	node->id = NULL;
	node->sensor = NULL;
	node->w1slave = NULL;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				if((node->id = MALLOC(strlen(stmp)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(node->id, stmp);
			}
			jchild = jchild->next;
		}
	}

	// if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		// interval = (int)round(itmp);

#ifndef _WIN32
	if((node->sensor = REALLOC(node->sensor, strlen(source_path)+strlen(node->id)+5)) == NULL) {
		OUT_OF_MEMORY
	}
	sprintf(node->sensor, "%s10-%s/", source_path, node->id);
	if((d = opendir(node->sensor))) {
		while((file = readdir(d)) != NULL) {
	#ifndef __sun
			if(file->d_type == DT_REG) {
	#else
			struct stat s;
			stat(file->d_name, &s);
			if(s.st_mode & S_IFDIR) {
	#endif
				if(strcmp(file->d_name, "w1_slave") == 0) {
					size_t w1slavelen = strlen(node->sensor)+10;
					if((node->w1slave = REALLOC(node->w1slave, w1slavelen+1)) == NULL) {
						OUT_OF_MEMORY
					}

					memset(node->w1slave, '\0', w1slavelen);
					strncpy(node->w1slave, node->sensor, strlen(node->sensor));
					strcat(node->w1slave, "w1_slave");
				}
			}
		}
		closedir(d);
	} else {
		logprintf(LOG_NOTICE, "1-wire device %s does not exists", node->sensor);
		return NULL;
	}
#endif

	json_find_number(jdevice, "temperature-offset", &node->temp_offset);

	node->next = data;
	data = node;

	// tv.tv_sec = interval;
	// tv.tv_usec = 0;
	// threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->name);
		if(tmp->id != NULL) {
			FREE(tmp->id);
		}
		if(tmp->sensor != NULL) {
			FREE(tmp->sensor);
		}
		if(tmp->w1slave != NULL) {
			FREE(tmp->w1slave);
		}
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void ds18s20Init(void) {
	protocol_register(&ds18s20);
	protocol_set_id(ds18s20, "ds18s20");
	protocol_device_add(ds18s20, "ds18s20", "1-wire Temperature Sensor");
	ds18s20->devtype = WEATHER;
	ds18s20->hwtype = SENSOR;

	options_add(&ds18s20->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&ds18s20->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[a-z0-9]{12}$");

	// options_add(&ds18s20->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&ds18s20->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&ds18s20->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&ds18s20->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&ds18s20->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	memset(source_path, '\0', 21);
	strcpy(source_path, "/sys/bus/w1/devices/");

	// ds18s20->initDev=&initDev;
	ds18s20->gc=&gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ds18s20";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	ds18s20Init();
}
#endif
