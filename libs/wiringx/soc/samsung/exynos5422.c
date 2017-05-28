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

#include "exynos5422.h"
#include "../../wiringX.h"
#include "../soc.h"

struct soc_t *exynos5422 = NULL;

static struct layout_t {
	char *name;

	int addr;

	struct {
		unsigned long offset;
		unsigned long bit;
	} con;

	struct {
		unsigned long offset;
		unsigned long bit;
	} dat;

	int support;

	enum pinmode_t mode;

	int fd;

} layout[] = {
	{ "GPIO_Y70", 0, { 0x000, 0  }, { 0x004, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y71", 0, { 0x000, 4  }, { 0x004, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y72", 0, { 0x000, 8  }, { 0x004, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y73", 0, { 0x000, 12 }, { 0x004, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y74", 0, { 0x000, 16 }, { 0x004, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y75", 0, { 0x000, 20 }, { 0x004, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y76", 0, { 0x000, 24 }, { 0x004, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y77", 0, { 0x000, 28 }, { 0x004, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X00", 0, { 0xC00, 0  }, { 0xC04, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X01", 0, { 0xC00, 4  }, { 0xC04, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X02", 0, { 0xC00, 8  }, { 0xC04, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X03", 0, { 0xC00, 12 }, { 0xC04, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X04", 0, { 0xC00, 16 }, { 0xC04, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X05", 0, { 0xC00, 20 }, { 0xC04, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X06", 0, { 0xC00, 24 }, { 0xC04, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X07", 0, { 0xC00, 28 }, { 0xC04, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X10", 0, { 0xC20, 0  }, { 0xC24, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X11", 0, { 0xC20, 4  }, { 0xC24, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X12", 0, { 0xC20, 8  }, { 0xC24, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X13", 0, { 0xC20, 12 }, { 0xC24, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X14", 0, { 0xC20, 16 }, { 0xC24, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X15", 0, { 0xC20, 20 }, { 0xC24, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X16", 0, { 0xC20, 24 }, { 0xC24, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X17", 0, { 0xC20, 28 }, { 0xC24, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X20", 0, { 0xC40, 0  }, { 0xC44, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X21", 0, { 0xC40, 4  }, { 0xC44, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X22", 0, { 0xC40, 8  }, { 0xC44, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X23", 0, { 0xC40, 12 }, { 0xC44, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X24", 0, { 0xC40, 16 }, { 0xC44, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X25", 0, { 0xC40, 20 }, { 0xC44, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X26", 0, { 0xC40, 24 }, { 0xC44, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X27", 0, { 0xC40, 28 }, { 0xC44, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X30", 0, { 0xC60, 0  }, { 0xC64, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X31", 0, { 0xC60, 4  }, { 0xC64, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X32", 0, { 0xC60, 8  }, { 0xC64, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X33", 0, { 0xC60, 12 }, { 0xC64, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X34", 0, { 0xC60, 16 }, { 0xC64, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X35", 0, { 0xC60, 20 }, { 0xC64, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X36", 0, { 0xC60, 24 }, { 0xC64, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_X37", 0, { 0xC60, 28 }, { 0xC64, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C00", 1, { 0x000, 0  }, { 0x004, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C01", 1, { 0x000, 4  }, { 0x004, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C02", 1, { 0x000, 8  }, { 0x004, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C03", 1, { 0x000, 12 }, { 0x004, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C04", 1, { 0x000, 16 }, { 0x004, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C05", 1, { 0x000, 20 }, { 0x004, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C06", 1, { 0x000, 24 }, { 0x004, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C07", 1, { 0x000, 28 }, { 0x004, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C10", 1, { 0x020, 0  }, { 0x024, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C11", 1, { 0x020, 4  }, { 0x024, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C12", 1, { 0x020, 8  }, { 0x024, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C13", 1, { 0x020, 12 }, { 0x024, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C14", 1, { 0x020, 16 }, { 0x024, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C15", 1, { 0x020, 20 }, { 0x024, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C16", 1, { 0x020, 24 }, { 0x024, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C17", 1, { 0x020, 28 }, { 0x024, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C20", 1, { 0x040, 0  }, { 0x044, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C21", 1, { 0x040, 4  }, { 0x044, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C22", 1, { 0x040, 8  }, { 0x044, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C23", 1, { 0x040, 12 }, { 0x044, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C24", 1, { 0x040, 16 }, { 0x044, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C25", 1, { 0x040, 20 }, { 0x044, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C26", 1, { 0x040, 24 }, { 0x044, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C30", 1, { 0x060, 0  }, { 0x064, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C31", 1, { 0x060, 4  }, { 0x064, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C32", 1, { 0x060, 8  }, { 0x064, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C33", 1, { 0x060, 12 }, { 0x064, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C40", 1, { 0x080, 0  }, { 0x084, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_C41", 1, { 0x080, 4  }, { 0x084, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D10", 1, { 0x0A0, 0  }, { 0x0A4, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D11", 1, { 0x0A0, 4  }, { 0x0A4, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D12", 1, { 0x0A0, 8  }, { 0x0A4, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D13", 1, { 0x0A0, 12 }, { 0x0A4, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D14", 1, { 0x0A0, 16 }, { 0x0A4, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D15", 1, { 0x0A0, 20 }, { 0x0A4, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D16", 1, { 0x0A0, 24 }, { 0x0A4, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_D17", 1, { 0x0A0, 28 }, { 0x0A4, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y00", 1, { 0x0C0, 0  }, { 0x0C4, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y01", 1, { 0x0C0, 4  }, { 0x0C4, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y02", 1, { 0x0C0, 8  }, { 0x0C4, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y03", 1, { 0x0C0, 12 }, { 0x0C4, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y04", 1, { 0x0C0, 16 }, { 0x0C4, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y05", 1, { 0x0C0, 20 }, { 0x0C4, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y10", 1, { 0x0E0, 0  }, { 0x0E4, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y11", 1, { 0x0E0, 4  }, { 0x0E4, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y12", 1, { 0x0E0, 8  }, { 0x0E4, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y13", 1, { 0x0E0, 12 }, { 0x0E4, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y20", 1, { 0x100, 0  }, { 0x104, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y21", 1, { 0x100, 4  }, { 0x104, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y22", 1, { 0x100, 8  }, { 0x104, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y23", 1, { 0x100, 12 }, { 0x104, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y24", 1, { 0x100, 16 }, { 0x104, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y25", 1, { 0x100, 20 }, { 0x104, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y30", 1, { 0x120, 0  }, { 0x124, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y31", 1, { 0x120, 4  }, { 0x124, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y32", 1, { 0x120, 8  }, { 0x124, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y33", 1, { 0x120, 12 }, { 0x124, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y34", 1, { 0x120, 16 }, { 0x124, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y35", 1, { 0x120, 20 }, { 0x124, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y36", 1, { 0x120, 24 }, { 0x124, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y37", 1, { 0x120, 28 }, { 0x124, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y40", 1, { 0x140, 0  }, { 0x144, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y41", 1, { 0x140, 4  }, { 0x144, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y42", 1, { 0x140, 8  }, { 0x144, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y43", 1, { 0x140, 12 }, { 0x144, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y44", 1, { 0x140, 16 }, { 0x144, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y45", 1, { 0x140, 20 }, { 0x144, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y46", 1, { 0x140, 24 }, { 0x144, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y47", 1, { 0x140, 28 }, { 0x144, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y50", 1, { 0x160, 0  }, { 0x164, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y51", 1, { 0x160, 4  }, { 0x164, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y52", 1, { 0x160, 8  }, { 0x164, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y53", 1, { 0x160, 12 }, { 0x164, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y54", 1, { 0x160, 16 }, { 0x164, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y55", 1, { 0x160, 20 }, { 0x164, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y56", 1, { 0x160, 24 }, { 0x164, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y57", 1, { 0x160, 28 }, { 0x164, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y60", 1, { 0x180, 0  }, { 0x184, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y61", 1, { 0x180, 4  }, { 0x184, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y62", 1, { 0x180, 8  }, { 0x184, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y63", 1, { 0x180, 12 }, { 0x184, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y64", 1, { 0x180, 16 }, { 0x184, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y65", 1, { 0x180, 20 }, { 0x184, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y66", 1, { 0x180, 24 }, { 0x184, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_Y67", 1, { 0x180, 28 }, { 0x184, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "GPIO_E00", 2, { 0x000, 0  }, { 0x004, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E01", 2, { 0x000, 4  }, { 0x004, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E02", 2, { 0x000, 8  }, { 0x004, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E03", 2, { 0x000, 12 }, { 0x004, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E04", 2, { 0x000, 16 }, { 0x004, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E05", 2, { 0x000, 20 }, { 0x004, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E06", 2, { 0x000, 24 }, { 0x004, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E07", 2, { 0x000, 28 }, { 0x004, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E10", 2, { 0x020, 0  }, { 0x024, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_E11", 2, { 0x020, 4  }, { 0x024, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F00", 2, { 0x040, 0  }, { 0x044, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F01", 2, { 0x040, 4  }, { 0x044, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F02", 2, { 0x040, 8  }, { 0x044, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F03", 2, { 0x040, 12 }, { 0x044, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F04", 2, { 0x040, 16 }, { 0x044, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F05", 2, { 0x040, 20 }, { 0x044, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F10", 2, { 0x060, 0  }, { 0x064, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F11", 2, { 0x060, 4  }, { 0x064, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F12", 2, { 0x060, 8  }, { 0x064, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F13", 2, { 0x060, 12 }, { 0x064, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F14", 2, { 0x060, 16 }, { 0x064, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F15", 2, { 0x060, 20 }, { 0x064, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F16", 2, { 0x060, 24 }, { 0x064, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_F17", 2, { 0x060, 28 }, { 0x064, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G00", 2, { 0x080, 0  }, { 0x084, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G01", 2, { 0x080, 4  }, { 0x084, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G02", 2, { 0x080, 8  }, { 0x084, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G03", 2, { 0x080, 12 }, { 0x084, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G04", 2, { 0x080, 16 }, { 0x084, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G05", 2, { 0x080, 20 }, { 0x084, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G06", 2, { 0x080, 24 }, { 0x084, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G07", 2, { 0x080, 28 }, { 0x084, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G10", 2, { 0x0A0, 0  }, { 0x0A4, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G11", 2, { 0x0A0, 4  }, { 0x0A4, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G12", 2, { 0x0A0, 8  }, { 0x0A4, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G13", 2, { 0x0A0, 12 }, { 0x0A4, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G14", 2, { 0x0A0, 16 }, { 0x0A4, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G15", 2, { 0x0A0, 20 }, { 0x0A4, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G16", 2, { 0x0A0, 24 }, { 0x0A4, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G17", 2, { 0x0A0, 28 }, { 0x0A4, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G20", 2, { 0x0C0, 0  }, { 0x0C4, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_G21", 2, { 0x0C0, 4  }, { 0x0C4, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_J40", 2, { 0x0E0, 0  }, { 0x0E4, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_J41", 2, { 0x0E0, 4  }, { 0x0E4, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_J42", 2, { 0x0E0, 8  }, { 0x0E4, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_J43", 2, { 0x0E0, 12 }, { 0x0E4, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A00", 3, { 0x000, 0  }, { 0x004, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A01", 3, { 0x000, 4  }, { 0x004, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A02", 3, { 0x000, 8  }, { 0x004, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A03", 3, { 0x000, 12 }, { 0x004, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A04", 3, { 0x000, 16 }, { 0x004, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A05", 3, { 0x000, 20 }, { 0x004, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A06", 3, { 0x000, 24 }, { 0x004, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_A07", 3, { 0x000, 28 }, { 0x004, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_A10", 3, { 0x020, 0  }, { 0x024, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A11", 3, { 0x020, 4  }, { 0x024, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A12", 3, { 0x020, 8  }, { 0x024, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A13", 3, { 0x020, 12 }, { 0x024, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A14", 3, { 0x020, 16 }, { 0x024, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A15", 3, { 0x020, 20 }, { 0x024, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A20", 3, { 0x040, 0  }, { 0x044, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_A21", 3, { 0x040, 4  }, { 0x044, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_A22", 3, { 0x040, 8  }, { 0x044, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_A23", 3, { 0x040, 12 }, { 0x044, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_A24", 3, { 0x040, 16 }, { 0x044, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A25", 3, { 0x040, 20 }, { 0x044, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A26", 3, { 0x040, 24 }, { 0x044, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_A27", 3, { 0x040, 28 }, { 0x044, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B00", 3, { 0x060, 0  }, { 0x064, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B01", 3, { 0x060, 4  }, { 0x064, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B02", 3, { 0x060, 8  }, { 0x064, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B03", 3, { 0x060, 12 }, { 0x064, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B04", 3, { 0x060, 16 }, { 0x064, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B10", 3, { 0x080, 0  }, { 0x084, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B11", 3, { 0x080, 4  }, { 0x084, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B12", 3, { 0x080, 8  }, { 0x084, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B13", 3, { 0x080, 12 }, { 0x084, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B14", 3, { 0x080, 16 }, { 0x084, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B20", 3, { 0x0A0, 0  }, { 0x0A4, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B21", 3, { 0x0A0, 4  }, { 0x0A4, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_B22", 3, { 0x0A0, 8  }, { 0x0A4, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B23", 3, { 0x0A0, 12 }, { 0x0A4, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B30", 3, { 0x0C0, 0  }, { 0x0C4, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B31", 3, { 0x0C0, 4  }, { 0x0C4, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B32", 3, { 0x0C0, 8  }, { 0x0C4, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B33", 3, { 0x0C0, 12 }, { 0x0C4, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B34", 3, { 0x0C0, 16 }, { 0x0C4, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B35", 3, { 0x0C0, 20 }, { 0x0C4, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B36", 3, { 0x0C0, 24 }, { 0x0C4, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B37", 3, { 0x0C0, 28 }, { 0x0C4, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B40", 3, { 0x0E0, 0  }, { 0x0E4, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_B41", 3, { 0x0E0, 4  }, { 0x0E4, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT | FUNCTION_I2C, PINMODE_NOT_SET, 0 },
	{ "GPIO_H00", 3, { 0x100, 0  }, { 0x104, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_H01", 3, { 0x100, 4  }, { 0x104, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_H02", 3, { 0x100, 8  }, { 0x104, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_H03", 3, { 0x100, 12 }, { 0x104, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_H04", 3, { 0x100, 16 }, { 0x104, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_H05", 3, { 0x100, 20 }, { 0x104, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_H06", 3, { 0x100, 24 }, { 0x104, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_H07", 3, { 0x100, 28 }, { 0x104, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Z0", 4, { 0x000, 0  }, { 0x004, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Z1", 4, { 0x000, 4  }, { 0x004, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Z2", 4, { 0x000, 8  }, { 0x004, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Z3", 4, { 0x000, 12 }, { 0x004, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Z4", 4, { 0x000, 16 }, { 0x004, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Z5", 4, { 0x000, 20 }, { 0x004, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
	{ "GPIO_Z6", 4, { 0x000, 24 }, { 0x004, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
};

static int exynos5422Setup(void) {
	int i = 0;

	if((exynos5422->fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open /dev/mem for raw memory access");
		return -1;
	}

	/*
	 * Mapping the GPIO register areas.
	 * -> 0: 0x13400000, 1: 0x13410000, 2: 0x14000000, 3: 0x14010000, 4: 0x03860000
	 */
	for (i = 0; i < 5; ++i) {
		if((exynos5422->gpio[i] = mmap(0, exynos5422->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, exynos5422->fd, exynos5422->base_addr[i])) == MAP_FAILED) {
			wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", exynos5422->brand, exynos5422->chip);
			return -1;
		}
	}

	return 0;
}

static char *exynos5422GetPinName(int pin) {
	return exynos5422->layout[pin].name;
}

static void exynos5422SetMap(int *map, size_t size) {
	exynos5422->map = map;
	exynos5422->map_size = size;
}

static void exynos5422SetIRQ(int *irq, size_t size) {
	exynos5422->irq = irq;
	exynos5422->irq_size = size;
}

static int exynos5422DigitalWrite(int i, enum digital_value_t value) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &exynos5422->layout[exynos5422->map[i]];

	if(exynos5422->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", exynos5422->brand, exynos5422->chip);
		return -1;
	}
	if(exynos5422->fd <= 0 || exynos5422->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", exynos5422->brand, exynos5422->chip);
		return -1;
	}
	if(pin->mode != PINMODE_OUTPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to output mode", exynos5422->brand, exynos5422->chip, i);
		return -1;
	}

	addr = (unsigned long)(exynos5422->gpio[pin->addr] + exynos5422->base_offs[pin->addr] + pin->dat.offset);
	val = soc_readl(addr);

	if(value == HIGH) {
		soc_writel(addr, val | (1 << pin->dat.bit));
	} else {
		soc_writel(addr, val & ~(1 << pin->dat.bit));
	}
	return 0;
}

static int exynos5422DigitalRead(int i) {
	void *gpio = NULL;
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &exynos5422->layout[exynos5422->map[i]];
	gpio = exynos5422->gpio[pin->addr];
	addr = (unsigned long)(gpio + exynos5422->base_offs[pin->addr] + pin->dat.offset);

	if(exynos5422->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", exynos5422->brand, exynos5422->chip);
		return -1;
	}
	if(exynos5422->fd <= 0 || exynos5422->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", exynos5422->brand, exynos5422->chip);
		return -1;
	}
	if(pin->mode != PINMODE_INPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to input mode", exynos5422->brand, exynos5422->chip, i);
		return -1;
	}

	val = soc_readl(addr);

	return (int)((val & (1 << pin->dat.bit)) >> pin->dat.bit);
}

static int exynos5422PinMode(int i, enum pinmode_t mode) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	if(exynos5422->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", exynos5422->brand, exynos5422->chip);
		return -1;
	}
	if(exynos5422->fd <= 0 || exynos5422->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", exynos5422->brand, exynos5422->chip);
		return -1;
	}

	pin = &exynos5422->layout[exynos5422->map[i]];
	addr = (unsigned long)(exynos5422->gpio[pin->addr] + exynos5422->base_offs[pin->addr] + pin->con.offset);
	pin->mode = mode;

	val = soc_readl(addr);
	if(mode == PINMODE_OUTPUT) {
		val &= ~(0xF << pin->con.bit);
		val |= (0x1 << pin->con.bit);
	} else if(mode == PINMODE_INPUT) {
		val &= ~(0xF << pin->con.bit);
	}
	soc_writel(addr, val);

	return 0;
}

static int exynos5422ISR(int i, enum isr_mode_t mode) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];

	if(exynos5422->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", exynos5422->brand, exynos5422->chip);
		return -1;
	}
	if(exynos5422->fd <= 0 || exynos5422->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", exynos5422->brand, exynos5422->chip);
		return -1;
	}

	pin = &exynos5422->layout[exynos5422->irq[i]];

	sprintf(path, "/sys/class/gpio/gpio%d", exynos5422->irq[i]);
	if((soc_sysfs_check_gpio(exynos5422, path)) == -1) {
		sprintf(path, "/sys/class/gpio/export");
		if(soc_sysfs_gpio_export(exynos5422, path, exynos5422->irq[i]) == -1) {
			return -1;
		}
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", exynos5422->irq[i]);
	if(soc_sysfs_set_gpio_direction(exynos5422, path, "in") == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/edge", exynos5422->irq[i]);
	if(soc_sysfs_set_gpio_interrupt_mode(exynos5422, path, mode) == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", exynos5422->irq[i]);
	if((pin->fd = soc_sysfs_gpio_reset_value(exynos5422, path)) == -1) {
		return -1;
	}
	pin->mode = PINMODE_INTERRUPT;

	return 0;
}

static int exynos5422WaitForInterrupt(int i, int ms) {
	struct layout_t *pin = &exynos5422->layout[exynos5422->irq[i]];

	if(pin->mode != PINMODE_INTERRUPT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to interrupt mode", exynos5422->brand, exynos5422->chip, i);
		return -1;
	}
	if(pin->fd <= 0) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d has not been opened for reading", exynos5422->brand, exynos5422->chip, i);
		return -1;
	}

	return soc_wait_for_interrupt(exynos5422, pin->fd, ms);
}

static int exynos5422GC(void) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int i = 0;

	if(exynos5422->map != NULL) {
		for(i=0;i<exynos5422->map_size;i++) {
			pin = &exynos5422->layout[exynos5422->map[i]];
			if(pin->mode == PINMODE_OUTPUT) {
				pinMode(i, PINMODE_INPUT);
			} else if(pin->mode == PINMODE_INTERRUPT) {
				sprintf(path, "/sys/class/gpio/gpio%d", exynos5422->irq[i]);
				if((soc_sysfs_check_gpio(exynos5422, path)) == 0) {
					sprintf(path, "/sys/class/gpio/unexport");
					soc_sysfs_gpio_unexport(exynos5422, path, i);
				}
			}
			if(pin->fd > 0) {
				close(pin->fd);
				pin->fd = 0;
			}
		}
	}
	if(exynos5422->gpio[0] != NULL) {
		munmap(exynos5422->gpio[0], exynos5422->page_size);
	}
	return 0;
}

static int exynos5422SelectableFd(int i) {
	struct layout_t *pin = NULL;

	if(exynos5422->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", exynos5422->brand, exynos5422->chip);
		return -1;
	}
	if(exynos5422->fd <= 0 || exynos5422->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", exynos5422->brand, exynos5422->chip);
		return -1;
	}

	pin = &exynos5422->layout[exynos5422->irq[i]];
	return pin->fd;
}

void exynos5422Init(void) {
	soc_register(&exynos5422, "Samsung", "Exynos5422");

	exynos5422->layout = layout;

	exynos5422->support.isr_modes = ISR_MODE_RISING | ISR_MODE_FALLING | ISR_MODE_BOTH | ISR_MODE_NONE;

	exynos5422->page_size = (4*1024);
	exynos5422->base_addr[0] = 0x13400000;
	exynos5422->base_offs[0] = 0x00000000;

	exynos5422->base_addr[1] = 0x13410000;
	exynos5422->base_offs[1] = 0x00000000;

	exynos5422->base_addr[2] = 0x14000000;
	exynos5422->base_offs[2] = 0x00000000;

	exynos5422->base_addr[3] = 0x14010000;
	exynos5422->base_offs[3] = 0x00000000;

	exynos5422->base_addr[4] = 0x03860000;
	exynos5422->base_offs[4] = 0x00000000;

	exynos5422->gc = &exynos5422GC;
	exynos5422->selectableFd = &exynos5422SelectableFd;

	exynos5422->pinMode = &exynos5422PinMode;
	exynos5422->setup = &exynos5422Setup;
	exynos5422->digitalRead = &exynos5422DigitalRead;
	exynos5422->digitalWrite = &exynos5422DigitalWrite;
	exynos5422->getPinName = &exynos5422GetPinName;
	exynos5422->setMap = &exynos5422SetMap;
	exynos5422->setIRQ = &exynos5422SetIRQ;
	exynos5422->isr = &exynos5422ISR;
	exynos5422->waitForInterrupt = &exynos5422WaitForInterrupt;
}
