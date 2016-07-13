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
#include "../hardware/hardware.h"
#include "../../wiringx/wiringX.h"
#include "IRgpio.h"

static int gpio_ir_in = 0;
static int gpio_ir_out = 0;
static int doPause = 0;

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

typedef struct data_t {
	int rbuffer[1025];
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
				if(duration > gpioIR->mingaplen) {
					/* Let's do a little filtering here as well */
					if(data->rptr >= gpioIR->minrawlen && data->rptr <= gpioIR->maxrawlen) {
						struct reason_received_pulsetrain_t *data1 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
						if(data1 == NULL) {
							OUT_OF_MEMORY
						}
						data1->length = data->rptr;
						memcpy(data1->pulses, data->rbuffer, data->rptr*sizeof(int));
						data1->hardware = gpioIR->id;

						eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data1);
					}
					data->rptr = 0;
				}
			}

			eventpool_fd_enable_highpri(node);
		} break;
		case EV_DISCONNECTED: {
			FREE(node->userdata);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}
#endif

static unsigned short gpioIRHwInit(void *(*callback)(void *)) {
#if defined(__arm__) || defined(__mips__)
	char *platform = GPIO_PLATFORM;
	if(settings_select_string(ORIGIN_MASTER, "gpio-platform", &platform) != 0 || strcmp(platform, "none") == 0) {
		logprintf(LOG_ERR, "no gpio-platform configured");
		return EXIT_FAILURE;
	}
	if(wiringXSetup(platform, logprintf) < 0) {
		return EXIT_FAILURE;
	}
	if(gpio_ir_out >= 0) {
		if(wiringXValidGPIO(gpio_ir_out) != 0) {
			logprintf(LOG_ERR, "invalid sender pin: %d", gpio_ir_out);
			return EXIT_FAILURE;
		}
		pinMode(gpio_ir_out, PINMODE_OUTPUT);
	}
	if(gpio_ir_in >= 0) {
		if(wiringXValidGPIO(gpio_ir_in) != 0) {
			logprintf(LOG_ERR, "invalid receiver pin: %d", gpio_ir_in);
			return EXIT_FAILURE;
		}
		if(wiringXISR(gpio_ir_in, ISR_MODE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_ir_in);
			return EXIT_SUCCESS;
		}
	}
	if(gpio_ir_in > 0) {
		int fd = wiringXSelectableFd(gpio_ir_in);

		struct data_t *data = MALLOC(sizeof(struct data_t));
		if(data == NULL) {
			OUT_OF_MEMORY;
		}
		memset(data->rbuffer, '\0', 1024);
		data->rptr = 0;
		data->callback = callback;

		eventpool_fd_add("IRgpio", fd, client_callback, NULL, data);
	}

	return EXIT_SUCCESS;
#else
	logprintf(LOG_ERR, "the IRgpio module is not supported on this hardware", gpio_ir_in);
	return EXIT_FAILURE;
#endif
}

// static unsigned short gpioIRHwDeinit(void) {
	// return EXIT_SUCCESS;
// }

static int gpioIRSend(int *code, int rawlen, int repeats) {
	int r = 0, x = 0;
	if(gpio_ir_out >= 0) {
		for(r=0;r<repeats;r++) {
			for(x=0;x<rawlen;x+=2) {
				digitalWrite(gpio_ir_out, 1);
				usleep((__useconds_t)code[x]);
				digitalWrite(gpio_ir_out, 0);
				if(x+1 < rawlen) {
					usleep((__useconds_t)code[x+1]);
				}
			}
		}
		digitalWrite(gpio_ir_out, 0);
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

static unsigned short gpioIRSettings(JsonNode *json) {
	if(strcmp(json->key, "receiver") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_ir_in = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	if(strcmp(json->key, "sender") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_ir_out = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void gpioIRInit(void) {
	hardware_register(&gpioIR);
	hardware_set_id(gpioIR, "IRgpio");

	options_add(&gpioIR->options, 'r', "receiver", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpioIR->options, 's', "sender", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");

	gpioIR->minrawlen = 1000;
	gpioIR->maxrawlen = 0;
	gpioIR->mingaplen = 10000;
	gpioIR->maxgaplen = 100000;

	gpioIR->hwtype=RFIR;
	gpioIR->comtype=COMOOK;
	gpioIR->init=&gpioIRHwInit;
	// gpioIR->deinit=&gpioIRHwDeinit;
	gpioIR->sendOOK=&gpioIRSend;
	// gpioIR->receiveOOK=&gpioIRReceive;
	gpioIR->settings=&gpioIRSettings;

	eventpool_callback(REASON_SEND_BEGIN, receiveStop);
	eventpool_callback(REASON_SEND_END, receiveStart);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "IRgpio";
	module->version = "2.0";
	module->reqversion = "8.0";
	module->reqcommit = NULL;
}

void init(void) {
	gpioIRInit();
}
#endif
