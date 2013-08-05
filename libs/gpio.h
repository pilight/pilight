/*
	Copyright (C) 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
*/

#ifndef _GPIO_H_
#define _GPIO_H_

#define HIGH	1
#define LOW		0

typedef struct {
	int nr;
	int pins[27];
} pins_t;

pins_t pins;

void gpio_register(int gpio_pin);
void gpio_deregister(int gpio_pin);
int gpio_request(int gpio_pin);
int gpio_pinmode(int gpio_pin, char mode[6]);
int gpio_free(void);
int gpio_wiringPi2BCM(int gpio_pin);

#endif