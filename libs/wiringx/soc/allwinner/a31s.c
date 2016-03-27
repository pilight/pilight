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

#include "a31s.h"
#include "../../wiringX.h"
#include "../soc.h"

struct soc_t *allwinnerA31s = NULL;

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
 { "PA18", 0, { 0x08, 8 }, { 0x10, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA19", 0, { 0x08, 12 }, { 0x10, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA20", 0, { 0x08, 16 }, { 0x10, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA21", 0, { 0x08, 20 }, { 0x10, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA22", 0, { 0x08, 24 }, { 0x10, 22 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA23", 0, { 0x08, 28 }, { 0x10, 23 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA24", 0, { 0x0C, 0 }, { 0x10, 24 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA25", 0, { 0x0C, 4 }, { 0x10, 25 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA26", 0, { 0x0C, 8 }, { 0x10, 26 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PA27", 0, { 0x0C, 12 }, { 0x10, 27 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB0", 0, { 0X24, 0 }, { 0x34, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB1", 0, { 0X24, 4 }, { 0x34, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB2", 0, { 0X24, 8 }, { 0x34, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB3", 0, { 0X24, 12 }, { 0x34, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB4", 0, { 0X24, 16 }, { 0x34, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB5", 0, { 0X24, 20 }, { 0x34, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB6", 0, { 0X24, 24 }, { 0x34, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PB7", 0, { 0X24, 28 }, { 0x34, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC0", 0, { 0X48, 0 }, { 0x58, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC1", 0, { 0X48, 4 }, { 0x58, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC2", 0, { 0X48, 8 }, { 0x58, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC3", 0, { 0X48, 12 }, { 0x58, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC4", 0, { 0X48, 16 }, { 0x58, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC5", 0, { 0X48, 20 }, { 0x58, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC6", 0, { 0X48, 24 }, { 0x58, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC7", 0, { 0X48, 28 }, { 0x58, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC8", 0, { 0X4C, 0 }, { 0x58, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC9", 0, { 0X4C, 4 }, { 0x58, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC10", 0, { 0X4C, 8 }, { 0x58, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC11", 0, { 0X4C, 12 }, { 0x58, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC12", 0, { 0x4C, 16 }, { 0x58, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC13", 0, { 0x4C, 20 }, { 0x58, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC14", 0, { 0x4C, 24 }, { 0x58, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC15", 0, { 0x4C, 28 }, { 0x58, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC24", 0, { 0x54, 0 }, { 0x58, 24 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC25", 0, { 0x54, 4 }, { 0x58, 25 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC26", 0, { 0x54, 8 }, { 0x58, 26 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC27", 0, { 0x54, 12 }, { 0x58, 27 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD0", 0, { 0X6C, 0 }, { 0x7C, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD1", 0, { 0X6C, 4 }, { 0x7C, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD2", 0, { 0X6C, 8 }, { 0x7C, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD3", 0, { 0X6C, 12 }, { 0x7C, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD4", 0, { 0X6C, 16 }, { 0x7C, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD5", 0, { 0X6C, 20 }, { 0x7C, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD6", 0, { 0X6C, 24 }, { 0x7C, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD7", 0, { 0X6C, 28 }, { 0x7C, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD8", 0, { 0X70, 0 }, { 0x7C, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD9", 0, { 0X70, 4 }, { 0x7C, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD10", 0, { 0X70, 8 }, { 0x7C, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD11", 0, { 0X70, 12 }, { 0x7C, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
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
 { "PE0", 0, { 0X90, 0 }, { 0xA0, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE1", 0, { 0X90, 4 }, { 0xA0, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE2", 0, { 0X90, 8 }, { 0xA0, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE3", 0, { 0X90, 12 }, { 0xA0, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE4", 0, { 0X90, 16 }, { 0xA0, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE5", 0, { 0X90, 20 }, { 0xA0, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE6", 0, { 0X90, 24 }, { 0xA0, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE7", 0, { 0X90, 28 }, { 0xA0, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE8", 0, { 0X94, 0 }, { 0xA0, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE9", 0, { 0X94, 4 }, { 0xA0, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE10", 0, { 0X94, 8 }, { 0xA0, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE11", 0, { 0X94, 12 }, { 0xA0, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE12", 0, { 0X94, 16 }, { 0xA0, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE13", 0, { 0X94, 20 }, { 0xA0, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE14", 0, { 0X94, 24 }, { 0xA0, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE15", 0, { 0X94, 28 }, { 0xA0, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF0", 0, { 0XB4, 0 }, { 0xC4, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF1", 0, { 0XB4, 4 }, { 0xC4, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF2", 0, { 0XB4, 8 }, { 0xC4, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF3", 0, { 0XB4, 12 }, { 0xC4, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF4", 0, { 0XB4, 16 }, { 0xC4, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF5", 0, { 0XB4, 20 }, { 0xC4, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG0", 0, { 0XD8, 0 }, { 0xE8, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG1", 0, { 0XD8, 4 }, { 0xE8, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG2", 0, { 0XD8, 8 }, { 0xE8, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG3", 0, { 0XD8, 12 }, { 0xE8, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG4", 0, { 0XD8, 16 }, { 0xE8, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG5", 0, { 0XD8, 20 }, { 0xE8, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG6", 0, { 0XD8, 24 }, { 0xE8, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG7", 0, { 0XD8, 28 }, { 0xE8, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG8", 0, { 0XDC, 0 }, { 0xE8, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG9", 0, { 0XDC, 4 }, { 0xE8, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG10", 0, { 0XDC, 8 }, { 0xE8, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG11", 0, { 0XDC, 12 }, { 0xE8, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG12", 0, { 0XDC, 16 }, { 0xE8, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG13", 0, { 0XDC, 20 }, { 0xE8, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG14", 0, { 0XDC, 24 }, { 0xE8, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG15", 0, { 0XDC, 28 }, { 0xE8, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG16", 0, { 0XE0, 0 }, { 0xE8, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG17", 0, { 0XE0, 4 }, { 0xE8, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG18", 0, { 0XE0, 8 }, { 0xE8, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH9", 0, { 0X100, 4 }, { 0x10C, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH10", 0, { 0x100, 8 }, { 0x10C, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH11", 0, { 0x100, 12 }, { 0x10C, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH12", 0, { 0x100, 16 }, { 0x10C, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH13", 0, { 0x100, 20 }, { 0x10C, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH14", 0, { 0x100, 24 }, { 0x10C, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PH15", 0, { 0X100, 28 }, { 0x10C, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH16", 0, { 0x104, 0 }, { 0x10C, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH17", 0, { 0x104, 4 }, { 0x10C, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH18", 0, { 0x104, 8 }, { 0x10C, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH19", 0, { 0x104, 12 }, { 0x10C, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH20", 0, { 0x104, 16 }, { 0x10C, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH21", 0, { 0x104, 20 }, { 0x10C, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH22", 0, { 0x104, 24 }, { 0x10C, 22 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH23", 0, { 0x104, 28 }, { 0x10C, 23 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH24", 0, { 0x108, 0 }, { 0x10C, 24 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH25", 0, { 0x108, 4 }, { 0x10C, 25 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH26", 0, { 0x108, 8 }, { 0x10C, 26 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH27", 0, { 0x108, 12 }, { 0x10C, 27 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PH28", 0, { 0x108, 16 }, { 0x10C, 28 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL0", 1, { 0X0, 0 }, { 0x10, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL1", 1, { 0X0, 4 }, { 0x10, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL2", 1, { 0X0, 8 }, { 0x10, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL3", 1, { 0X0, 12 }, { 0x10, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL4", 1, { 0X0, 16 }, { 0x10, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL5", 1, { 0X0, 20 }, { 0x10, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL6", 1, { 0X0, 24 }, { 0x10, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL7", 1, { 0X0, 28 }, { 0x10, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM0", 1, { 0X24, 0 }, { 0x34, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM1", 1, { 0X24, 4 }, { 0x34, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM2", 1, { 0X24, 8 }, { 0x34, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM3", 1, { 0X24, 12 }, { 0x34, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM4", 1, { 0X24, 16 }, { 0x34, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM5", 1, { 0X24, 20 }, { 0x34, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM6", 1, { 0X24, 24 }, { 0x34, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PM7", 1, { 0X24, 28 }, { 0x34, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 }
};

static int allwinnerA31sSetup(void) {
	if((allwinnerA31s->fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open /dev/mem for raw memory access");
		return -1;
	}

	if((allwinnerA31s->gpio[0] = (unsigned char *)mmap(0, allwinnerA31s->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, allwinnerA31s->fd, allwinnerA31s->base_addr[0])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1;
	}

	if((allwinnerA31s->gpio[1] = (unsigned char *)mmap(0, allwinnerA31s->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, allwinnerA31s->fd, allwinnerA31s->base_addr[1])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1;
	}	

	return 0;
}

static char *allwinnerA31sGetPinName(int pin) {
	return allwinnerA31s->layout[pin].name;
}

static void allwinnerA31sSetMap(int *map) {
	allwinnerA31s->map = map;
}

static int allwinnerA31sDigitalWrite(int i, enum digital_value_t value) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	unsigned long val = 0; 

	pin = &allwinnerA31s->layout[allwinnerA31s->map[i]];

	if(allwinnerA31s->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1; 
	}
	if(allwinnerA31s->fd <= 0 || allwinnerA31s->gpio[pin->addr] == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1;
	}
	if(pin->mode != PINMODE_OUTPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to output mode", allwinnerA31s->brand, allwinnerA31s->chip, i);
		return -1;
	}

	addr = (unsigned long)(allwinnerA31s->gpio[pin->addr] + allwinnerA31s->base_offs[pin->addr] + pin->data.offset);

	val = soc_readl(addr);
	if(value == HIGH) {
		soc_writel(addr, val | (1 << pin->data.bit));
	} else {
		soc_writel(addr, val & ~(1 << pin->data.bit)); 
	}
	return 0;
}

static int allwinnerA31sDigitalRead(int i) {
	void *gpio = NULL;
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	unsigned long val = 0;

	pin = &allwinnerA31s->layout[allwinnerA31s->map[i]];
	gpio = allwinnerA31s->gpio[pin->addr];
	addr = (unsigned long)(gpio + allwinnerA31s->base_offs[pin->addr] + pin->select.offset);

	if(allwinnerA31s->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1; 
	}
	if(allwinnerA31s->fd <= 0 || allwinnerA31s->gpio[pin->addr] == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1;
	}
	if(pin->mode != PINMODE_INPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to input mode", allwinnerA31s->brand, allwinnerA31s->chip, i);
		return -1;
	}

	val = soc_readl(addr);

	return (int)((val & (1 << pin->data.bit)) >> pin->data.bit);
}

static int allwinnerA31sPinMode(int i, enum pinmode_t mode) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	unsigned long val = 0;

	if(allwinnerA31s->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1; 
	} 
	pin = &allwinnerA31s->layout[allwinnerA31s->map[i]];
	if(allwinnerA31s->fd <= 0 || allwinnerA31s->gpio[pin->addr] == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerA31s->brand, allwinnerA31s->chip);
		return -1;
	}

	addr = (unsigned long)(allwinnerA31s->gpio[pin->addr] + allwinnerA31s->base_offs[pin->addr] + pin->select.offset);
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

static int allwinnerA31sGC(void) {
	struct layout_t *pin = NULL;
	int i = 0, l = 0;

	if(allwinnerA31s->map != NULL) {
		l = sizeof(allwinnerA31s->map)/sizeof(allwinnerA31s->map[0]);

		for(i=0;i<l;i++) {
			pin = &allwinnerA31s->layout[allwinnerA31s->map[i]];
			if(pin->mode == PINMODE_OUTPUT) {
				pinMode(i, PINMODE_INPUT);
			}
			if(pin->fd > 0) {
				close(pin->fd);
				pin->fd = 0;
			}
		}
	}
	if(allwinnerA31s->gpio[0] != NULL) {
		munmap(allwinnerA31s->gpio[0], allwinnerA31s->page_size);
	}
	if(allwinnerA31s->gpio[1] != NULL) {
		munmap(allwinnerA31s->gpio[1], allwinnerA31s->page_size);
	} 
	return 0;
}

void allwinnerA31sInit(void) {
	soc_register(&allwinnerA31s, "Allwinner", "A31s");

	allwinnerA31s->layout = layout;

	allwinnerA31s->support.isr_modes = ISR_MODE_RISING | ISR_MODE_FALLING | ISR_MODE_BOTH | ISR_MODE_NONE;

	allwinnerA31s->page_size = (4*1024);
	allwinnerA31s->base_addr[0] = 0x01C20000;
	allwinnerA31s->base_offs[0] = 0x00000800;

	allwinnerA31s->base_addr[1] = 0x01F02000;
	allwinnerA31s->base_offs[1] = 0x00000C00;

	allwinnerA31s->gc = &allwinnerA31sGC;

	allwinnerA31s->pinMode = &allwinnerA31sPinMode;
	allwinnerA31s->setup = &allwinnerA31sSetup;
	allwinnerA31s->digitalRead = &allwinnerA31sDigitalRead;
	allwinnerA31s->digitalWrite = &allwinnerA31sDigitalWrite;
	allwinnerA31s->getPinName = &allwinnerA31sGetPinName;
	allwinnerA31s->setMap = &allwinnerA31sSetMap;
}
