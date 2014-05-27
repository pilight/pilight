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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "hardware.h"
#include "json.h"
#include "gc.h"
#include "433lirc.h"

#define FREQ433				433920
#define FREQ38				38000

static int lirc_433_initialized = 0;
static int lirc_433_setfreq = 0;
static int lirc_433_fd = 0;
static char *lirc_433_socket = NULL;

typedef unsigned long __u32;

static unsigned short lirc433HwInit(void) {
	unsigned int freq = 0;

	if(strcmp(lirc_433_socket, "/var/lirc/lircd") == 0) {
		logprintf(LOG_ERR, "refusing to connect to lircd socket");
		return EXIT_FAILURE;
	}

	if(lirc_433_initialized == 0) {
		if((lirc_433_fd = open(lirc_433_socket, O_RDWR)) < 0) {
			logprintf(LOG_ERR, "could not open %s", lirc_433_socket);
			return EXIT_FAILURE;
		} else {
			/* Only set the frequency once */
			if(lirc_433_setfreq == 0) {
				freq = FREQ433;
				/* Set the lirc_rpi frequency to 433.92Mhz */
				if(ioctl(lirc_433_fd, _IOW('i', 0x00000013, __u32), &freq) == -1) {
					logprintf(LOG_ERR, "could not set lirc_rpi send frequency");
					exit(EXIT_FAILURE);
				}
				lirc_433_setfreq = 1;
			}
			logprintf(LOG_DEBUG, "initialized lirc_rpi lirc");
			lirc_433_initialized = 1;
		}
	}
	return EXIT_SUCCESS;
}

static unsigned short lirc433HwDeinit(void) {
	unsigned int freq = 0;

	if(lirc_433_initialized == 1) {

		freq = FREQ38;

		if(lirc_433_fd != 0) {
			/* Restore the lirc_rpi frequency to its default value */
			if(ioctl(lirc_433_fd, _IOW('i', 0x00000013, __u32), &freq) == -1) {
				logprintf(LOG_ERR, "could not restore default freq of the lirc_rpi lirc");
				exit(EXIT_FAILURE);
			} else {
				logprintf(LOG_DEBUG, "default freq of the lirc_rpi lirc set");
			}

			close(lirc_433_fd);
			lirc_433_fd = 0;
		}

		logprintf(LOG_DEBUG, "deinitialized lirc_rpi lirc");
		lirc_433_initialized = 0;
	}

	return EXIT_SUCCESS;
}

static int lirc433Send(int *code) {
	size_t i = 0;
	while(code[i]) {
		i++;
	}
	i++;
	i*=sizeof(int);
	ssize_t n = write(lirc_433_fd, code, i);

	if(n == i) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}

static int lirc433Receive(void) {
	int data = 0;

	if((read(lirc_433_fd, &data, sizeof(data))) == 0) {
		data = 1;
	}

	return (data & 0x00FFFFFF);
}

static unsigned short lirc433Settings(JsonNode *json) {
	if(strcmp(json->key, "socket") == 0) {
		if(json->tag == JSON_STRING) {
			lirc_433_socket = malloc(strlen(json->string_)+1);
			if(!lirc_433_socket) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(lirc_433_socket, json->string_);
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}


static int lirc433gc(void) {
	if(lirc_433_socket) {
		sfree((void *)&lirc_433_socket);
	}

	return 1;
}

#ifndef MODULE
__attribute__((weak))
#endif
void lirc433Init(void) {

	gc_attach(lirc433gc);

	hardware_register(&lirc433);
	hardware_set_id(lirc433, "433lirc");

	options_add(&lirc433->options, 's', "socket", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_STRING, NULL, "^/dev/([a-z]+)[0-9]+$");

	lirc433->type=RF433;
	lirc433->init=&lirc433HwInit;
	lirc433->deinit=&lirc433HwDeinit;
	lirc433->send=&lirc433Send;
	lirc433->receive=&lirc433Receive;
	lirc433->settings=&lirc433Settings;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name =  "433lirc";
	module->version =  "1.0";
	module->reqversion =  "4.0";
	module->reqcommit =  "45";
}

void init(void) {
	lirc433Init();
}
#endif
