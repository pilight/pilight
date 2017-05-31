/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include <string.h>
#include <unistd.h>
#include <wiringx.h>

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/irq.h"
#include "../config/hardware.h"
#include "../config/settings.h"
#include "433gpio.h"

static int gpio_433_in = 0;
static int gpio_433_out = 0;

static unsigned short gpio433HwInit(void) {
	char *platform = GPIO_PLATFORM;

	if(settings_find_string("gpio-platform", &platform) != 0 || strcmp(platform, "none") == 0) {
		logprintf(LOG_ERR, "no gpio-platform configured");
		return EXIT_FAILURE;
	}
	if(wiringXSetup(platform, logprintf1) < 0) {
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
	return EXIT_SUCCESS;
}

static unsigned short gpio433HwDeinit(void) {
	return EXIT_SUCCESS;
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
		sleep(1);
	}
	return EXIT_SUCCESS;
}

static int gpio433Receive(void) {
	if(gpio_433_in >= 0) {
		return irq_read(gpio_433_in);
	} else {
		sleep(1);
		return 0;
	}
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
	gpio433->deinit=&gpio433HwDeinit;
	gpio433->sendOOK=&gpio433Send;
	gpio433->receiveOOK=&gpio433Receive;
	gpio433->settings=&gpio433Settings;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "433gpio";
	module->version = "1.3";
	module->reqversion = "7.0";
	module->reqcommit = "186";
}

void init(void) {
	gpio433Init();
}
#endif
