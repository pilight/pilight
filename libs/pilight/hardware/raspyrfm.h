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

struct hardware_t *raspyrfm;
void raspyrfmInit(void);

#endif
