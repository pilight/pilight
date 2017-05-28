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
#include <stdio.h>
#include <stdlib.h>

#include "a10.h"
#include "../../wiringX.h"
#include "../soc.h"

struct soc_t *allwinnerH3 = NULL;

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
 { "PA0", 0, { 0x00, 0 }, { 0x10, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA1", 0, { 0x00, 4 }, { 0x10, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA2", 0, { 0x00, 8 }, { 0x10, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA3", 0, { 0x00, 12 }, { 0x10, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA4", 0, { 0x00, 16 }, { 0x10, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA5", 0, { 0x00, 20 }, { 0x10, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA6", 0, { 0x00, 24 }, { 0x10, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA7", 0, { 0x00, 28 }, { 0x10, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA8", 0, { 0x04, 0 }, { 0x10, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA9", 0, { 0x04, 4 }, { 0x10, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA10", 0, { 0x04, 8 }, { 0x10, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA11", 0, { 0x04, 12 }, { 0x10, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA12", 0, { 0x04, 16 }, { 0x10, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA13", 0, { 0x04, 20 }, { 0x10, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA14", 0, { 0x04, 24 }, { 0x10, 14 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA15", 0, { 0x04, 28 }, { 0x10, 15 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA16", 0, { 0x08, 0 }, { 0x10, 16 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA17", 0, { 0x08, 4 }, { 0x10, 17 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA18", 0, { 0x08, 8 }, { 0x10, 18 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA19", 0, { 0x08, 12 }, { 0x10, 19 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA20", 0, { 0x08, 16 }, { 0x10, 20 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PA21", 0, { 0x08, 20 }, { 0x10, 21 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PC0", 0, { 0x48, 0 }, { 0x58, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC1", 0, { 0x48, 4 }, { 0x58, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC2", 0, { 0x48, 8 }, { 0x58, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC3", 0, { 0x48, 12 }, { 0x58, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC4", 0, { 0x48, 16 }, { 0x58, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC5", 0, { 0x48, 20 }, { 0x58, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC6", 0, { 0x48, 24 }, { 0x58, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC7", 0, { 0x48, 28 }, { 0x58, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC8", 0, { 0x4C, 0 }, { 0x58, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC9", 0, { 0x4C, 4 }, { 0x58, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC10", 0, { 0x4C, 8 }, { 0x58, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC11", 0, { 0x4C, 12 }, { 0x58, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC12", 0, { 0x4C, 16 }, { 0x58, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC13", 0, { 0x4C, 20 }, { 0x58, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC14", 0, { 0x4C, 24 }, { 0x58, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC15", 0, { 0x4C, 28 }, { 0x58, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC16", 0, { 0x50, 0 }, { 0x58, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC17", 0, { 0x50, 4 }, { 0x58, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PC18", 0, { 0x50, 8 }, { 0x58, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD0", 0, { 0x6C, 0 }, { 0x7C, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD1", 0, { 0x6C, 4 }, { 0x7C, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD2", 0, { 0x6C, 8 }, { 0x7C, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD3", 0, { 0x6C, 12 }, { 0x7C, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD4", 0, { 0x6C, 16 }, { 0x7C, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD5", 0, { 0x6C, 20 }, { 0x7C, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD6", 0, { 0x6C, 24 }, { 0x7C, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD7", 0, { 0x6C, 28 }, { 0x7C, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD8", 0, { 0x70, 0 }, { 0x7C, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD9", 0, { 0x70, 4 }, { 0x7C, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD10", 0, { 0x70, 8 }, { 0x7C, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD11", 0, { 0x70, 12 }, { 0x7C, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD12", 0, { 0x70, 16 }, { 0x7C, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD13", 0, { 0x70, 20 }, { 0x7C, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD14", 0, { 0x70, 24 }, { 0x7C, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD15", 0, { 0x70, 28 }, { 0x7C, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD16", 0, { 0x74, 0 }, { 0x7C, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PD17", 0, { 0x74, 4 }, { 0x7C, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE0", 0, { 0x90, 0 }, { 0xA0, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE1", 0, { 0x90, 4 }, { 0xA0, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE2", 0, { 0x90, 8 }, { 0xA0, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE3", 0, { 0x90, 12 }, { 0xA0, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE4", 0, { 0x90, 16 }, { 0xA0, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE5", 0, { 0x90, 20 }, { 0xA0, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE6", 0, { 0x90, 24 }, { 0xA0, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE7", 0, { 0x90, 28 }, { 0xA0, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE8", 0, { 0x94, 0 }, { 0xA0, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE9", 0, { 0x94, 4 }, { 0xA0, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE10", 0, { 0x94, 8 }, { 0xA0, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE11", 0, { 0x94, 12 }, { 0xA0, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE12", 0, { 0x94, 16 }, { 0xA0, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE13", 0, { 0x94, 20 }, { 0xA0, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE14", 0, { 0x94, 24 }, { 0xA0, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PE15", 0, { 0x94, 28 }, { 0xA0, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF0", 0, { 0xB4, 0 }, { 0xC4, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF1", 0, { 0xB4, 4 }, { 0xC4, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF2", 0, { 0xB4, 8 }, { 0xC4, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF3", 0, { 0xB4, 12 }, { 0xC4, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF4", 0, { 0xB4, 16 }, { 0xC4, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF5", 0, { 0xB4, 20 }, { 0xC4, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PF6", 0, { 0xB4, 24 }, { 0xC4, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PG0", 0, { 0xD8, 0 }, { 0xE8, 0 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG1", 0, { 0xD8, 4 }, { 0xE8, 1 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG2", 0, { 0xD8, 8 }, { 0xE8, 2 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG3", 0, { 0xD8, 12 }, { 0xE8, 3 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG4", 0, { 0xD8, 16 }, { 0xE8, 4 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG5", 0, { 0xD8, 20 }, { 0xE8, 5 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG6", 0, { 0xD8, 24 }, { 0xE8, 6 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG7", 0, { 0xD8, 28 }, { 0xE8, 7 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG8", 0, { 0xDC, 0 }, { 0xE8, 8 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG9", 0, { 0xDC, 4 }, { 0xE8, 9 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG10", 0, { 0xDC, 8 }, { 0xE8, 10 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG11", 0, { 0xDC, 12 }, { 0xE8, 11 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG12", 0, { 0xDC, 12 }, { 0xE8, 12 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PG13", 0, { 0xDC, 12 }, { 0xE8, 13 }, FUNCTION_DIGITAL | FUNCTION_INTERRUPT, PINMODE_NOT_SET, 0 },
 { "PL0", 1, { 0x00, 0 }, { 0x10, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL1", 1, { 0x00, 4 }, { 0x10, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL2", 1, { 0x00, 8 }, { 0x10, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL3", 1, { 0x00, 12 }, { 0x10, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL4", 1, { 0x00, 16 }, { 0x10, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL5", 1, { 0x00, 20 }, { 0x10, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL6", 1, { 0x00, 24 }, { 0x10, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL7", 1, { 0x00, 28 }, { 0x10, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL8", 1, { 0x04, 0 }, { 0x10, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL9", 1, { 0x04, 4 }, { 0x10, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL10", 1, { 0x04, 8 }, { 0x10, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
 { "PL11", 1, { 0x04, 12 }, { 0x10, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
};

static int allwinnerH3Setup(void) {
	if((allwinnerH3->fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open /dev/mem for raw memory access");
		return -1;
	}

	if((allwinnerH3->gpio[0] = (unsigned char *)mmap(0, allwinnerH3->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, allwinnerH3->fd, allwinnerH3->base_addr[0])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}

	if((allwinnerH3->gpio[1] = (unsigned char *)mmap(0, allwinnerH3->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, allwinnerH3->fd, allwinnerH3->base_addr[1])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}

	return 0;
}

static char *allwinnerH3GetPinName(int pin) {
	return allwinnerH3->layout[pin].name;
}

static void allwinnerH3SetMap(int *map, size_t size) {
	allwinnerH3->map = map;
	allwinnerH3->map_size = size;
}

static void allwinnerH3SetIRQ(int *irq, size_t size) {
	allwinnerH3->irq = irq;
	allwinnerH3->irq_size = size;
}

static int allwinnerH3DigitalWrite(int i, enum digital_value_t value) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &allwinnerH3->layout[allwinnerH3->map[i]];

	if(allwinnerH3->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}
	if(allwinnerH3->fd <= 0 || allwinnerH3->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}
	if(pin->mode != PINMODE_OUTPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to output mode", allwinnerH3->brand, allwinnerH3->chip, i);
		return -1;
	}

	addr = (unsigned long)(allwinnerH3->gpio[pin->addr] + allwinnerH3->base_offs[pin->addr] + pin->data.offset);

	val = soc_readl(addr);
	if(value == HIGH) {
		soc_writel(addr, val | (1 << pin->data.bit));
	} else {
		soc_writel(addr, val & ~(1 << pin->data.bit));
	}
	return 0;
}

static int allwinnerH3DigitalRead(int i) {
	void *gpio = NULL;
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	pin = &allwinnerH3->layout[allwinnerH3->map[i]];
	gpio = allwinnerH3->gpio[pin->addr];
	addr = (unsigned long)(gpio + allwinnerH3->base_offs[pin->addr] + pin->data.offset);

	if(allwinnerH3->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}
	if(allwinnerH3->fd <= 0 || allwinnerH3->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}
	if(pin->mode != PINMODE_INPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to input mode", allwinnerH3->brand, allwinnerH3->chip, i);
		return -1;
	}

	val = soc_readl(addr);
	return (int)((val & (1 << pin->data.bit)) >> pin->data.bit);
}

static int allwinnerH3PinMode(int i, enum pinmode_t mode) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	uint32_t val = 0;

	if(allwinnerH3->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}
	if(allwinnerH3->fd <= 0 || allwinnerH3->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}

	pin = &allwinnerH3->layout[allwinnerH3->map[i]];
	addr = (unsigned long)(allwinnerH3->gpio[pin->addr] + allwinnerH3->base_offs[pin->addr] + pin->select.offset);
	pin->mode = mode;

	val = soc_readl(addr);
	if(mode == PINMODE_OUTPUT) {
		val |= (1 << pin->select.bit);
	} else if(mode == PINMODE_INPUT) {
		val &= ~(1 << pin->select.bit);
	}
	val &= ~(1 << (pin->select.bit+1));
	val &= ~(1 << (pin->select.bit+2));
	soc_writel(addr, val);
	return 0;
}


static int allwinnerH3ISR(int i, enum isr_mode_t mode) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int x = 0;

	if(allwinnerH3->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}
	if(allwinnerH3->fd <= 0 || allwinnerH3->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}

	pin = &allwinnerH3->layout[allwinnerH3->irq[i]];
	char name[strlen(pin->name)+1];

	memset(&name, '\0', strlen(pin->name)+1);
	for(x = 0; pin->name[x]; x++){
		name[x] = tolower(pin->name[x]);
	}

	sprintf(path, "/sys/class/gpio/gpio%d", allwinnerH3->irq[i]);
	if((soc_sysfs_check_gpio(allwinnerH3, path)) == 0) {
		sprintf(path, "/sys/class/gpio/unexport");
		soc_sysfs_gpio_unexport(allwinnerH3, path, allwinnerH3->irq[i]);
	}

	sprintf(path, "/sys/class/gpio/gpio%d", allwinnerH3->irq[i]);
	if((soc_sysfs_check_gpio(allwinnerH3, path)) == -1) {
		sprintf(path, "/sys/class/gpio/export");
		if(soc_sysfs_gpio_export(allwinnerH3, path, allwinnerH3->irq[i]) == -1) {
			return -1;
		}
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", allwinnerH3->irq[i]);
	if(soc_sysfs_set_gpio_direction(allwinnerH3, path, "in") == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/edge", allwinnerH3->irq[i]);
	if(soc_sysfs_set_gpio_interrupt_mode(allwinnerH3, path, mode) == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", allwinnerH3->irq[i]);
	if((pin->fd = soc_sysfs_gpio_reset_value(allwinnerH3, path)) == -1) {
		return -1;
	}
	pin->mode = PINMODE_INTERRUPT;

	return 0;
}

static int allwinnerH3WaitForInterrupt(int i, int ms) {
	struct layout_t *pin = &allwinnerH3->layout[allwinnerH3->irq[i]];

	if(pin->mode != PINMODE_INTERRUPT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to interrupt mode", allwinnerH3->brand, allwinnerH3->chip, i);
		return -1;
	}
	if(pin->fd <= 0) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d has not been opened for reading", allwinnerH3->brand, allwinnerH3->chip, i);
		return -1;
	}

	return soc_wait_for_interrupt(allwinnerH3, pin->fd, ms);
}

static int allwinnerH3GC(void) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int i = 0, x = 0;

	if(allwinnerH3->map != NULL) {
		for(i=0;i<allwinnerH3->map_size;i++) {
			pin = &allwinnerH3->layout[allwinnerH3->map[i]];
			if(pin->mode == PINMODE_OUTPUT) {
				pinMode(i, PINMODE_INPUT);
			} else if(pin->mode == PINMODE_INTERRUPT) {
				char name[strlen(pin->name)+1];

				memset(&name, '\0', strlen(pin->name)+1);
				for(x = 0; pin->name[x]; x++){
					name[x] = tolower(pin->name[x]);
				}
				sprintf(path, "/sys/class/gpio/gpio%d", allwinnerH3->irq[i]);
				if((soc_sysfs_check_gpio(allwinnerH3, path)) == 0) {
					sprintf(path, "/sys/class/gpio/unexport");
					soc_sysfs_gpio_unexport(allwinnerH3, path, allwinnerH3->irq[i]);
				}
			}
			if(pin->fd > 0) {
				close(pin->fd);
				pin->fd = 0;
			}
		}
	}
	if(allwinnerH3->gpio[0] != NULL) {
		munmap(allwinnerH3->gpio[0], allwinnerH3->page_size);
	}
	return 0;
}

static int allwinnerH3SelectableFd(int i) {
	struct layout_t *pin = NULL;

	if(allwinnerH3->irq == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}

	if(allwinnerH3->fd <= 0 || allwinnerH3->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", allwinnerH3->brand, allwinnerH3->chip);
		return -1;
	}

	pin = &allwinnerH3->layout[allwinnerH3->irq[i]];
	return pin->fd;
}

void allwinnerH3Init(void) {
	soc_register(&allwinnerH3, "Allwinner", "H3");

	allwinnerH3->layout = layout;

	allwinnerH3->support.isr_modes = ISR_MODE_RISING | ISR_MODE_FALLING | ISR_MODE_BOTH | ISR_MODE_NONE;

	allwinnerH3->page_size = (4*1024);
	allwinnerH3->base_addr[0] = 0x01C20000;
	allwinnerH3->base_offs[0] = 0x00000800;

	allwinnerH3->base_addr[1] = 0x01F02000;
	allwinnerH3->base_offs[1] = 0x00000C00;

	allwinnerH3->gc = &allwinnerH3GC;
	allwinnerH3->selectableFd = &allwinnerH3SelectableFd;

	allwinnerH3->pinMode = &allwinnerH3PinMode;
	allwinnerH3->setup = &allwinnerH3Setup;
	allwinnerH3->digitalRead = &allwinnerH3DigitalRead;
	allwinnerH3->digitalWrite = &allwinnerH3DigitalWrite;
	allwinnerH3->getPinName = &allwinnerH3GetPinName;
	allwinnerH3->setMap = &allwinnerH3SetMap;
	allwinnerH3->setIRQ = &allwinnerH3SetIRQ;
	allwinnerH3->isr = &allwinnerH3ISR;
	allwinnerH3->waitForInterrupt = &allwinnerH3WaitForInterrupt;

}
