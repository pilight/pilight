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
#include "orangepipc+.h"

struct platform_t *orangepipcp = NULL;

/*
3v|5v
 8|5v
 9|0v
 7|15
0v|16
 0| 1
 2|0v
 3| 4
3v| 5
12|0v
13| 6
14|10
0v|11
0v|0v
21|0v
22|26
23|0v
24|27
25|28
0v|29

0v
17|PA5
18|PA4

LED RED = PA15
LED GREEN = PL10
*/

static int irq[] = {
 /*  0,   1,   2,   3 */
     1,  -1,   0,   3,
 /*  4,   5,   6,   7 */
    -1,  -1,   2,   6,
 /*  8,   9,  10,  11 */
	  12,  11,  -1,  21,
 /* 12,  13,  14,  15 */
    -1,  -1,  -1,  13,
 /* 16,  17,  18,  19 (LED) */
    14,   5,   4,  -1,
 /* 20,  21,  22,  23 */
    -1,   7,   8,   9,
 /* 24,  25,  26,  27 */
    10,  20, 200, 201,
 /* 28,  29 */
   198, 199
};

static int map[] = {
 /*  PA1, PD14,  PA0,  PA3 */
       1,   55,    0,    3,
 /*  PC4,  PC7,  PA2,  PA6 */
      26,   29,    2,    6,
 /* PA12, PA11,  PC3, PA21 */
      12,   11,   25,   21,
 /*  PC0,  PC1,  PC2, PA13 */
      22,   23,   24,   13,
 /* PA14,  PA5,  PA4, PA15 */
      14,    5,    4,   15,
 /* PL10,  PA7,  PA8,  PA9 */
     106,    7,    8,    9,
 /* PA10, PA20,  PG8,  PG9 */
      10,   20,   90,   91,
 /*  PG6,  PG7 */
      88,   89
};

static int orangepipcpValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		if(map[pin] == -1) {
			return -1;
		}
		return 0;
	} else {
		return -1;
	}
}

static int orangepipcpPinMode(int i, enum pinmode_t mode) {
	if(map[i] == -1) {
		return -1;
	}
	return orangepipcp->soc->pinMode(i, mode);
}

static int orangepipcpDigitalWrite(int i, enum digital_value_t value) {
	if(map[i] == -1) {
		return -1;
	}
	return orangepipcp->soc->digitalWrite(i, value);
}

static int orangepipcpDigitalRead(int i) {
	/* Red LED - Green LED */
	if(i == 19 || i == 20) {
		return -1;
	}
	return orangepipcp->soc->digitalRead(i);
}

static int orangepipcpSetup(void) {
	const size_t msize = sizeof(map) / sizeof(map[0]);
	const size_t qsize = sizeof(irq) / sizeof(irq[0]);
	orangepipcp->soc->setup();
	orangepipcp->soc->setMap(map, msize);
	orangepipcp->soc->setIRQ(irq, qsize);
	return 0;
}

static int orangepipcpISR(int i, enum isr_mode_t mode) {
	if(irq[i] == -1) {
		return -1;
	}
	orangepipcp->soc->isr(i, mode);
	return 0;
}

void orangepipcpInit(void) {
	platform_register(&orangepipcp, "orangepipc+");

	orangepipcp->soc = soc_get("Allwinner", "H3");

	orangepipcp->digitalRead = &orangepipcpDigitalRead;
	orangepipcp->digitalWrite = &orangepipcpDigitalWrite;
	orangepipcp->pinMode = &orangepipcpPinMode;
	orangepipcp->setup = &orangepipcpSetup;

	orangepipcp->isr = &orangepipcpISR;
	orangepipcp->waitForInterrupt = orangepipcp->soc->waitForInterrupt;

	orangepipcp->selectableFd = orangepipcp->soc->selectableFd;
	orangepipcp->gc = orangepipcp->soc->gc;

	orangepipcp->validGPIO = &orangepipcpValidGPIO;
}
