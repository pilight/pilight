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
#include "wiringPi.h"
#include "pilight.h"

int pilight_mod_initialized = 0;
char *pilight_mod_socket = DEFAULT_PILIGHT_SOCKET;
int pilight_mod_fd = 0;
int pilight_mod_out = 0;

unsigned short pilightModHwInit(void) {
	char *socket = NULL;

	pilight_mod_out = GPIO_OUT_PIN;	

	if(settings_find_string("hw-socket", &socket) == 0) {
		pilight_mod_socket = socket;
	}

	settings_find_number("gpio-sender", &pilight_mod_out);

	if(pilight_mod_initialized == 0) {
		if((pilight_mod_fd = open(pilight_mod_socket, O_RDONLY)) < 0) {
			logprintf(LOG_ERR, "could not open %s", pilight_mod_socket);
			return EXIT_FAILURE;
		} else {
			/* Only set the frequency once */
			logprintf(LOG_DEBUG, "initialized pilight module");
			pilight_mod_initialized = 1;
		}
	}

	if(wiringPiSetup() == -1) {
		if(pilight_mod_initialized) {
			close(pilight_mod_fd);
		}
		return EXIT_FAILURE;
	}
	pinMode(pilight_mod_out, OUTPUT);	

	return EXIT_SUCCESS;
}

unsigned short pilightModHwDeinit(void) {
	FILE *fd;

	if((fd = fopen ("/sys/class/gpio/unexport", "w"))) {
		fprintf(fd, "%d\n", wpiPinToGpio(pilight_mod_out));
		fclose(fd);
	}	

	if(pilight_mod_initialized == 1) {
		if(pilight_mod_fd != 0) {
			close(pilight_mod_fd);
			pilight_mod_fd = 0;
		}

		logprintf(LOG_DEBUG, "deinitialized pilight module");
		pilight_mod_initialized	= 0;
	}

	return EXIT_SUCCESS;
}

int pilightModSend(int *code) {
	unsigned short i = 0;

	while(code[i]) {
		digitalWrite(pilight_mod_out, 1);
		usleep((__useconds_t)code[i++]);
		digitalWrite(pilight_mod_out, 0);
		usleep((__useconds_t)code[i++]);
	}
	return 0;
}

int pilightModReceive(void) {
	char buff[255] = {0};
	if((read(pilight_mod_fd, buff, sizeof(buff))) == 0) {
		strcpy(buff, "1");
	}

	return (atoi(buff) & 0x00FFFFFF);
}

void pilightModInit(void) {

	hardware_register(&pilight_mod);
	hardware_set_id(pilight_mod, "pilight");

	pilight_mod->init=&pilightModHwInit;
	pilight_mod->deinit=&pilightModHwDeinit;
	pilight_mod->send=&pilightModSend;
	pilight_mod->receive=&pilightModReceive;
}