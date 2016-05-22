/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/threadpool.h"
#include "../core/eventpool.h"
#include "../hardware/hardware.h"
#include "../../wiringx/wiringX.h"
#include "433gpio.h"

static int gpio_433_in = 0;
static int gpio_433_out = 0;
static int doPause = 0;

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

typedef struct data_t {
	int rbuffer[1024];
	int rptr;
	void *(*callback)(void *);
} data_t;

struct timestamp_t timestamp;

#if defined(__arm__) || defined(__mips__)
static void *reason_received_pulsetrain_free(void *param) {
	struct reason_received_pulsetrain_t *data = param;
	FREE(data);
	return NULL;
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	struct data_t *data = node->userdata;
	int duration = 0;

	if(doPause == 1) {
		return 0;
	}
	switch(event) {
		case EV_CONNECT_SUCCESS: {
			eventpool_fd_enable_highpri(node);
			timestamp.first = 0;
			timestamp.second = 0;
		} break;
		case EV_HIGHPRI: {
			eventpool_fd_enable_highpri(node);
			uint8_t c = 0;

			(void)read(node->fd, &c, 1);
			lseek(node->fd, 0, SEEK_SET);

			struct timeval tv;
			gettimeofday(&tv, NULL);
			timestamp.first = timestamp.second;
			timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

			duration = (int)((int)timestamp.second-(int)timestamp.first);
			if(duration > 0) {
				data->rbuffer[data->rptr++] = duration;
				if(data->rptr > MAXPULSESTREAMLENGTH-1) {
					data->rptr = 0;
				}
				if(duration > gpio433->mingaplen) {
					/* Let's do a little filtering here as well */
					if(data->rptr >= gpio433->minrawlen && data->rptr <= gpio433->maxrawlen) {
						struct reason_received_pulsetrain_t *data1 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
						if(data1 == NULL) {
							OUT_OF_MEMORY
						}
						data1->length = data->rptr;
						memcpy(data1->pulses, data->rbuffer, data->rptr*sizeof(int));
						data1->hardware = gpio433->id;

						eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data1);
					}
					data->rptr = 0;
				}
			}
		} break;
		case EV_DISCONNECTED: {
			FREE(node->userdata);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}
#endif

static unsigned short gpio433HwInit(void *(*callback)(void *)) {
#if defined(__arm__) || defined(__mips__)
	char *platform = GPIO_PLATFORM;
	if(settings_select_string(ORIGIN_MASTER, "gpio-platform", &platform) != 0 || strcmp(platform, "none") == 0) {
		logprintf(LOG_ERR, "no gpio-platform configured");
		return EXIT_FAILURE;
	}
	if(wiringXSetup(platform, logprintf) < 0) {
		return EXIT_FAILURE;
	}
	if(gpio_433_out >= 0) {
		if(wiringXValidGPIO(gpio_433_out) != 0) {
			logprintf(LOG_ERR, "invalid sender pin: %d", gpio_433_out);
			return EXIT_FAILURE;
		}
		pinMode(gpio_433_out, PINMODE_OUTPUT);
	}
	if(gpio_433_in >= 0) {
		if(wiringXValidGPIO(gpio_433_in) != 0) {
			logprintf(LOG_ERR, "invalid receiver pin: %d", gpio_433_in);
			return EXIT_FAILURE;
		}
		if(wiringXISR(gpio_433_in, ISR_MODE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_433_in);
			return EXIT_SUCCESS;
		}
	}
	if(gpio_433_in > 0) {
		int fd = wiringXSelectableFd(gpio_433_in);

		struct data_t *data = MALLOC(sizeof(struct data_t));
		if(data == NULL) {
			OUT_OF_MEMORY;
		}
		memset(data->rbuffer, '\0', sizeof(data->rbuffer));
		data->rptr = 0;
		data->callback = callback;

		eventpool_fd_add("433gpio", fd, client_callback, NULL, data);
	}
	return EXIT_SUCCESS;
#else
	logprintf(LOG_ERR, "the 433gpio module is not supported on this hardware", gpio_433_in);
	return EXIT_FAILURE;
#endif
}

static int gpio433Send(int *code, int rawlen, int repeats) {
	int r = 0, x = 0;
	if(gpio_433_out >= 0) {
		for(r=0;r<repeats;r++) {
			for(x=0;x<rawlen;x+=2) {
				digitalWrite(gpio_433_out, 1);
				usleep((__useconds_t)code[x]);
				digitalWrite(gpio_433_out, 0);
				if(x+1 < rawlen) {
					usleep((__useconds_t)code[x+1]);
				}
			}
		}
		digitalWrite(gpio_433_out, 0);
	} else {
		usleep(10);
	}
	return EXIT_SUCCESS;
}

static void *receiveStop(void *param) {
	doPause = 1;
	return NULL;
}

static void *receiveStart(void *param) {
	doPause = 0;
	return NULL;
}

static unsigned short gpio433Settings(JsonNode *json) {
	if(strcmp(json->key, "receiver") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_433_in = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	if(strcmp(json->key, "sender") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_433_out = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void gpio433Init(void) {
	hardware_register(&gpio433);
	hardware_set_id(gpio433, "433gpio");

	options_add(&gpio433->options, 'r', "receiver", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpio433->options, 's', "sender", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");

	gpio433->minrawlen = 1000;
	gpio433->maxrawlen = 0;
	gpio433->mingaplen = 5100;
	gpio433->maxgaplen = 10000;

	gpio433->hwtype=RF433;
	gpio433->comtype=COMOOK;
	gpio433->init=&gpio433HwInit;
	// gpio433->deinit=&gpio433HwDeinit;
	gpio433->sendOOK=&gpio433Send;
	// gpio433->receiveOOK=&gpio433Receive;
	gpio433->settings=&gpio433Settings;

	eventpool_callback(REASON_SEND_BEGIN, receiveStop);
	eventpool_callback(REASON_SEND_END, receiveStart);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "433gpio";
	module->version = "2.0";
	module->reqversion = "8.0";
	module->reqcommit = NULL;
}

void init(void) {
	gpio433Init();
}
#endif
