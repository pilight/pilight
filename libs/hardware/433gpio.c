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

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "hardware.h"
#include "wiringPi.h"
#include "json.h"
#include "irq.h"
#include "433gpio.h"

int gpio_433_in = 0;
int gpio_433_out = 0;
int gpio_433_initialized = 0;

unsigned short gpio433HwInit(void) {
	if(wiringPiSetup() == -1) {
		return EXIT_FAILURE;
	}
	gpio_433_initialized = 1;
	pinMode(gpio_433_out, OUTPUT);
	if(wiringPiISR(gpio_433_in, INT_EDGE_BOTH) < 0) {
		logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_433_in) ;
		return EXIT_SUCCESS;
	}
	return EXIT_SUCCESS;
}

unsigned short gpio433HwDeinit(void) {
	FILE *fd;
	if(gpio_433_initialized) {
		if((fd = fopen ("/sys/class/gpio/unexport", "w"))) {
			fprintf(fd, "%d", wpiPinToGpio(gpio_433_out));
			fclose(fd);
		}
		if((fd = fopen ("/sys/class/gpio/unexport", "w"))) {
			fprintf(fd, "%d", wpiPinToGpio(gpio_433_in));
			fclose(fd);
		}
	}
	return EXIT_SUCCESS;
}

int gpio433Send(int *code) {
	unsigned short i = 0;

	while(code[i]) {
		digitalWrite(gpio_433_out, 1);
		usleep((__useconds_t)code[i++]);
		digitalWrite(gpio_433_out, 0);
		usleep((__useconds_t)code[i++]);
	}
	return EXIT_SUCCESS;
}

int gpio433Receive(void) {
	return irq_read(gpio_433_in);
}

unsigned short gpio433Settings(JsonNode *json) {
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

void gpio433Init(void) {

	hardware_register(&gpio433);
	hardware_set_id(gpio433, "433gpio");

	piHiPri(55);

	options_add(&gpio433->options, 'r', "receiver", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]+$");
	options_add(&gpio433->options, 's', "sender", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]+$");

	gpio433->type=RF433;
	gpio433->init=&gpio433HwInit;
	gpio433->deinit=&gpio433HwDeinit;
	gpio433->send=&gpio433Send;
	gpio433->receive=&gpio433Receive;
	gpio433->settings=&gpio433Settings;
}
