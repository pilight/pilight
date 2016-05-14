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
#include <ctype.h>

#include "a10.h"
#include "../../wiringX.h"
#include "../soc.h"

struct soc_t *allwinnerA10 = NULL;

static struct layout_t {
	char *name;

	int addr;

	struct {
		unsigned long offset;
		unsigned long bit;
	} select;

	struct {
		unsigned long offset;
		unsigned long bit;
	} data;

	int support;

	enum pinmode_t mode;

	int fd;

} layout[] = {
 { "PA0", 0, { 0x00, 0 }, { 0x10, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA1", 0, { 0x00, 4 }, { 0x10, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA2", 0, { 0x00, 8 }, { 0x10, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA3", 0, { 0x00, 12 }, { 0x10, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA4", 0, { 0x00, 16 }, { 0x10, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA5", 0, { 0x00, 20 }, { 0x10, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA6", 0, { 0x00, 24 }, { 0x10, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA7", 0, { 0x00, 28 }, { 0x10, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA8", 0, { 0x04, 0 }, { 0x10, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA9", 0, { 0x04, 4 }, { 0x10, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA10", 0, { 0x04, 8 }, { 0x10, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA11", 0, { 0x04, 12 }, { 0x10, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA12", 0, { 0x04, 16 }, { 0x10, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA13", 0, { 0x04, 20 }, { 0x10, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA14", 0, { 0x04, 24 }, { 0x10, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA15", 0, { 0x04, 28 }, { 0x10, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA16", 0, { 0x08, 0 }, { 0x10, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA17", 0, { 0x08, 4 }, { 0x10, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB0", 0, { 0x24, 0 }, { 0x34, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB1", 0, { 0x24, 4 }, { 0x34, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB2", 0, { 0x24, 8 }, { 0x34, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB3", 0, { 0x24, 12 }, { 0x34, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB4", 0, { 0x24, 16 }, { 0x34, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB5", 0, { 0x24, 20 }, { 0x34, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB6", 0, { 0x24, 24 }, { 0x34, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB7", 0, { 0x24, 28 }, { 0x34, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB8", 0, { 0x28, 0 }, { 0x34, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB9", 0, { 0x28, 4 }, { 0x34, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB10", 0, { 0x28, 8 }, { 0x34, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB11", 0, { 0x28, 12 }, { 0x34, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB12", 0, { 0x28, 16 }, { 0x34, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB13", 0, { 0x28, 20 }, { 0x34, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB14", 0, { 0x28, 24 }, { 0x34, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB15", 0, { 0x28, 28 }, { 0x34, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB16", 0, { 0x32, 0 }, { 0x34, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB17", 0, { 0x32, 4 }, { 0x34, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB18", 0, { 0x32, 8 }, { 0x34, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB19", 0, { 0x32, 12 }, { 0x34, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB20", 0, { 0x32, 16 }, { 0x34, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB21", 0, { 0x32, 20 }, { 0x34, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB22", 0, { 0x32, 24 }, { 0x34, 22 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB23", 0, { 0x32, 28 }, { 0x34, 23 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC0", 0, { 0x48, 0 }, { 0x58, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC1", 0, { 0x48, 4 }, { 0x58, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC2", 0, { 0x48, 8 }, { 0x58, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC3", 0, { 0x48, 12 }, { 0x58, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC4", 0, { 0x48, 16 }, { 0x58, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC5", 0, { 0x48, 20 }, { 0x58, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC6", 0, { 0x48, 24 }, { 0x58, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC7", 0, { 0x48, 28 }, { 0x58, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC8", 0, { 0x4C, 0 }, { 0x58, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC9", 0, { 0x4C, 4 }, { 0x58, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC10", 0, { 0x4C, 8 }, { 0x58, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC11", 0, { 0x4C, 12 }, { 0x58, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC12", 0, { 0x4C, 16 }, { 0x58, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC13", 0, { 0x4C, 20 }, { 0x58, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC14", 0, { 0x4C, 24 }, { 0x58, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC15", 0, { 0x4C, 28 }, { 0x58, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC16", 0, { 0x50, 0 }, { 0x58, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC17", 0, { 0x50, 4 }, { 0x58, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC18", 0, { 0x50, 8 }, { 0x58, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC19", 0, { 0x50, 12 }, { 0x58, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC20", 0, { 0x50, 16 }, { 0x58, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC21", 0, { 0x50, 20 }, { 0x58, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC22", 0, { 0x50, 24 }, { 0x58, 22 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC23", 0, { 0x50, 28 }, { 0x58, 23 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD0", 0, { 0x6C, 0 }, { 0x7C, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD1", 0, { 0x6C, 4 }, { 0x7C, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD2", 0, { 0x6C, 8 }, { 0x7C, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD3", 0, { 0x6C, 12 }, { 0x7C, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD4", 0, { 0x6C, 16 }, { 0x7C, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD5", 0, { 0x6C, 20 }, { 0x7C, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD6", 0, { 0x6C, 24 }, { 0x7C, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD7", 0, { 0x6C, 28 }, { 0x7C, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD8", 0, { 0x70, 0 }, { 0x7C, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD9", 0, { 0x70, 4 }, { 0x7C, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD10", 0, { 0x70, 8 }, { 0x7C, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD11", 0, { 0x70, 12 }, { 0x7C, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD12", 0, { 0x70, 16 }, { 0x7C, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD13", 0, { 0x70, 20 }, { 0x7C, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD14", 0, { 0x70, 24 }, { 0x7C, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD15", 0, { 0x70, 28 }, { 0x7C, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD16", 0, { 0x74, 0 }, { 0x7C, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD17", 0, { 0x74, 4 }, { 0x7C, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD18", 0, { 0x74, 8 }, { 0x7C, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD19", 0, { 0x74, 12 }, { 0x7C, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD20", 0, { 0x74, 16 }, { 0x7C, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD21", 0, { 0x74, 20 }, { 0x7C, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD22", 0, { 0x74, 24 }, { 0x7C, 22 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD23", 0, { 0x74, 28 }, { 0x7C, 23 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD24", 0, { 0x78, 0 }, { 0x7C, 24 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD25", 0, { 0x78, 4 }, { 0x7C, 25 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD26", 0, { 0x78, 8 }, { 0x7C, 26 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD27", 0, { 0x78, 12 }, { 0x7C, 27 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE0", 0, { 0x90, 0 }, { 0xA0, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE1", 0, { 0x90, 4 }, { 0xA0, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE2", 0, { 0x90, 8 }, { 0xA0, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE3", 0, { 0x90, 12 }, { 0xA0, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE4", 0, { 0x90, 16 }, { 0xA0, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE5", 0, { 0x90, 20 }, { 0xA0, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE6", 0, { 0x90, 24 }, { 0xA0, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE7", 0, { 0x90, 28 }, { 0xA0, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE8", 0, { 0x94, 0 }, { 0xA0, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE9", 0, { 0x94, 4 }, { 0xA0, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE10", 0, { 0x94, 8 }, { 0xA0, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE11", 0, { 0x94, 12 }, { 0xA0, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF0", 0, { 0xB4, 0 }, { 0xC4, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF1", 0, { 0xB4, 4 }, { 0xC4, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF2", 0, { 0xB4, 8 }, { 0xC4, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF3", 0, { 0xB4, 12 }, { 0xC4, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF4", 0, { 0xB4, 16 }, { 0xC4, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF5", 0, { 0xB4, 20 }, { 0xC4, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG0", 0, { 0xDC, 0 }, { 0xE8, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG1", 0, { 0xDC, 4 }, { 0xE8, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG2", 0, { 0xDC, 8 }, { 0xE8, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG3", 0, { 0xDC, 12 }, { 0xE8, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG4", 0, { 0xDC, 16 }, { 0xE8, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG5", 0, { 0xDC, 20 }, { 0xE8, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG6", 0, { 0xDC, 24 }, { 0xE8, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG7", 0, { 0xDC, 28 }, { 0xE8, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG8", 0, { 0xE0, 0 }, { 0xE8, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG9", 0, { 0xE0, 4 }, { 0xE8, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG10", 0, { 0xE0, 8 }, { 0xE8, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG11", 0, { 0xE0, 12 }, { 0xE8, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH0", 0, { 0xFC, 0 }, { 0x10C, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH1", 0, { 0xFC, 4 }, { 0x10C, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH2", 0, { 0xFC, 8 }, { 0x10C, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH3", 0, { 0xFC, 12 }, { 0x10C, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH4", 0, { 0xFC, 16 }, { 0x10C, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH5", 0, { 0xFC, 20 }, { 0x10C, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH6", 0, { 0xFC, 24 }, { 0x10C, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH7", 0, { 0xFC, 28 }, { 0x10C, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH8", 0, { 0x100, 0 }, { 0x10C, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH9", 0, { 0x100, 4 }, { 0x10C, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH10", 0, { 0x100, 8 }, { 0x10C, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH11", 0, { 0x100, 12 }, { 0x10C, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH12", 0, { 0x100, 16 }, { 0x10C, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH13", 0, { 0x100, 20 }, { 0x10C, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH14", 0, { 0x100, 24 }, { 0x10C, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH15", 0, { 0x100, 28 }, { 0x10C, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH16", 0, { 0x104, 0 }, { 0x10C, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH17", 0, { 0x104, 4 }, { 0x10C, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH18", 0, { 0x104, 8 }, { 0x10C, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH19", 0, { 0x104, 12 }, { 0x10C, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH20", 0, { 0x104, 16 }, { 0x10C, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH21", 0, { 0x104, 20 }, { 0x10C, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH22", 0, { 0x104, 24 }, { 0x10C, 22 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH23", 0, { 0x104, 28 }, { 0x10C, 23 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI0", 0, { 0x120, 0 }, { 0x130, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI1", 0, { 0x120, 4 }, { 0x130, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI2", 0, { 0x120, 8 }, { 0x130, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI3", 0, { 0x120, 12 }, { 0x130, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI4", 0, { 0x120, 16 }, { 0x130, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI5", 0, { 0x120, 20 }, { 0x130, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI6", 0, { 0x120, 24 }, { 0x130, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI7", 0, { 0x124, 28 }, { 0x130, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI8", 0, { 0x124, 0 }, { 0x130, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI9", 0, { 0x124, 4 }, { 0x130, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI10", 0, { 0x124, 8 }, { 0x130, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PI11", 0, { 0x124, 12 }, { 0x130, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PI12", 0, { 0x124, 16 }, { 0x130, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PI13", 0, { 0x124, 20 }, { 0x130, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PI14", 0, { 0x124, 24 }, { 0x130, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI15", 0, { 0x128, 28 }, { 0x130, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI16", 0, { 0x128, 0 }, { 0x130, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI17", 0, { 0x128, 4 }, { 0x130, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI18", 0, { 0x128, 8 }, { 0x130, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI19", 0, { 0x128, 12 }, { 0x130, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI20", 0, { 0x128, 16 }, { 0x130, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PI21", 0, { 0x128, 20 }, { 0x130, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 }
};

static int allwinnerA10Setup(void) {
	if((allwinnerA10->fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open /dev/mem for raw memory access");
		return -1;
	}

	if((allwinnerA10->gpio[0] = (unsigned char *)mmap(0, allwinnerA10->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, allwinnerA10->fd, allwinnerA10->base_addr[0])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", allwinnerA10->brand, allwinnerA10->chip);
		return -1;
	}

	return 0;
}

static char *allwinnerA10GetPinName(int pin) {
	return allwinnerA10->layout[pin].name;
}

static void allwinnerA10SetMap(int *map) {
	allwinnerA10->map = map;
}

static void allwinnerA10SetIRQ(int *irq) {
	allwinnerA10->irq = irq;
}

static int allwinnerA10DigitalWrite(int i, enum digital_value_t value) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &allwinnerA10->layout[allwinnerA10->map[i]];

	if(allwinnerA10->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA10->brand, allwinnerA10->chip);
		return -1; 
	}
	if(allwinnerA10->fd <= 0 || allwinnerA10->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA10->brand, allwinnerA10->chip);
		return -1;
	}
	if(pin->mode != PINMODE_OUTPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to output mode", allwinnerA10->brand, allwinnerA10->chip, i);
		return -1;
	}

	addr = (unsigned long)(allwinnerA10->gpio[pin->addr] + allwinnerA10->base_offs[pin->addr] + pin->data.offset);

	val = soc_readl(addr);
	if(value == HIGH) {
		soc_writel(addr, val | (1 << pin->data.bit));
	} else {
		soc_writel(addr, val & ~(1 << pin->data.bit)); 
	}
	return 0;
}

static int allwinnerA10DigitalRead(int i) {
	void *gpio = NULL;
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &allwinnerA10->layout[allwinnerA10->map[i]];
	gpio = allwinnerA10->gpio[pin->addr];
	addr = (unsigned long)(gpio + allwinnerA10->base_offs[pin->addr] + pin->select.offset);

	if(allwinnerA10->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA10->brand, allwinnerA10->chip);
		return -1; 
	}
	if(allwinnerA10->fd <= 0 || allwinnerA10->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA10->brand, allwinnerA10->chip);
		return -1;
	}
	if(pin->mode != PINMODE_INPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to input mode", allwinnerA10->brand, allwinnerA10->chip, i);
		return -1;
	}

	val = soc_readl(addr);

	return (int)((val & (1 << pin->data.bit)) >> pin->data.bit);
}

static int allwinnerA10PinMode(int i, enum pinmode_t mode) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	if(allwinnerA10->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA10->brand, allwinnerA10->chip);
		return -1; 
	} 
	if(allwinnerA10->fd <= 0 || allwinnerA10->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA10->brand, allwinnerA10->chip);
		return -1;
	}

	pin = &allwinnerA10->layout[allwinnerA10->map[i]];
	addr = (unsigned long)(allwinnerA10->gpio[pin->addr] + allwinnerA10->base_offs[pin->addr] + pin->select.offset);
	pin->mode = mode;

	val = soc_readl(addr);
	if(mode == PINMODE_OUTPUT) {
		soc_writel(addr, val | (1 << pin->select.bit));
	} else if(mode == PINMODE_INPUT) {
		soc_writel(addr, val & ~(1 << pin->select.bit));
	}
	soc_writel(addr, val & ~(1 << (pin->select.bit+1)));
	soc_writel(addr, val & ~(1 << (pin->select.bit+2)));
	return 0;
}


static int allwinnerA10ISR(int i, enum isr_mode_t mode) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int x = 0;

	if(allwinnerA10->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA10->brand, allwinnerA10->chip);
		return -1; 
	} 
	if(allwinnerA10->fd <= 0 || allwinnerA10->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA10->brand, allwinnerA10->chip);
		return -1;
	}

	pin = &allwinnerA10->layout[allwinnerA10->irq[i]];
	char name[strlen(pin->name)+1];

	memset(&name, '\0', strlen(pin->name)+1);
	for(x = 0; pin->name[x]; x++){
		name[x] = tolower(pin->name[x]);
	} 

	sprintf(path, "/sys/class/gpio/gpio%d_%s", i, name);
	if((soc_sysfs_check_gpio(allwinnerA10, path)) == -1) {
		sprintf(path, "/sys/class/gpio/export");
		if(soc_sysfs_gpio_export(allwinnerA10, path, i) == -1) {
			return -1;
		}
	}

	sprintf(path, "/sys/class/gpio/gpio%d_%s/direction", i, name); 
	if(soc_sysfs_set_gpio_direction(allwinnerA10, path, "in") == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d_%s/edge", i, name);
	if(soc_sysfs_set_gpio_interrupt_mode(allwinnerA10, path, mode) == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d_%s/value", i, name);
	if((pin->fd = soc_sysfs_gpio_reset_value(allwinnerA10, path)) == -1) {
		return -1;
	}
	pin->mode = PINMODE_INTERRUPT; 

	return 0;
}

static int allwinnerA10WaitForInterrupt(int i, int ms) {
	struct layout_t *pin = &allwinnerA10->layout[allwinnerA10->irq[i]];

	if(pin->mode != PINMODE_INTERRUPT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to interrupt mode", allwinnerA10->brand, allwinnerA10->chip, i);
		return -1;
	}
	if(pin->fd <= 0) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d has not been opened for reading", allwinnerA10->brand, allwinnerA10->chip, i);
		return -1; 
	}

	return soc_wait_for_interrupt(allwinnerA10, pin->fd, ms);
}

static int allwinnerA10GC(void) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int i = 0, l = 0, x = 0;

	if(allwinnerA10->map != NULL) {
		l = sizeof(allwinnerA10->map)/sizeof(allwinnerA10->map[0]);

		for(i=0;i<l;i++) {
			pin = &allwinnerA10->layout[allwinnerA10->map[i]];
			if(pin->mode == PINMODE_OUTPUT) {
				pinMode(i, PINMODE_INPUT);
			} else if(pin->mode == PINMODE_INTERRUPT) {
				char name[strlen(pin->name)+1];

				memset(&name, '\0', strlen(pin->name)+1);
				for(x = 0; pin->name[x]; x++){
					name[x] = tolower(pin->name[x]);
				}
				sprintf(path, "/sys/class/gpio/gpio%d_%s", i, name);
				if((soc_sysfs_check_gpio(allwinnerA10, path)) == 0) {
					sprintf(path, "/sys/class/gpio/unexport");
					soc_sysfs_gpio_unexport(allwinnerA10, path, i);
				}
			}
			if(pin->fd > 0) {
				close(pin->fd);
				pin->fd = 0;
			}
		}
	}
	if(allwinnerA10->gpio[0] != NULL) {
		munmap(allwinnerA10->gpio[0], allwinnerA10->page_size);
	} 
	return 0;
}

static int allwinnerA10SelectableFd(int i) {
	struct layout_t *pin = NULL;

	if(allwinnerA10->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA10->brand, allwinnerA10->chip);
		return -1; 
	} 
	if(allwinnerA10->fd <= 0 || allwinnerA10->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA10->brand, allwinnerA10->chip);
		return -1;
	}

	pin = &allwinnerA10->layout[allwinnerA10->irq[i]];
	return pin->fd;
}

void allwinnerA10Init(void) {
	soc_register(&allwinnerA10, "Allwinner", "A10");

	allwinnerA10->layout = layout;

	allwinnerA10->support.isr_modes = ISR_MODE_RISING | ISR_MODE_FALLING | ISR_MODE_BOTH | ISR_MODE_NONE;

	allwinnerA10->page_size = (4*1024);
	allwinnerA10->base_addr[0] = 0x01C20000;
	allwinnerA10->base_offs[0] = 0x00000800;

	allwinnerA10->gc = &allwinnerA10GC;
	allwinnerA10->selectableFd = &allwinnerA10SelectableFd;

	allwinnerA10->pinMode = &allwinnerA10PinMode;
	allwinnerA10->setup = &allwinnerA10Setup;
	allwinnerA10->digitalRead = &allwinnerA10DigitalRead;
	allwinnerA10->digitalWrite = &allwinnerA10DigitalWrite;
	allwinnerA10->getPinName = &allwinnerA10GetPinName;
	allwinnerA10->setMap = &allwinnerA10SetMap;
	allwinnerA10->setIRQ = &allwinnerA10SetIRQ;
	allwinnerA10->isr = &allwinnerA10ISR;
	allwinnerA10->waitForInterrupt = &allwinnerA10WaitForInterrupt;

}
