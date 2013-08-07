/*
	Copyright (C) 2013 CurlyMo

	This file is part of the pilight.

    pilight is free software: you can redistribute it and/or modify it under the 
	terms of the GNU General Public License as published by the Free Software 
	Foundation, either version 3 of the License, or (at your option) any later 
	version.

    pilight transceiver is distributed in the hope that it will be useful, but 
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
	or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
	for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include "settings.h"

#ifndef USE_LIRC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>

#include "irq.h"
#include "gpio.h"
#include "gc.h"
#include "log.h"

struct {
	unsigned long first;
	unsigned long second;
	unsigned long period;
} timestamp;

/* Attaches an interrupt handler to a specific GPIO pin
   Whenever an rising, falling or changing interrupt occurs
   the function given as the last argument will be called */
int irq_attach(int gpio_pin, int mode) {

	gc_attach(irq_free);

	char command[50], folder[32];
	int err;
	struct stat s;

	sprintf(folder, "/sys/class/gpio/gpio%d", gpio_wiringPi2BCM(gpio_pin));
	if((err = stat(folder, &s)) != -1 && S_ISDIR(s.st_mode)) {
		if(mode == CHANGE)
			sprintf(command, "echo falling > /sys/class/gpio/gpio%d/edge", gpio_wiringPi2BCM(gpio_pin));
		else if(mode == RISING)
			sprintf(command, "echo rising > /sys/class/gpio/gpio%d/edge", gpio_wiringPi2BCM(gpio_pin));
		else if(mode == FALLING)
			sprintf(command, "echo both > /sys/class/gpio/gpio%d/edge", gpio_wiringPi2BCM(gpio_pin));

	} else {
		logprintf(LOG_ERR, "can't claim gpio pin %d", gpio_pin);
		return EXIT_FAILURE;
	}
	
	if(system(command) == -1) {
		logprintf(LOG_ERR, "can't claim gpio pin %d", gpio_pin);
		return EXIT_FAILURE;
	}
	irq_reset(&fd, rdbuf);

	memset(fn, 0x00, GPIO_FN_MAXLEN);
	snprintf(fn, GPIO_FN_MAXLEN-1, "/sys/class/gpio/gpio%d/value", gpio_wiringPi2BCM(gpio_pin));
	
	fd=open(fn, O_RDONLY);
	if(fd < 0) {
		logprintf(LOG_ERR, "can't open gpio pin %d", gpio_pin);
		return EXIT_FAILURE;
	}	
	
	pfd.fd=fd;
	pfd.events=POLLPRI;

	if(read(fd, rdbuf, RDBUF_LEN-1)<0) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int irq_free(void) {
	close(fd);
	return EXIT_SUCCESS;
}

int irq_read(void) {
	struct timeval tv;
	gettimeofday(&tv,NULL);

	irq_reset(&pfd.fd, rdbuf);
	if(irq_poll(&pfd.fd, rdbuf, &pfd)==0) {
		timestamp.first = timestamp.second;
		timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

		return (int)timestamp.second-(int)timestamp.first;
	}
	return EXIT_FAILURE;
}

/* Resets the interrupt buffer */
void irq_reset(int *f, char *buf) {
	memset(buf, 0x00, RDBUF_LEN);
	lseek(*f, 0, SEEK_SET);
}

/* Reads the interrupt buffer */
int irq_poll(int *f, char *buf, struct pollfd* p) {
	int i;
	
	i=poll((struct pollfd*)p, 1, POLL_TIMEOUT);
	if(i < 0) {
		logprintf(LOG_ERR, "failed to poll interrupt");
		close(*f);
		return EXIT_FAILURE;
	}
	
	/* Waiting... */
	if(i == 0) {
		return EXIT_SUCCESS;
	}
	
	if(read(*f, buf, RDBUF_LEN-1) < 0) {
		logprintf(LOG_ERR, "failed to read interrupt buffer");
		return EXIT_FAILURE;
	}	

	/* Interrupt */
	return EXIT_SUCCESS;
}

#endif