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
#include "odroidc2.h"

struct platform_t *odroidc2 = NULL;

/*
 * |-----|
 * |3v|5v|
 * |I2|5v|
 * |I2|0v|
 * | 7|TX|
 * |0v|RX|
 * | 0| 1|
 * | 2|0v|
 * | 3| 4|
 * |3v| 5|
 * |12|0v|
 * |13| 6|
 * |14|10|
 * |0v|11|
 * |I2|I2|
 * |21|0v|
 * |22|26|
 * |23|0v|
 * |24|27|
 * |AD|2v|
 * |0v|AD|
 * |-----|
 * */

static int map[] = {
	/* 	GPIOX_19,	GPIOX_10,	GPIOX_11,	GPIOX_9		*/
		247,		238,		239,		237,
	/* 	GPIOX_8,	GPIOX_5,	GPIOX_3,	GPIOX_21	*/
		236,		233,		231,		249,
	/* 	(Padding),	(Padding),	GPIOX_1,	GPIOY_11	*/
		-1,		-1,		229,		225,
	/* 	GPIOX_7,	GPIOX_4,	GPIOX_2,	(Padding)	*/
		235,		232,		230,		-1,
	/*	(Padding),	(Padding),	(Padding),	(Padding)	*/
		-1,		-1,		-1,		-1,
	/*	(Padding),	GPIOX_0,	GPIOY_8,	GPIOX_6		*/
		-1,		228,		219,		234,
	/*	GPIOY_3,	(Padding),	GPIOY_13,	GPIOY_7		*/
		214,		-1,		224,		218,
};

static int odroidc2ValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int odroidc2Setup(void) {
	odroidc2->soc->setup();
	odroidc2->soc->setMap(map);
	return 0;
}

void odroidc2Init(void) {
	platform_register(&odroidc2, "odroidc2");

	odroidc2->soc = soc_get("Amlogic", "S905");
	odroidc2->soc->setMap(map);

	odroidc2->digitalRead = odroidc2->soc->digitalRead;
	odroidc2->digitalWrite = odroidc2->soc->digitalWrite;
	odroidc2->pinMode = odroidc2->soc->pinMode;
	odroidc2->setup = odroidc2Setup;

	odroidc2->isr = odroidc2->soc->isr;
	odroidc2->waitForInterrupt = odroidc2->soc->waitForInterrupt;

	odroidc2->selectableFd = odroidc2->soc->selectableFd;
	odroidc2->gc = odroidc2->soc->gc;

	odroidc2->validGPIO = &odroidc2ValidGPIO;
}
