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
#include "json.h"
#include "wiringPi.h"
#include "433pilight.h"

unsigned short pilight433HwInit(void) {
	return EXIT_SUCCESS;
}

unsigned short pilight433HwDeinit(void) {
	return EXIT_SUCCESS;
}

int pilight433Send(int *code) {
	sleep(1);
	return 0;
}

int pilight433Receive(void) {
	sleep(1);
	return 0;
}

unsigned short pilight433Settings(JsonNode *json) {
	return EXIT_SUCCESS;
}

void pilight433Init(void) {
}