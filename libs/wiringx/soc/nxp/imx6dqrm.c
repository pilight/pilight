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

#include "imx6dqrm.h"
#include "../../wiringX.h"
#include "../soc.h"

struct soc_t *nxpIMX6DQRM = NULL;

#define FUNC_GPIO		0x5

static struct layout_t {
	char *name;

	int addr;

	struct {
		unsigned long offset;
		unsigned long bit;
	} data;

	struct {
		unsigned long offset;
		unsigned long bit;
	} select;

	int support;

	enum pinmode_t mode;

	int fd;

} layout[] = {
	{ "GPIO1_IO00", 0, { 0x9C000, 0x00 }, { 0xE0220, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO01", 0, { 0x9C000, 0x01 }, { 0xE0224, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO02", 0, { 0x9C000, 0x02 }, { 0xE0234, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO03", 0, { 0x9C000, 0x03 }, { 0xE022C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO04", 0, { 0x9C000, 0x04 }, { 0xE0238, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO05", 0, { 0x9C000, 0x05 }, { 0xE023C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO06", 0, { 0x9C000, 0x06 }, { 0xE0230, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO07", 0, { 0x9C000, 0x07 }, { 0xE0240, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO08", 0, { 0x9C000, 0x08 }, { 0xE0244, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO09", 0, { 0x9C000, 0x09 }, { 0xE0228, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO10", 0, { 0x9C000, 0x0A }, { 0xE0354, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO11", 0, { 0x9C000, 0x0B }, { 0xE0358, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO12", 0, { 0x9C000, 0x0C }, { 0xE035C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO13", 0, { 0x9C000, 0x0D }, { 0xE0050, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO14", 0, { 0x9C000, 0x0E }, { 0xE004C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO15", 0, { 0x9C000, 0x0D }, { 0xE0054, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO16", 0, { 0x9C000, 0x10 }, { 0xE0340, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO17", 0, { 0x9C000, 0x11 }, { 0xE033C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO18", 0, { 0x9C000, 0x12 }, { 0xE0348, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO19", 0, { 0x9C000, 0x13 }, { 0xE034C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO20", 0, { 0x9C000, 0x14 }, { 0xE0350, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO21", 0, { 0x9C000, 0x15 }, { 0xE0344, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO22", 0, { 0x9C000, 0x16 }, { 0xE01D0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO23", 0, { 0x9C000, 0x17 }, { 0xE01D4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO24", 0, { 0x9C000, 0x18 }, { 0xE01D8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO25", 0, { 0x9C000, 0x19 }, { 0xE01DC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO26", 0, { 0x9C000, 0x1A }, { 0xE01E0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO27", 0, { 0x9C000, 0x1B }, { 0xE01E4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO28", 0, { 0x9C000, 0x1C }, { 0xE01E8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO29", 0, { 0x9C000, 0x1D }, { 0xE01EC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO30", 0, { 0x9C000, 0x1E }, { 0xE01F0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO1_IO31", 0, { 0x9C000, 0x1F }, { 0xE01F4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },

	{ "GPIO2_IO00", 0, { 0xA0000, 0x00 }, { 0xE02FC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO01", 0, { 0xA0000, 0x01 }, { 0xE0300, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO02", 0, { 0xA0000, 0x02 }, { 0xE0304, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO03", 0, { 0xA0000, 0x03 }, { 0xE0308, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO04", 0, { 0xA0000, 0x04 }, { 0xE030C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO05", 0, { 0xA0000, 0x05 }, { 0xE0310, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO06", 0, { 0xA0000, 0x06 }, { 0xE0314, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO07", 0, { 0xA0000, 0x07 }, { 0xE0318, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO08", 0, { 0xA0000, 0x08 }, { 0xE031C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO09", 0, { 0xA0000, 0x09 }, { 0xE0320, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO10", 0, { 0xA0000, 0x0A }, { 0xE0324, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO11", 0, { 0xA0000, 0x0B }, { 0xE0328, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO12", 0, { 0xA0000, 0x0C }, { 0xE032C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO13", 0, { 0xA0000, 0x0D }, { 0xE0330, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO14", 0, { 0xA0000, 0x0E }, { 0xE0334, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO15", 0, { 0xA0000, 0x0F }, { 0xE0338, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO16", 0, { 0xA0000, 0x10 }, { 0xE00DC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO17", 0, { 0xA0000, 0x11 }, { 0xE00E0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO18", 0, { 0xA0000, 0x12 }, { 0xE00E4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO19", 0, { 0xA0000, 0x13 }, { 0xE00E8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO20", 0, { 0xA0000, 0x14 }, { 0xE00EC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO21", 0, { 0xA0000, 0x15 }, { 0xE00F0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO22", 0, { 0xA0000, 0x16 }, { 0xE00F4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO23", 0, { 0xA0000, 0x17 }, { 0xE00F8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO24", 0, { 0xA0000, 0x18 }, { 0xE00FC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO25", 0, { 0xA0000, 0x19 }, { 0xE0100, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO26", 0, { 0xA0000, 0x1A }, { 0xE0104, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO27", 0, { 0xA0000, 0x1B }, { 0xE0108, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO28", 0, { 0xA0000, 0x1C }, { 0xE010C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO29", 0, { 0xA0000, 0x1D }, { 0xE0110, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },	
	{ "GPIO2_IO30", 0, { 0xA0000, 0x1E }, { 0xE008C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO2_IO31", 0, { 0xA0000, 0x1F }, { 0xE00B0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },

	{ "GPIO3_IO00", 0, { 0xA4000, 0x00 }, { 0xE0114, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO01", 0, { 0xA4000, 0x01 }, { 0xE0118, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO02", 0, { 0xA4000, 0x02 }, { 0xE011C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO03", 0, { 0xA4000, 0x03 }, { 0xE0120, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO04", 0, { 0xA4000, 0x04 }, { 0xE0124, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO05", 0, { 0xA4000, 0x05 }, { 0xE0128, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO06", 0, { 0xA4000, 0x06 }, { 0xE012C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO07", 0, { 0xA4000, 0x07 }, { 0xE0130, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO08", 0, { 0xA4000, 0x08 }, { 0xE0134, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO09", 0, { 0xA4000, 0x09 }, { 0xE0138, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO10", 0, { 0xA4000, 0x0A }, { 0xE013C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO11", 0, { 0xA4000, 0x0B }, { 0xE0140, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO12", 0, { 0xA4000, 0x0C }, { 0xE0144, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO13", 0, { 0xA4000, 0x0D }, { 0xE0148, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO14", 0, { 0xA4000, 0x0E }, { 0xE014C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO15", 0, { 0xA4000, 0x0F }, { 0xE0150, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },	
	{ "GPIO3_IO16", 0, { 0xA4000, 0x10 }, { 0xE0090, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO17", 0, { 0xA4000, 0x11 }, { 0xE0094, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO18", 0, { 0xA4000, 0x12 }, { 0xE0098, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO19", 0, { 0xA4000, 0x13 }, { 0xE009C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO20", 0, { 0xA4000, 0x14 }, { 0xE00A0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO21", 0, { 0xA4000, 0x15 }, { 0xE00A4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO22", 0, { 0xA4000, 0x16 }, { 0xE00A8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO23", 0, { 0xA4000, 0x17 }, { 0xE00AC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO24", 0, { 0xA4000, 0x18 }, { 0xE00B4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO25", 0, { 0xA4000, 0x19 }, { 0xE00B8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO26", 0, { 0xA4000, 0x1A }, { 0xE00BC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO27", 0, { 0xA4000, 0x1B }, { 0xE00C0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO28", 0, { 0xA4000, 0x1C }, { 0xE00C4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO29", 0, { 0xA4000, 0x1D }, { 0xE00C8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO30", 0, { 0xA4000, 0x1E }, { 0xE00CC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO3_IO31", 0, { 0xA4000, 0x1F }, { 0xE00D0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },

	{ "GPIO4_IO05", 0, { 0xA8000, 0x05 }, { 0xE0254, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO06", 0, { 0xA8000, 0x06 }, { 0xE01F8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO07", 0, { 0xA8000, 0x07 }, { 0xE01FC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO08", 0, { 0xA8000, 0x08 }, { 0xE0200, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO09", 0, { 0xA8000, 0x09 }, { 0xE0204, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO10", 0, { 0xA8000, 0x0A }, { 0xE0208, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO11", 0, { 0xA8000, 0x0B }, { 0xE020C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO12", 0, { 0xA8000, 0x0B }, { 0xE0210, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO13", 0, { 0xA8000, 0x0C }, { 0xE0214, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO14", 0, { 0xA8000, 0x0D }, { 0xE0218, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO15", 0, { 0xA8000, 0x0E }, { 0xE021C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO16", 0, { 0xA8000, 0x10 }, { 0xE015C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO17", 0, { 0xA8000, 0x11 }, { 0xE0160, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO18", 0, { 0xA8000, 0x12 }, { 0xE0164, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO19", 0, { 0xA8000, 0x13 }, { 0xE0168, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO20", 0, { 0xA8000, 0x14 }, { 0xE016C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO21", 0, { 0xA8000, 0x15 }, { 0xE0170, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO22", 0, { 0xA8000, 0x16 }, { 0xE0174, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO23", 0, { 0xA8000, 0x17 }, { 0xE0178, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO24", 0, { 0xA8000, 0x18 }, { 0xE017C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO25", 0, { 0xA8000, 0x19 }, { 0xE0180, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO26", 0, { 0xA8000, 0x1A }, { 0xE0184, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO27", 0, { 0xA8000, 0x1B }, { 0xE0188, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO28", 0, { 0xA8000, 0x1C }, { 0xE018C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO29", 0, { 0xA8000, 0x1D }, { 0xE0190, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO30", 0, { 0xA8000, 0x1E }, { 0xE0194, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO4_IO31", 0, { 0xA8000, 0x1F }, { 0xE0198, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },

	{ "GPIO5_IO00", 0, { 0xAC000, 0x00 }, { 0xE0154, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },

	{ "GPIO5_IO02", 0, { 0xAC000, 0x02 }, { 0xE0088, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },	

	{ "GPIO5_IO04", 0, { 0xAC000, 0x04 }, { 0xE00D4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO05", 0, { 0xAC000, 0x05 }, { 0xE019C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO06", 0, { 0xAC000, 0x06 }, { 0xE01A0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO07", 0, { 0xAC000, 0x07 }, { 0xE01A4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO08", 0, { 0xAC000, 0x08 }, { 0xE01A8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO09", 0, { 0xAC000, 0x09 }, { 0xE01AC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO10", 0, { 0xAC000, 0x0A }, { 0xE01B0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO11", 0, { 0xAC000, 0x0B }, { 0xE01B4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO12", 0, { 0xAC000, 0x0C }, { 0xE01B8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO13", 0, { 0xAC000, 0x0D }, { 0xE01BC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO14", 0, { 0xAC000, 0x0E }, { 0xE01C0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO15", 0, { 0xAC000, 0x0F }, { 0xE01C4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO16", 0, { 0xAC000, 0x10 }, { 0xE01C8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO17", 0, { 0xAC000, 0x11 }, { 0xE01CC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO18", 0, { 0xAC000, 0x12 }, { 0xE0258, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO19", 0, { 0xAC000, 0x13 }, { 0xE025C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO20", 0, { 0xAC000, 0x14 }, { 0xE0260, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO21", 0, { 0xAC000, 0x15 }, { 0xE0264, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO22", 0, { 0xAC000, 0x16 }, { 0xE0268, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO23", 0, { 0xAC000, 0x17 }, { 0xE026C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO24", 0, { 0xAC000, 0x18 }, { 0xE0270, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO25", 0, { 0xAC000, 0x19 }, { 0xE0274, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO26", 0, { 0xAC000, 0x1A }, { 0xE0278, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO27", 0, { 0xAC000, 0x1B }, { 0xE027C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO28", 0, { 0xAC000, 0x1C }, { 0xE0280, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO29", 0, { 0xAC000, 0x1D }, { 0xE0284, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO30", 0, { 0xAC000, 0x1E }, { 0xE0288, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO5_IO31", 0, { 0xAC000, 0x1F }, { 0xE028C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	
	{ "GPIO6_IO00", 0, { 0xB0000, 0x00 }, { 0xE0290, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO01", 0, { 0xB0000, 0x01 }, { 0xE0294, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO02", 0, { 0xB0000, 0x02 }, { 0xE0298, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO03", 0, { 0xB0000, 0x03 }, { 0xE029C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO04", 0, { 0xB0000, 0x04 }, { 0xE02A0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO05", 0, { 0xB0000, 0x05 }, { 0xE02A4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO06", 0, { 0xB0000, 0x06 }, { 0xE00D8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO07", 0, { 0xB0000, 0x07 }, { 0xE02D4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO08", 0, { 0xB0000, 0x08 }, { 0xE02D8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO09", 0, { 0xB0000, 0x09 }, { 0xE02DC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO10", 0, { 0xB0000, 0x0A }, { 0xE02E0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO11", 0, { 0xB0000, 0x0B }, { 0xE02E4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },

	{ "GPIO6_IO14", 0, { 0xB0000, 0x0E }, { 0xE02E8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO15", 0, { 0xB0000, 0x0F }, { 0xE02EC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO16", 0, { 0xB0000, 0x10 }, { 0xE02F0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO17", 0, { 0xB0000, 0x11 }, { 0xE02A8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO18", 0, { 0xB0000, 0x12 }, { 0xE02AC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO19", 0, { 0xB0000, 0x13 }, { 0xE0058, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO20", 0, { 0xB0000, 0x14 }, { 0xE005C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO21", 0, { 0xB0000, 0x15 }, { 0xE0060, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO22", 0, { 0xB0000, 0x16 }, { 0xE0064, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO23", 0, { 0xB0000, 0x17 }, { 0xE0068, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO24", 0, { 0xB0000, 0x18 }, { 0xE006C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO25", 0, { 0xB0000, 0x19 }, { 0xE0070, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO26", 0, { 0xB0000, 0x20 }, { 0xE0074, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO27", 0, { 0xB0000, 0x21 }, { 0xE0078, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO28", 0, { 0xB0000, 0x22 }, { 0xE007C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO29", 0, { 0xB0000, 0x23 }, { 0xE0080, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO6_IO30", 0, { 0xB0000, 0x24 }, { 0xE0084, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },	
	{ "GPIO6_IO31", 0, { 0xB0000, 0x1F }, { 0xE0158, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },

	{ "GPIO7_IO00", 0, { 0xB4000, 0x00 }, { 0xE02B0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO01", 0, { 0xB4000, 0x01 }, { 0xE02B4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO02", 0, { 0xB4000, 0x02 }, { 0xE02B8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO03", 0, { 0xB4000, 0x03 }, { 0xE02BC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO04", 0, { 0xB4000, 0x04 }, { 0xE02C0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO05", 0, { 0xB4000, 0x05 }, { 0xE02C4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO06", 0, { 0xB4000, 0x06 }, { 0xE02C8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO07", 0, { 0xB4000, 0x07 }, { 0xE02CC, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO08", 0, { 0xB4000, 0x08 }, { 0xE02D0, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO09", 0, { 0xB4000, 0x09 }, { 0xE02F4, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO10", 0, { 0xB4000, 0x0A }, { 0xE02F8, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO11", 0, { 0xB4000, 0x0B }, { 0xE0248, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO12", 0, { 0xB4000, 0x0C }, { 0xE024C, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO7_IO13", 0, { 0xB4000, 0x0D }, { 0xE0250, 0x0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 }
};

static int nxpIMX6DQRMSetup(void) {
	if((nxpIMX6DQRM->fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open /dev/mem for raw memory access");
		return -1;
	}

	if((nxpIMX6DQRM->gpio[0] = (unsigned char *)mmap(0, nxpIMX6DQRM->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, nxpIMX6DQRM->fd, nxpIMX6DQRM->base_addr[0])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1;
	}

	return 0;
}

static char *nxpIMX6DQRMGetPinName(int pin) {
	return nxpIMX6DQRM->layout[pin].name;
}

static void nxpIMX6DQRMSetMap(int *map) {
	nxpIMX6DQRM->map = map;
}

static void nxpIMX6DQRMSetIRQ(int *irq) {
	nxpIMX6DQRM->irq = irq;
}

static int nxpIMX6DQRMDigitalWrite(int i, enum digital_value_t value) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	unsigned long val = 0; 

	pin = &nxpIMX6DQRM->layout[nxpIMX6DQRM->map[i]];

	if(nxpIMX6DQRM->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1; 
	}
	if(nxpIMX6DQRM->fd <= 0 || nxpIMX6DQRM->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1;
	}
	if(pin->mode != PINMODE_OUTPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to output mode", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip, i);
		return -1;
	}

	addr = (unsigned long)(nxpIMX6DQRM->gpio[pin->addr] + nxpIMX6DQRM->base_offs[pin->addr] + pin->data.offset);

	val = soc_readl(addr);
	if(value == HIGH) {
		soc_writel(addr, val | (1 << pin->data.bit));
	} else {
		soc_writel(addr, val & ~(1 << pin->data.bit)); 
	}
	return 0;
}

static int nxpIMX6DQRMDigitalRead(int i) {
	void *gpio = NULL;
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	unsigned long val = 0;

	pin = &nxpIMX6DQRM->layout[nxpIMX6DQRM->map[i]];
	gpio = nxpIMX6DQRM->gpio[pin->addr];
	addr = (unsigned long)(gpio + nxpIMX6DQRM->base_offs[pin->addr] + pin->data.offset + 8);

	if(nxpIMX6DQRM->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1; 
	}
	if(nxpIMX6DQRM->fd <= 0 || nxpIMX6DQRM->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1;
	}
	if(pin->mode != PINMODE_INPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to input mode", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip, i);
		return -1;
	}

	val = soc_readl(addr);
	
	return (int)((val & (1 << pin->data.bit)) >> pin->data.bit);
}

static int nxpIMX6DQRMPinMode(int i, enum pinmode_t mode) {
	struct layout_t *pin = NULL;
	unsigned long addrSel = 0;
	unsigned long addrDat = 0;
	unsigned long val = 0;

	if(nxpIMX6DQRM->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1; 
	} 
	if(nxpIMX6DQRM->fd <= 0 || nxpIMX6DQRM->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1;
	}

	pin = &nxpIMX6DQRM->layout[nxpIMX6DQRM->map[i]];
	addrSel = (unsigned long)(nxpIMX6DQRM->gpio[pin->addr] + nxpIMX6DQRM->base_offs[pin->addr] + pin->select.offset);
	addrDat = (unsigned long)(nxpIMX6DQRM->gpio[pin->addr] + nxpIMX6DQRM->base_offs[pin->addr] + pin->data.offset + 4);
	pin->mode = mode;

	soc_writel(addrSel, FUNC_GPIO);

	val = soc_readl(addrDat);
	if(mode == PINMODE_OUTPUT) {
		soc_writel(addrDat, val | (1 << pin->data.bit));
	} else if(mode == PINMODE_INPUT) {
		soc_writel(addrDat, val & ~(1 << pin->data.bit));
	}

	return 0;
}


static int nxpIMX6DQRMISR(int i, enum isr_mode_t mode) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];

	if(nxpIMX6DQRM->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1; 
	} 
	if(nxpIMX6DQRM->fd <= 0 || nxpIMX6DQRM->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1;
	}

	pin = &nxpIMX6DQRM->layout[nxpIMX6DQRM->map[i]];

	sprintf(path, "/sys/class/gpio/gpio%d", nxpIMX6DQRM->irq[i]);
	if((soc_sysfs_check_gpio(nxpIMX6DQRM, path)) == -1) {
		sprintf(path, "/sys/class/gpio/export");
		if(soc_sysfs_gpio_export(nxpIMX6DQRM, path, nxpIMX6DQRM->irq[i]) == -1) {
			return -1;
		}
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", nxpIMX6DQRM->irq[i]);
	if(soc_sysfs_set_gpio_direction(nxpIMX6DQRM, path, "in") == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/edge", nxpIMX6DQRM->irq[i]);
	if(soc_sysfs_set_gpio_interrupt_mode(nxpIMX6DQRM, path, mode) == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", nxpIMX6DQRM->irq[i]);
	if((pin->fd = soc_sysfs_gpio_reset_value(nxpIMX6DQRM, path)) == -1) {
		return -1;
	}
	pin->mode = PINMODE_INTERRUPT; 

	return 0;
}

static int nxpIMX6DQRMWaitForInterrupt(int i, int ms) {
	struct layout_t *pin = &nxpIMX6DQRM->layout[nxpIMX6DQRM->map[i]];

	if(pin->mode != PINMODE_INTERRUPT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to interrupt mode", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip, i);
		return -1;
	}
	if(pin->fd <= 0) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d has not been opened for reading", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip, i);
		return -1; 
	}

	return soc_wait_for_interrupt(nxpIMX6DQRM, pin->fd, ms);
}

static int nxpIMX6DQRMGC(void) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int i = 0, l = 0;

	if(nxpIMX6DQRM->map != NULL) {
		l = sizeof(nxpIMX6DQRM->map)/sizeof(nxpIMX6DQRM->map[0]);

		for(i=0;i<l;i++) {
			pin = &nxpIMX6DQRM->layout[nxpIMX6DQRM->map[i]];
			if(pin->mode == PINMODE_OUTPUT) {
				pinMode(i, PINMODE_INPUT);
			} else if(pin->mode == PINMODE_INTERRUPT) {
				sprintf(path, "/sys/class/gpio/gpio%d", nxpIMX6DQRM->irq[i]);
				if((soc_sysfs_check_gpio(nxpIMX6DQRM, path)) == 0) {
					sprintf(path, "/sys/class/gpio/unexport");
					soc_sysfs_gpio_unexport(nxpIMX6DQRM, path, i);
				}
			}
			if(pin->fd > 0) {
				close(pin->fd);
				pin->fd = 0;
			}
		}
	}
	if(nxpIMX6DQRM->gpio[0] != NULL) {
		munmap(nxpIMX6DQRM->gpio[0], nxpIMX6DQRM->page_size);
	} 
	return 0;
}

static int nxpIMX6DQRMSelectableFd(int i) {
	struct layout_t *pin = NULL;

	if(nxpIMX6DQRM->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1; 
	} 
	if(nxpIMX6DQRM->fd <= 0 || nxpIMX6DQRM->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", nxpIMX6DQRM->brand, nxpIMX6DQRM->chip);
		return -1;
	}

	pin = &nxpIMX6DQRM->layout[nxpIMX6DQRM->map[i]];
	return pin->fd;
}

void nxpIMX6DQRMInit(void) {
	nxpIMX6DQRM = malloc(sizeof(struct soc_t));

	strcpy(nxpIMX6DQRM->brand, "NXP");
	strcpy(nxpIMX6DQRM->chip, "IMX6DQRM");

	nxpIMX6DQRM->map = NULL;
	nxpIMX6DQRM->irq = NULL;
	nxpIMX6DQRM->layout = layout;

	nxpIMX6DQRM->support.isr_modes = ISR_MODE_RISING | ISR_MODE_FALLING | ISR_MODE_BOTH | ISR_MODE_NONE;

	nxpIMX6DQRM->page_size = (1024*1024);
	nxpIMX6DQRM->base_addr[0] = 0x02000000;
	nxpIMX6DQRM->base_offs[0] = 0x00000000;

	nxpIMX6DQRM->gc = &nxpIMX6DQRMGC;
	nxpIMX6DQRM->selectableFd = &nxpIMX6DQRMSelectableFd;

	nxpIMX6DQRM->pinMode = &nxpIMX6DQRMPinMode;
	nxpIMX6DQRM->setup = &nxpIMX6DQRMSetup;
	nxpIMX6DQRM->digitalRead = &nxpIMX6DQRMDigitalRead;
	nxpIMX6DQRM->digitalWrite = &nxpIMX6DQRMDigitalWrite;
	nxpIMX6DQRM->getPinName = &nxpIMX6DQRMGetPinName;
	nxpIMX6DQRM->setMap = &nxpIMX6DQRMSetMap;
	nxpIMX6DQRM->setIRQ = &nxpIMX6DQRMSetIRQ;
	nxpIMX6DQRM->isr = &nxpIMX6DQRMISR;
	nxpIMX6DQRM->waitForInterrupt = &nxpIMX6DQRMWaitForInterrupt;

	soc_register(nxpIMX6DQRM);
}
