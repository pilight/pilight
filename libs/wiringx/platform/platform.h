/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __WIRINGX_PLATFORM_H_
#define __WIRINGX_PLATFORM_H_

#include <limits.h>

#include "../wiringX.h"
#include "../soc/soc.h"

typedef struct platform_t {
	char **name;
	int nralias;
	
	struct soc_t *soc;

	int (*setup)(void);
	int (*pinMode)(int, enum pinmode_t);
	int (*analogRead)(int);
	int (*digitalWrite)(int, enum digital_value_t);
	int (*digitalRead)(int);
	int (*waitForInterrupt)(int, int);
	int (*isr)(int, enum isr_mode_t);
	int (*selectableFd)(int);

	int (*validGPIO)(int);
	int (*gc)(void);
	
	struct platform_t *next;
} platform_t;

void platform_register(struct platform_t **, char *);
void platform_add_alias(struct platform_t **, char *);
struct platform_t *platform_get_by_name(char *, int *);
struct platform_t *platform_iterate(int);
char *platform_iterate_name(int);
int platform_gc(void);

#endif