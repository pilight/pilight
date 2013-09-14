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

#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include "settings.h"

typedef struct hardware_t {
	char *id;

	unsigned short (*init)(void);
	unsigned short (*deinit)(void);
	int (*receive)(void);
	unsigned short (*send)(int *code);
} hardware_t;

typedef struct hardwares_t {
	struct hardware_t *listener;
	struct hardwares_t *next;
} hardwares_t;

struct hardwares_t *hardwares;

void hardware_init(void);
void hardware_register(hardware_t **hw);
int hardware_gc(void);

#endif
