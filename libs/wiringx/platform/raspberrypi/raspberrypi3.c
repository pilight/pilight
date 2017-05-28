/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "../../soc/soc.h"
#include "../../wiringX.h"
#include "../platform.h"
#include "raspberrypi3.h"

struct platform_t *raspberrypi3 = NULL;

static int map[] = {
	/* 	FSEL17,	FSEL18,	FSEL27,	FSEL22 	*/
			17, 		18, 		27, 		22,
	/* 	FSEL23,	FSEL24,	FSEL25,	FSEL4 	*/
			23, 		24, 		25, 		 4,
	/* 	FSEL2,	FSEL3,	FSEL8,	FSEL7 	*/
			 2, 		 3, 		 8, 		 7,
	/*	FSEL10,	FSEL9,	FSEL11,	FSEL14	*/
			10,			 9,			11,			14,
	/*	FSEL15													*/
			15,			-1,			-1,			-1,
	/*					FSEL5,	FSEL6,	FSEL13	*/
			-1,			 5,			 6,			13,
	/*	FSEL19,	FSEL26,	FSEL12,	FSEL16	*/
			19,			26,			12,			16,
	/*	FSEL20,	FSEL21,	FSEL0,	FSEL1		*/
			20,			21,			 0,			 1
};

static int raspberrypi3ValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		if(map[pin] == -1) {
			return -1;
		}
		return 0;
	} else {
		return -1;
	}
}

static int raspberrypi3Setup(void) {
	const size_t size = sizeof(map) / sizeof(map[0]);
	raspberrypi3->soc->setup();
	raspberrypi3->soc->setMap(map, size);
	raspberrypi3->soc->setIRQ(map, size);
	return 0;
}

void raspberrypi3Init(void) {
	platform_register(&raspberrypi3, "raspberrypi3");

	/*
	 * The Raspberry Pi 3 uses the Broadcom 2837,
	 * but the 2837 uses the same addressen as the
	 * Broadcom 2836.
	 */
	raspberrypi3->soc = soc_get("Broadcom", "2836");
	raspberrypi3->soc->setMap(map, sizeof(map) / sizeof(map[0]));

	raspberrypi3->digitalRead = raspberrypi3->soc->digitalRead;
	raspberrypi3->digitalWrite = raspberrypi3->soc->digitalWrite;
	raspberrypi3->pinMode = raspberrypi3->soc->pinMode;
	raspberrypi3->setup = &raspberrypi3Setup;

	raspberrypi3->isr = raspberrypi3->soc->isr;
	raspberrypi3->waitForInterrupt = raspberrypi3->soc->waitForInterrupt;

	raspberrypi3->selectableFd = raspberrypi3->soc->selectableFd;
	raspberrypi3->gc = raspberrypi3->soc->gc;

	raspberrypi3->validGPIO = &raspberrypi3ValidGPIO;
}
