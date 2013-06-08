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

#ifndef GC_H_
#define GC_H_

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>

typedef struct {
	int nr;
	int (*listeners[10])();
} collectors;

collectors gc;

void gc_handler(int signal);
void gc_attach(int (*fp)());
void gc_detach(int (*fp)());
void gc_catch();

#endif