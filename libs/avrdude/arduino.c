/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2009 Lars Immisch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* $Id: arduino.c 1294 2014-03-12 23:03:18Z joerg_wunsch $ */

/*
 * avrdude interface for Arduino programmer
 *
 * The Arduino programmer is mostly a STK500v1, just the signature bytes
 * are read differently.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "avrdude.h"
#include "pgm.h"
#include "stk500_private.h"
#include "stk500.h"
#include "serial.h"
#include "arduino.h"

static int arduino_open(PROGRAMMER * pgm, char * port) {
	union pinfo pinfo;
	strcpy(pgm->port, port);
	pinfo.baud = pgm->baudrate? pgm->baudrate: 115200;

	if(serial_open(port, pinfo, &pgm->fd) == -1) {
		return -1;
	}

	/* Clear DTR and RTS to unload the RESET capacitor
	* (for example in Arduino) */
	serial_set_dtr_rts(&pgm->fd, 0);
	usleep(250*1000);
	/* Set DTR and RTS back to high */
	serial_set_dtr_rts(&pgm->fd, 1);
	usleep(50*1000);

	/*
	* drain any extraneous input
	*/
	stk500_drain(pgm, 0);

	if(stk500_getsync(pgm) < 0) {
		return -1;
	}

	return 0;
}

static void arduino_close(PROGRAMMER * pgm) {
	serial_set_dtr_rts(&pgm->fd, 0);
	serial_close(&pgm->fd);
	pgm->fd.ifd = -1;
}

const char arduino_desc[] = "Arduino programmer";

void arduino_initpgm(PROGRAMMER * pgm) {
	/* This is mostly a STK500; just the signature is read
		differently than on real STK500v1
		and the DTR signal is set when opening the serial port
		for the Auto-Reset feature */
	stk500_initpgm(pgm);

	strcpy(pgm->type, "Arduino");
	pgm->open = arduino_open;
	pgm->close = arduino_close;
}
