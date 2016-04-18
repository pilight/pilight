/*
	Copyright (c) 2016 Brian Kim <brian.kim@hardkernel.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __WIRINGX_S905_H_
#define __WIRINGX_S905_H_

#include "../soc.h"
#include "../../wiringX.h"

extern struct soc_t *amlogicS905;

void amlogicS905Init(void);

#endif
