/*
	Copyright (C) 2013 - 2014 Stefan Seegel

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#ifndef _HARDWARE_RASPYRFM_H_
#define _HARDWARE_RASPYRFM_H_

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "../config/hardware.h"

#define FXOSC 32E6
#define FSTEP (FXOSC / (1UL<<19))
#define FREQTOFREG(F) ((uint32_t) (F * 1E6 / FSTEP + .5)) //frequency

typedef struct {
	uint8_t spi_ch;
	uint32_t freq;
} rfm_settings_t;

extern unsigned short raspyrfmSettings(JsonNode *json, rfm_settings_t *rfmsettings);
extern unsigned short raspyrfmHwInit(rfm_settings_t *rfmsettings);
extern int raspyrfmSend(int *code, int rawlen, int repeats, rfm_settings_t *rfmsettings);

#endif
