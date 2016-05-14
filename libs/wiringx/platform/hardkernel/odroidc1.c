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
#include "odroidc1.h"

struct platform_t *odroidc1 = NULL;

/*
 * |-----|
 * |3v|5v|
 * | 8|5v|
 * | 9|0v|
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
 * |17|18|
 * |21|0v|
 * |22|26|
 * |23|0v|
 * |24|27|
 * |AD|2v|
 * |0v|AD|
 * |-----|
 * */

static int map[] = {
	/* 	GPIOY_8,		GPIOY_7,		GPIOX_19,		GPIOX_18	*/
			 88,				 87,				116,				115,
	/* 	GPIOX_7,		GPIOX_5,		GPIOX_6,		GPIOY_3		*/
			104,				102,				103,				 83,
	/* 	GPIODV_24,	GPIODV_25,	GPIOX_20,		GPIOX_21	*/
			 74,				 75,				117,				118,
	/* 	GPIOX_10,		GPIOX_9,		GPIOX_8,		(Padding)	*/
			107,				106,				105,				 -1,
	/*	(Padding),	GPIODV_26,	GPIODV_27,	(Padding)	*/
			 -1,				 76,				 77,				 -1,
	/*	(Padding),	GPIOX_4,		GPIOX_3,		GPIOX_11		*/
			 -1,				101,				100,				108,
	/*	GPIOX_0,	 	(Padding),	GPIOX_2,		GPIOX_1	*/
			 97,				 -1,				 99,				 98
};

static int odroidc1ValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		if(map[pin] == -1) {
			return -1;
		}
		return 0;
	} else {
		return -1;
	}
}

static int odroidc1Setup(void) {
	odroidc1->soc->setup();
	odroidc1->soc->setMap(map);
	odroidc1->soc->setIRQ(map);
	return 0;
}

void odroidc1Init(void) {
	platform_register(&odroidc1, "odroidc1");

	odroidc1->soc = soc_get("Amlogic", "S805");
	odroidc1->soc->setMap(map);

	odroidc1->digitalRead = odroidc1->soc->digitalRead;
	odroidc1->digitalWrite = odroidc1->soc->digitalWrite;
	odroidc1->pinMode = odroidc1->soc->pinMode;
	odroidc1->setup = odroidc1Setup;

	odroidc1->isr = odroidc1->soc->isr;
	odroidc1->waitForInterrupt = odroidc1->soc->waitForInterrupt;

	odroidc1->selectableFd = odroidc1->soc->selectableFd;
	odroidc1->gc = odroidc1->soc->gc;

	odroidc1->validGPIO = &odroidc1ValidGPIO;
}
