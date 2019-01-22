/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/eventpool.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../config/settings.h"
#include "../protocol.h"
#include "dht11.h"

#define MAXTIMINGS 100

#if !defined(__FreeBSD__) && !defined(_WIN32)
#include <wiringx.h>

typedef struct data_t {
	char *name;

	int id;
	int interval;

	double temp_offset;
	double humi_offset;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

// static uint8_t sizecvt(const int read_value) {
	// /* digitalRead() and friends from wiringx are defined as returning a value
	   // < 256. However, they are returned as int() types. This is a safety function */
	// if(read_value > 255 || read_value < 0) {
		// logprintf(LOG_NOTICE, "invalid data from wiringX library");
		// return -1;
	// }

	// return (uint8_t)read_value;
// }

// static void *reason_code_received_free(void *param) {
	// struct reason_code_received_t *data = param;
	// FREE(data);
	// return NULL;
// }

/*
 * FIXME
 */
// static void *thread(void *param) {
	// struct data_t *settings = param;
	// uint8_t laststate = HIGH;
	// uint8_t counter = 0;
	// uint8_t j = 0, i = 0;

	// int x = 0, dht11_dat[5] = {0,0,0,0,0};

	// // pull pin down for 18 milliseconds
	// pinMode(settings->id, PINMODE_OUTPUT);
	// digitalWrite(settings->id, HIGH);
// #ifdef _WIN32
	// SleepEx(500000, True);  // 500 ms
// #else
	// usleep(500000);  // 500 ms
// #endif
	// // then pull it up for 40 microseconds
	// digitalWrite(settings->id, LOW);
// #ifdef _WIN32
	// SleepEx(20000, True);  // 500 ms
// #else
	// usleep(20000);  // 500 ms
// #endif
	// // prepare to read the pin
	// pinMode(settings->id, PINMODE_INPUT);

	// // detect change and read data
	// for(i=0;(i<MAXTIMINGS); i++) {
		// counter = 0;
		// delayMicroseconds(10);

		// while((x = sizecvt(digitalRead(settings->id))) == laststate && x != -1) {
			// counter++;
			// delayMicroseconds(1);
			// if(counter == 255) {
				// break;
			// }
		// }
		// laststate = sizecvt(digitalRead(settings->id));

		// if(counter == 255) {
			// break;
		// }

		// // ignore first 3 transitions
		// if((i >= 4) && (i%2 == 0)) {

			// // shove each bit into the storage bytes
			// dht11_dat[(int)((double)j/8)] <<= 1;
			// if(counter > 16)
				// dht11_dat[(int)((double)j/8)] |= 1;
			// j++;
		// }
	// }

	// // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
	// // print it out if data is good
	// if((j >= 40) && (dht11_dat[4] == ((dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF))) {
		// double h = dht11_dat[0];
		// double t = dht11_dat[2];
		// t += settings->temp_offset;
		// h += settings->humi_offset;

		// struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
		// if(data == NULL) {
			// OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		// }
		// snprintf(data->message, 1024, "{\"gpio\":%d,\"temperature\":%.1f,\"humidity\":%.1f}", settings->id, t, h);
		// strncpy(data->origin, "receiver", 255);
		// data->protocol = dht11->id;
		// if(strlen(pilight_uuid) > 0) {
			// data->uuid = pilight_uuid;
		// } else {
			// data->uuid = NULL;
		// }
		// data->repeat = 1;
		// eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
	// } else {
		// logprintf(LOG_DEBUG, "dht11 data checksum was wrong");
	// }

	// // struct timeval tv;
	// // tv.tv_sec = settings->interval;
	// // tv.tv_usec = 0;
	// // threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);

	// return (void *)NULL;
// }

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	// struct timeval tv;
	int match = 0, interval = 10;
	double itmp = 0.0;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(dht11->id, jchild->string_) == 0) {
				match = 1;
				break;
		}
			jchild = jchild->next;
		}
	}

	if(match == 0) {
		return NULL;
	}

	if((node = MALLOC(sizeof(struct data_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	node->id = 0;
	node->temp_offset = 0;
	node->humi_offset = 0;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "gpio", &itmp) == 0) {
				node->id = (int)round(itmp);
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);

	json_find_number(jdevice, "temperature-offset", &node->temp_offset);
	json_find_number(jdevice, "humidity-offset", &node->humi_offset);

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jdevice->key);

	node->interval = interval;

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
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}

static int checkValues(struct JsonNode *code) {
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	double itmp = -1;

	/* Validate GPIO number */
	if((jid = json_find_member(code, "id")) != NULL) {
		if((jchild = json_find_element(jid, 0)) != NULL) {
			if(json_find_number(jchild, "gpio", &itmp) == 0) {
#if defined(__arm__) || defined(__mips__)					
				int gpio = (int)itmp;
				char *platform = GPIO_PLATFORM;

				if(config_setting_get_string("gpio-platform", 0, &platform) != 0) {
					logprintf(LOG_ERR, "no gpio-platform configured");
					return NULL;
				}
				if(strcmp(platform, "none") == 0) {
					FREE(platform);
					logprintf(LOG_ERR, "no gpio-platform configured");
					return NULL;
				}
				if(wiringXSetup(platform, _logprintf) < 0) {
					FREE(platform);
					return NULL;
				}
				if(wiringXValidGPIO(gpio) != 0) {
					FREE(platform);
					logprintf(LOG_ERR, "dht22: invalid gpio range");
					return NULL;
				}
				FREE(platform);
#endif
			}
		}
	}

	return 0;
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void dht11Init(void) {

	protocol_register(&dht11);
	protocol_set_id(dht11, "dht11");
	protocol_device_add(dht11, "dht11", "1-wire Temperature and Humidity Sensor");
	dht11->devtype = WEATHER;
	dht11->hwtype = SENSOR;
	dht11->multipleId = 0;

	options_add(&dht11->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&dht11->options, "h", "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&dht11->options, "g", "gpio", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);

	// options_add(&dht11->options, "0", "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&dht11->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&dht11->options, "0", "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&dht11->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&dht11->options, "0", "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&dht11->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&dht11->options, "0", "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&dht11->options, "0", "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

#if !defined(__FreeBSD__) && !defined(_WIN32)
	// dht11->initDev=&initDev;
	dht11->gc=&gc;
	dht11->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "dht11";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	dht11Init();
}
#endif
