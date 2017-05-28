/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __WIRINGX_SOC_H_
#define __WIRINGX_SOC_H_

#include "../wiringX.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_REG_AREA	8

struct layout_t;

typedef struct soc_t {
	char brand[255];
	char chip[255];

	int *map;
	size_t map_size;
	int *irq;
	size_t irq_size;

	struct layout_t *layout;

	struct {
		int isr_modes;
	} support;

	void *gpio[MAX_REG_AREA];
	int fd;

	size_t page_size;
	uintptr_t base_addr[MAX_REG_AREA];
	uintptr_t base_offs[MAX_REG_AREA];

	int (*digitalWrite)(int, enum digital_value_t);
	int (*digitalRead)(int);
	int (*pinMode)(int, enum pinmode_t);
	int (*isr)(int, enum isr_mode_t);
	int (*waitForInterrupt)(int, int);

	int (*setup)(void);
	void (*setMap)(int *, size_t size);
	void (*setIRQ)(int *, size_t size);
	char *(*getPinName)(int);

	int (*validGPIO)(int);
	int (*selectableFd)(int);
	int (*gc)(void);

	struct soc_t *next;
} soc_t;

void soc_register(struct soc_t **, char *, char *);
struct soc_t *soc_get(char *, char *);
void soc_writel(uintptr_t, uint32_t);
uint32_t soc_readl(uintptr_t);
int soc_gc(void);

int soc_sysfs_check_gpio(struct soc_t *, char *);
int soc_sysfs_gpio_export(struct soc_t *, char *, int);
int soc_sysfs_gpio_unexport(struct soc_t *, char *, int);
int soc_sysfs_set_gpio_interrupt_mode(struct soc_t *, char *, enum isr_mode_t);
int soc_sysfs_set_gpio_direction(struct soc_t *, char *, char *);
int soc_sysfs_gpio_reset_value(struct soc_t *, char *);
int soc_wait_for_interrupt(struct soc_t *, int, int);
#endif
