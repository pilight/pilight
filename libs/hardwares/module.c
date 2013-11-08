/*
	Copyright (C) 2013 CurlyMo

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
#include "log.h"
#include "hardware.h"
#include "module.h"

const char *module_socket = DEFAULT_LIRC_SOCKET;
int module_initialized = 0;
int module_setfreq = 0;
int module_fd = 0;

typedef unsigned long __u32;

unsigned short moduleHwInit(void) {
	unsigned int freq = 0;
	char *socket = NULL;

	if(settings_find_string("hw-socket", &socket) == 0) {
		module_socket = socket;
	}

	if(strcmp(module_socket, "/var/lirc/lircd") == 0) {
		logprintf(LOG_ERR, "refusing to connect to lircd socket");
		return EXIT_FAILURE;
	}

	if(module_initialized == 0) {
		if((module_fd = open(module_socket, O_RDWR)) < 0) {
			logprintf(LOG_ERR, "could not open %s", module_socket);
			return EXIT_FAILURE;
		} else {
			/* Only set the frequency once */
			if(module_setfreq == 0) {
				freq = FREQ433;
				/* Set the lirc_rpi frequency to 433.92Mhz */
				if(ioctl(module_fd, _IOW('i', 0x00000013, __u32), &freq) == -1) {
					logprintf(LOG_ERR, "could not set lirc_rpi send frequency");
					exit(EXIT_FAILURE);
				}
				module_setfreq = 1;
			}
			logprintf(LOG_DEBUG, "initialized lirc_rpi module");
			module_initialized = 1;
		}
	}
	return EXIT_SUCCESS;
}

unsigned short moduleHwDeinit(void) {
	unsigned int freq = 0;

	if(module_initialized == 1) {

		freq = FREQ38;

		/* Restore the lirc_rpi frequency to its default value */
		if(ioctl(module_fd, _IOW('i', 0x00000013, __u32), &freq) == -1) {
			logprintf(LOG_ERR, "could not restore default freq of the lirc_rpi module");
			exit(EXIT_FAILURE);
		} else {
			logprintf(LOG_DEBUG, "default freq of the lirc_rpi module set");
		}

		if(module_fd != 0) {
			close(module_fd);
			module_fd = 0;
		}

		logprintf(LOG_DEBUG, "deinitialized lirc_rpi module");
		module_initialized	= 0;
	}

	return EXIT_SUCCESS;
}

int moduleSend(int *code) {
	size_t i = 0;
	while(code[i]) {
		i++;
	}
	i++;
	i*=sizeof(int);
	int n = write(module_fd, code, i);

	if(n == i) {
		return 0;
	} else {
		return 1;
	}
}

int moduleReceive(void) {
	int data = 0;

	if((read(module_fd, &data, sizeof(data))) == 0) {
		data = 1;
	}

	return (data & 0x00FFFFFF);
}

void moduleInit(void) {

	hardware_register(&module);
	hardware_set_id(module, "module");

	module->init=&moduleHwInit;
	module->deinit=&moduleHwDeinit;
	module->send=&moduleSend;
	module->receive=&moduleReceive;
}