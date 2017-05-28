/*
	Copyright (c) 2016 Brian Kim <brian.kim@hardkernel.com>

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
#include "odroidxu4.h"

struct platform_t *odroidxu4 = NULL;

/*
 * |-----|
 * |5v|0v|
 * |AD| 1|
 * | 0|16|
 * |12|15|
 * |13|14|
 * |10|PW|
 * | 2| 9|
 * | 7| 8|
 * | 3| 4|
 * |22|21|
 * |26|23|
 * |AD|11|
 * | 5| 6|
 * |27|0v|
 * |2v|0v|
 * |-----|
 *
 * -------------------
 * |36 34 33 32 2v 5v|
 * |37 35 0v 31 30 0v|
 * -------------------
 */

static int map[] = {
	/*	GPIX_A03,		GPIO_A02,		GPIO_X15,		GPIO_X16	*/
			174,				173,				 21,				 22,
	/* 	GPIO_X13,		GPIO_X17,		GPIO_X20,		GPIO_X12	*/
			 19,				 23,				 24,				 18,
	/* 	GPIO_B32,		GPIO_B33,		GPIO_A25,		GPIO_X21	*/
			209,				210,				190,				 25,
	/* 	GPIO_A27,		GPIO_A26,		GPIO_A24,		GPIO_A01	*/
			192,				191,				189,				172,
	/*	GPIO_A00,		(Padding), 	(Padding),	(Padding)	*/
			171,			 	 -1,			 	 -1,			 	 -1,
	/*	(Padding),	GPIO_X24,		GPIO_X26,		GPIO_X27	*/
			 -1,			 	 28,				 30,			 	 31,
	/*	(Padding),	(Padding),	GPIO_X25,		GPIO_X31	*/
			 -1,				 -1,				 29,				 33,
	/*	(Padding),	(Padding),	GPIO_A22,		GPIO_A23	*/
			 -1,				 -1,				187,				188,
	/*	GPIO_X32,		GPIO_Z0,		GPIO_Z1,		GPIO_Z4		*/
			 34,				225,				226,				229,
	/*	GPIO_Z2,		GPIO_Z3,		(Padding),	(Padding)	*/
			227,				228,				 -1,				 -1
};

static int odroidxu4ValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		if(map[pin] == -1) {
			return -1;
		}
		return 0;
	} else {
		return -1;
	}
}

static int odroidxu4Setup(void) {
	const size_t size = sizeof(map) / sizeof(map[0]);
	odroidxu4->soc->setup();
	odroidxu4->soc->setMap(map, size);
	odroidxu4->soc->setIRQ(map, size);
	return 0;
}

void odroidxu4Init(void) {
	platform_register(&odroidxu4, "odroidxu4");

	odroidxu4->soc = soc_get("Samsung", "Exynos5422");
	odroidxu4->soc->setMap(map, sizeof(map) / sizeof(map[0]));

	odroidxu4->digitalRead = odroidxu4->soc->digitalRead;
	odroidxu4->digitalWrite = odroidxu4->soc->digitalWrite;
	odroidxu4->pinMode = odroidxu4->soc->pinMode;
	odroidxu4->setup = odroidxu4Setup;

	odroidxu4->isr = odroidxu4->soc->isr;
	odroidxu4->waitForInterrupt = odroidxu4->soc->waitForInterrupt;

	odroidxu4->selectableFd = odroidxu4->soc->selectableFd;
	odroidxu4->gc = odroidxu4->soc->gc;

	odroidxu4->validGPIO = &odroidxu4ValidGPIO;
}
