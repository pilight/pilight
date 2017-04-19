/*
	Copyright (C) 2017 Stefan Seegel
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
