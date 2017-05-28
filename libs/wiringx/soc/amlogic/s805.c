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
#include <ctype.h>

#include "s805.h"
#include "../../wiringX.h"
#include "../soc.h"

struct soc_t *amlogicS805 = NULL;

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
	} out;

	struct {
		unsigned long offset;
		unsigned long bit;
	} in;

	int support;

	enum pinmode_t mode;

	int fd;

} layout[] = {
	{ "GPIOAO_0", 1, { 0x24, 0 }, { 0x24, 16 }, { 0x28, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_1", 1, { 0x24, 1 }, { 0x24, 17 }, { 0x28, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_2", 1, { 0x24, 2 }, { 0x24, 18 }, { 0x28, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_3", 1, { 0x24, 3 }, { 0x24, 19 }, { 0x28, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_4", 1, { 0x24, 4 }, { 0x24, 20 }, { 0x28, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_5", 1, { 0x24, 5 }, { 0x24, 21 }, { 0x28, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_6", 1, { 0x24, 6 }, { 0x24, 22 }, { 0x28, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_7", 1, { 0x24, 7 }, { 0x24, 23 }, { 0x28, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_8", 1, { 0x24, 8 }, { 0x24, 24 }, { 0x28, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_9", 1, { 0x24, 9 }, { 0x24, 25 }, { 0x28, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_10", 1, { 0x24, 10 }, { 0x24, 26 }, { 0x28, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_11", 1, { 0x24, 11 }, { 0x24, 27 }, { 0x28, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_12", 1, { 0x24, 12 }, { 0x24, 28 }, { 0x28, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOAO_13", 1, { 0x24, 13 }, { 0x24, 29 }, { 0x28, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_0", 0, { 0x54, 19 }, { 0x58, 19 }, { 0x5C, 19 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_1", 0, { 0x54, 20 }, { 0x58, 20 }, { 0x5C, 20 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_2", 0, { 0x54, 21 }, { 0x58, 21 }, { 0x5C, 21 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_3", 0, { 0x54, 22 }, { 0x58, 22 }, { 0x5C, 22 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_4", 0, { 0x54, 23 }, { 0x58, 23 }, { 0x5C, 23 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_5", 0, { 0x54, 24 }, { 0x58, 24 }, { 0x5C, 24 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_6", 0, { 0x54, 25 }, { 0x58, 25 }, { 0x5C, 25 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_7", 0, { 0x54, 26 }, { 0x58, 26 }, { 0x5C, 26 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_8", 0, { 0x54, 27 }, { 0x58, 27 }, { 0x5C, 27 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOH_9", 0, { 0x54, 28 }, { 0x58, 28 }, { 0x5C, 28 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_0", 0, { 0x54, 0 }, { 0x58, 0 }, { 0x5C, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_1", 0, { 0x54, 1 }, { 0x58, 1 }, { 0x5C, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_2", 0, { 0x54, 2 }, { 0x58, 2 }, { 0x5C, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_3", 0, { 0x54, 3 }, { 0x58, 3 }, { 0x5C, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_4", 0, { 0x54, 4 }, { 0x58, 4 }, { 0x5C, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_5", 0, { 0x54, 5 }, { 0x58, 5 }, { 0x5C, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_6", 0, { 0x54, 6 }, { 0x58, 6 }, { 0x5C, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_7", 0, { 0x54, 7 }, { 0x58, 7 }, { 0x5C, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_8", 0, { 0x54, 8 }, { 0x58, 8 }, { 0x5C, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_9", 0, { 0x54, 9 }, { 0x58, 9 }, { 0x5C, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_10", 0, { 0x54, 10 }, { 0x58, 10 }, { 0x5C, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_11", 0, { 0x54, 11 }, { 0x58, 11 }, { 0x5C, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_12", 0, { 0x54, 12 }, { 0x58, 12 }, { 0x5C, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_13", 0, { 0x54, 13 }, { 0x58, 13 }, { 0x5C, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_14", 0, { 0x54, 14 }, { 0x58, 14 }, { 0x5C, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_15", 0, { 0x54, 15 }, { 0x58, 15 }, { 0x5C, 15 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_16", 0, { 0x54, 16 }, { 0x58, 16 }, { 0x5C, 16 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_17", 0, { 0x54, 17 }, { 0x58, 17 }, { 0x5C, 17 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "BOOT_18", 0, { 0x54, 18 }, { 0x58, 18 }, { 0x5C, 18 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "CARD_0", 0, { 0x30, 22 }, { 0x34, 22 }, { 0x38, 22 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "CARD_1", 0, { 0x30, 23 }, { 0x34, 23 }, { 0x38, 23 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "CARD_2", 0, { 0x30, 24 }, { 0x34, 24 }, { 0x38, 24 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "CARD_3", 0, { 0x30, 25 }, { 0x34, 25 }, { 0x38, 25 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "CARD_4", 0, { 0x30, 26 }, { 0x34, 26 }, { 0x38, 26 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "CARD_5", 0, { 0x30, 27 }, { 0x34, 27 }, { 0x38, 27 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "CARD_6", 0, { 0x30, 28 }, { 0x34, 28 }, { 0x38, 28 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_0", 0, { 0x48, 0 }, { 0x4C, 0 }, { 0x50, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_1", 0, { 0x48, 1 }, { 0x4C, 1 }, { 0x50, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_2", 0, { 0x48, 2 }, { 0x4C, 2 }, { 0x50, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_3", 0, { 0x48, 3 }, { 0x4C, 3 }, { 0x50, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_4", 0, { 0x48, 4 }, { 0x4C, 4 }, { 0x50, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_5", 0, { 0x48, 5 }, { 0x4C, 5 }, { 0x50, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_6", 0, { 0x48, 6 }, { 0x4C, 6 }, { 0x50, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_7", 0, { 0x48, 7 }, { 0x4C, 7 }, { 0x50, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_8", 0, { 0x48, 8 }, { 0x4C, 8 }, { 0x50, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_9", 0, { 0x48, 9 }, { 0x4C, 9 }, { 0x50, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_10", 0, { 0x48, 10 }, { 0x4C, 10 }, { 0x50, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_11", 0, { 0x48, 11 }, { 0x4C, 11 }, { 0x50, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_12", 0, { 0x48, 12 }, { 0x4C, 12 }, { 0x50, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_13", 0, { 0x48, 13 }, { 0x4C, 13 }, { 0x50, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_14", 0, { 0x48, 14 }, { 0x4C, 14 }, { 0x50, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_15", 0, { 0x48, 15 }, { 0x4C, 15 }, { 0x50, 15 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_16", 0, { 0x48, 16 }, { 0x4C, 16 }, { 0x50, 16 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_17", 0, { 0x48, 17 }, { 0x4C, 17 }, { 0x50, 17 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_18", 0, { 0x48, 18 }, { 0x4C, 18 }, { 0x50, 18 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_19", 0, { 0x48, 19 }, { 0x4C, 19 }, { 0x50, 19 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_20", 0, { 0x48, 20 }, { 0x4C, 20 }, { 0x50, 20 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_21", 0, { 0x48, 21 }, { 0x4C, 21 }, { 0x50, 21 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_22", 0, { 0x48, 22 }, { 0x4C, 22 }, { 0x50, 22 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_23", 0, { 0x48, 23 }, { 0x4C, 23 }, { 0x50, 23 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_24", 0, { 0x48, 24 }, { 0x4C, 24 }, { 0x50, 24 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_25", 0, { 0x48, 25 }, { 0x4C, 25 }, { 0x50, 25 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_26", 0, { 0x48, 26 }, { 0x4C, 26 }, { 0x50, 26 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_27", 0, { 0x48, 27 }, { 0x4C, 27 }, { 0x50, 27 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_28", 0, { 0x48, 28 }, { 0x4C, 28 }, { 0x50, 28 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIODV_29", 0, { 0x48, 29 }, { 0x4C, 29 }, { 0x50, 29 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_0", 0, { 0x3C, 0 }, { 0x40, 0 }, { 0x44, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_1", 0, { 0x3C, 1 }, { 0x40, 1 }, { 0x44, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_2", 0, { 0x3C, 2 }, { 0x40, 2 }, { 0x44, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_3", 0, { 0x3C, 3 }, { 0x40, 3 }, { 0x44, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_4", 0, { 0x3C, 4 }, { 0x40, 4 }, { 0x44, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_5", 0, { 0x3C, 5 }, { 0x40, 5 }, { 0x44, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_6", 0, { 0x3C, 6 }, { 0x40, 6 }, { 0x44, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_7", 0, { 0x3C, 7 }, { 0x40, 7 }, { 0x44, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_8", 0, { 0x3C, 8 }, { 0x40, 8 }, { 0x44, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_9", 0, { 0x3C, 9 }, { 0x40, 9 }, { 0x44, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_10", 0, { 0x3C, 10 }, { 0x40, 10 }, { 0x44, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_11", 0, { 0x3C, 11 }, { 0x40, 11 }, { 0x44, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_12", 0, { 0x3C, 12 }, { 0x40, 12 }, { 0x44, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_13", 0, { 0x3C, 13 }, { 0x40, 13 }, { 0x44, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_14", 0, { 0x3C, 14 }, { 0x40, 14 }, { 0x44, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_15", 0, { 0x3C, 15 }, { 0x40, 15 }, { 0x44, 15 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOY_16", 0, { 0x3C, 16 }, { 0x40, 16 }, { 0x44, 16 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_0", 0, { 0x30, 0 }, { 0x34, 0 }, { 0x38, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_1", 0, { 0x30, 1 }, { 0x34, 1 }, { 0x38, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_2", 0, { 0x30, 2 }, { 0x34, 2 }, { 0x38, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_3", 0, { 0x30, 3 }, { 0x34, 3 }, { 0x38, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_4", 0, { 0x30, 4 }, { 0x34, 4 }, { 0x38, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_5", 0, { 0x30, 5 }, { 0x34, 5 }, { 0x38, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_6", 0, { 0x30, 6 }, { 0x34, 6 }, { 0x38, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_7", 0, { 0x30, 7 }, { 0x34, 7 }, { 0x38, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_8", 0, { 0x30, 8 }, { 0x34, 8 }, { 0x38, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_9", 0, { 0x30, 9 }, { 0x34, 9 }, { 0x38, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_10", 0, { 0x30, 10 }, { 0x34, 10 }, { 0x38, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_11", 0, { 0x30, 11 }, { 0x34, 11 }, { 0x38, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_12", 0, { 0x30, 12 }, { 0x34, 12 }, { 0x38, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_13", 0, { 0x30, 13 }, { 0x34, 13 }, { 0x38, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_14", 0, { 0x30, 14 }, { 0x34, 14 }, { 0x38, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_15", 0, { 0x30, 15 }, { 0x34, 15 }, { 0x38, 15 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_16", 0, { 0x30, 16 }, { 0x34, 16 }, { 0x38, 16 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_17", 0, { 0x30, 17 }, { 0x34, 17 }, { 0x38, 17 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_18", 0, { 0x30, 18 }, { 0x34, 18 }, { 0x38, 18 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_19", 0, { 0x30, 19 }, { 0x34, 19 }, { 0x38, 19 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_20", 0, { 0x30, 20 }, { 0x34, 20 }, { 0x38, 20 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIOX_21", 0, { 0x30, 21 }, { 0x34, 21 }, { 0x38, 21 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_0_P", 0, { 0x60, 12 }, { 0x64, 12 }, { 0x68, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_0_N", 0, { 0x60, 13 }, { 0x64, 13 }, { 0x68, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_1_P", 0, { 0x60, 14 }, { 0x64, 14 }, { 0x68, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_1_N", 0, { 0x60, 15 }, { 0x64, 15 }, { 0x68, 15 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_2_P", 0, { 0x60, 16 }, { 0x64, 16 }, { 0x68, 16 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_2_N", 0, { 0x60, 17 }, { 0x64, 17 }, { 0x68, 17 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_3_P", 0, { 0x60, 18 }, { 0x64, 18 }, { 0x68, 18 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_3_N", 0, { 0x60, 19 }, { 0x64, 19 }, { 0x68, 19 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_4_P", 0, { 0x60, 20 }, { 0x64, 20 }, { 0x68, 20 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "DIF_4_N", 0, { 0x60, 21 }, { 0x64, 21 }, { 0x68, 21 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
};

static int amlogicS805Setup(void) {
	if((amlogicS805->fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open /dev/mem for raw memory access");
		return -1;
	}

	if((amlogicS805->gpio[0] = (unsigned char *)mmap(0, amlogicS805->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, amlogicS805->fd, amlogicS805->base_addr[0])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}

	if((amlogicS805->gpio[1] = (unsigned char *)mmap(0, amlogicS805->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, amlogicS805->fd, amlogicS805->base_addr[1])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}

	return 0;
}

static char *amlogicS805GetPinName(int pin) {
	return amlogicS805->layout[pin].name;
}

static void amlogicS805SetMap(int *map, size_t size) {
	amlogicS805->map = map;
	amlogicS805->map_size = size;
}

static void amlogicS805SetIRQ(int *irq, size_t size) {
	amlogicS805->irq = irq;
	amlogicS805->irq_size = size;
}

static int amlogicS805DigitalWrite(int i, enum digital_value_t value) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &amlogicS805->layout[amlogicS805->map[i]];

	if(amlogicS805->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}
	if(amlogicS805->fd <= 0 || amlogicS805->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}
	if(pin->mode != PINMODE_OUTPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to output mode", amlogicS805->brand, amlogicS805->chip, i);
		return -1;
	}

	addr = (unsigned long)(amlogicS805->gpio[pin->addr] + amlogicS805->base_offs[pin->addr] + pin->out.offset);
	val = soc_readl(addr);

	if(value == HIGH) {
		soc_writel(addr, val | (1 << pin->out.bit));
	} else {
		soc_writel(addr, val & ~(1 << pin->out.bit));
	}
	return 0;
}

static int amlogicS805DigitalRead(int i) {
	void *gpio = NULL;
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &amlogicS805->layout[amlogicS805->map[i]];
	gpio = amlogicS805->gpio[pin->addr];
	addr = (unsigned long)(gpio + amlogicS805->base_offs[pin->addr] + pin->in.offset);

	if(amlogicS805->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}
	if(amlogicS805->fd <= 0 || amlogicS805->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}
	if(pin->mode != PINMODE_INPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to input mode", amlogicS805->brand, amlogicS805->chip, i);
		return -1;
	}

	val = soc_readl(addr);

	return (int)((val & (1 << pin->in.bit)) >> pin->in.bit);
}

static int amlogicS805PinMode(int i, enum pinmode_t mode) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	if(amlogicS805->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}
	if(amlogicS805->fd <= 0 || amlogicS805->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}

	pin = &amlogicS805->layout[amlogicS805->map[i]];
	addr = (unsigned long)(amlogicS805->gpio[pin->addr] + amlogicS805->base_offs[pin->addr] + pin->select.offset);
	pin->mode = mode;

	val = soc_readl(addr);
	if(mode == PINMODE_OUTPUT) {
		val &= ~(1 << pin->select.bit);
	} else if(mode == PINMODE_INPUT) {
		val |= (1 << pin->select.bit);
	}
	soc_writel(addr, val);

	return 0;
}

static int amlogicS805ISR(int i, enum isr_mode_t mode) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];

	if(amlogicS805->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}
	if(amlogicS805->fd <= 0 || amlogicS805->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}

	pin = &amlogicS805->layout[amlogicS805->irq[i]];

	sprintf(path, "/sys/class/gpio/gpio%d", amlogicS805->irq[i]);
	if((soc_sysfs_check_gpio(amlogicS805, path)) == -1) {
		sprintf(path, "/sys/class/gpio/export");
		if(soc_sysfs_gpio_export(amlogicS805, path, amlogicS805->irq[i]) == -1) {
			return -1;
		}
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", amlogicS805->irq[i]);
	if(soc_sysfs_set_gpio_direction(amlogicS805, path, "in") == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/edge", amlogicS805->irq[i]);
	if(soc_sysfs_set_gpio_interrupt_mode(amlogicS805, path, mode) == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", amlogicS805->irq[i]);
	if((pin->fd = soc_sysfs_gpio_reset_value(amlogicS805, path)) == -1) {
		return -1;
	}
	pin->mode = PINMODE_INTERRUPT;

	return 0;
}

static int amlogicS805WaitForInterrupt(int i, int ms) {
	struct layout_t *pin = &amlogicS805->layout[amlogicS805->irq[i]];

	if(pin->mode != PINMODE_INTERRUPT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to interrupt mode", amlogicS805->brand, amlogicS805->chip, i);
		return -1;
	}
	if(pin->fd <= 0) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d has not been opened for reading", amlogicS805->brand, amlogicS805->chip, i);
		return -1;
	}

	return soc_wait_for_interrupt(amlogicS805, pin->fd, ms);
}

static int amlogicS805GC(void) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int i = 0;

	if(amlogicS805->map != NULL) {
		for(i=0;i<amlogicS805->map_size;i++) {
			pin = &amlogicS805->layout[amlogicS805->map[i]];
			if(pin->mode == PINMODE_OUTPUT) {
				pinMode(i, PINMODE_INPUT);
			} else if(pin->mode == PINMODE_INTERRUPT) {
				sprintf(path, "/sys/class/gpio/gpio%d", amlogicS805->irq[i]);
				if((soc_sysfs_check_gpio(amlogicS805, path)) == 0) {
					sprintf(path, "/sys/class/gpio/unexport");
					soc_sysfs_gpio_unexport(amlogicS805, path, i);
				}
			}
			if(pin->fd > 0) {
				close(pin->fd);
				pin->fd = 0;
			}
		}
	}
	if(amlogicS805->gpio[0] != NULL) {
		munmap(amlogicS805->gpio[0], amlogicS805->page_size);
	}
	return 0;
}

static int amlogicS805SelectableFd(int i) {
	struct layout_t *pin = NULL;

	if(amlogicS805->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}
	if(amlogicS805->fd <= 0 || amlogicS805->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", amlogicS805->brand, amlogicS805->chip);
		return -1;
	}

	pin = &amlogicS805->layout[amlogicS805->irq[i]];
	return pin->fd;
}

void amlogicS805Init(void) {
	soc_register(&amlogicS805, "Amlogic", "S805");

	amlogicS805->layout = layout;

	amlogicS805->support.isr_modes = ISR_MODE_RISING | ISR_MODE_FALLING | ISR_MODE_BOTH | ISR_MODE_NONE;

	amlogicS805->page_size = (4*1024);
	amlogicS805->base_addr[0] = 0xC1108000;
	amlogicS805->base_offs[0] = 0x00000000;

	amlogicS805->base_addr[1] = 0xC8100000;
	amlogicS805->base_offs[1] = 0x00000000;

	amlogicS805->gc = &amlogicS805GC;
	amlogicS805->selectableFd = &amlogicS805SelectableFd;

	amlogicS805->pinMode = &amlogicS805PinMode;
	amlogicS805->setup = &amlogicS805Setup;
	amlogicS805->digitalRead = &amlogicS805DigitalRead;
	amlogicS805->digitalWrite = &amlogicS805DigitalWrite;
	amlogicS805->getPinName = &amlogicS805GetPinName;
	amlogicS805->setMap = &amlogicS805SetMap;
	amlogicS805->setIRQ = &amlogicS805SetIRQ;
	amlogicS805->isr = &amlogicS805ISR;
	amlogicS805->waitForInterrupt = &amlogicS805WaitForInterrupt;
}
