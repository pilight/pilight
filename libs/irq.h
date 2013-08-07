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

#include "settings.h"

#ifndef USE_LIRC

#ifndef _IRQ_H_
#define _IRQ_H_

#include <poll.h>

#define CHANGE				0
#define FALLING				1
#define RISING				2

typedef void (*func)(void);

char fn[GPIO_FN_MAXLEN];
int fd, ret;
struct pollfd pfd;
char rdbuf[RDBUF_LEN];

int irq_attach(int gpio_pin, int mode);
void irq_reset(int *fd, char *rdbuf);
int irq_poll(int *fd, char *rdbuf, struct pollfd* pfd);
int irq_read(void);
int irq_free(void);

#endif

#endif