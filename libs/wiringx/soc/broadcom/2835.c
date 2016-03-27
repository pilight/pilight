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

#include "2835.h"
#include "../../wiringX.h"
#include "../soc.h"

struct soc_t *broadcom2835 = NULL;

#define GPFSEL0	0x00
#define GPFSEL1	0x04
#define GPFSEL2	0x08
#define GPFSEL3	0x0C
#define GPFSEL4	0x10
#define GPFSEL5	0x14

#define GPSET0	0x1C
#define GPSET1	0x20

#define GPCLR0	0x28
#define GPCLR1	0x2C

#define GPLEV0	0x34
#define GPLEV1	0x38

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
	} set;

	struct {
		unsigned long offset;
		unsigned long bit;
	} clear;

	struct {
		unsigned long offset;
		unsigned long bit;
	} level;	

	int support;

	enum pinmode_t mode;

	int fd;

} layout[] = {
	{ "FSEL0", 0, { GPFSEL0, 0 }, { GPSET0, 0 }, { GPCLR0, 0 }, { GPLEV0, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL1", 0, { GPFSEL0, 3 }, { GPSET0, 1 }, { GPCLR0, 1 }, { GPLEV0, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL2", 0, { GPFSEL0, 6 }, { GPSET0, 2 }, { GPCLR0, 2 }, { GPLEV0, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL3", 0, { GPFSEL0, 9 }, { GPSET0, 3 }, { GPCLR0, 3 }, { GPLEV0, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL4", 0, { GPFSEL0, 12 }, { GPSET0, 4 }, { GPCLR0, 4 }, { GPLEV0, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL5", 0, { GPFSEL0, 15 }, { GPSET0, 5 }, { GPCLR0, 5 }, { GPLEV0, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL6", 0, { GPFSEL0, 18 }, { GPSET0, 6 }, { GPCLR0, 6 }, { GPLEV0, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL7", 0, { GPFSEL0, 21 }, { GPSET0, 7 }, { GPCLR0, 7 }, { GPLEV0, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL8", 0, { GPFSEL0, 24 }, { GPSET0, 8 }, { GPCLR0, 8 }, { GPLEV0, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL9", 0, { GPFSEL0, 27 }, { GPSET0, 9 }, { GPCLR0, 9 }, { GPLEV0, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL10", 0, { GPFSEL1, 0 }, { GPSET0, 10 }, { GPCLR0, 10 }, { GPLEV0, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL11", 0, { GPFSEL1, 3 }, { GPSET0, 11 }, { GPCLR0, 11 }, { GPLEV0, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL12", 0, { GPFSEL1, 6 }, { GPSET0, 12 }, { GPCLR0, 12 }, { GPLEV0, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL13", 0, { GPFSEL1, 9 }, { GPSET0, 13 }, { GPCLR0, 13 }, { GPLEV0, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL14", 0, { GPFSEL1, 12 }, { GPSET0, 14 }, { GPCLR0, 14 }, { GPLEV0, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL15", 0, { GPFSEL1, 15 }, { GPSET0, 15 }, { GPCLR0, 15 }, { GPLEV0, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL16", 0, { GPFSEL1, 18 }, { GPSET0, 16 }, { GPCLR0, 16 }, { GPLEV0, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL17", 0, { GPFSEL1, 21 }, { GPSET0, 17 }, { GPCLR0, 17 }, { GPLEV0, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL18", 0, { GPFSEL1, 24 }, { GPSET0, 18 }, { GPCLR0, 18 }, { GPLEV0, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL19", 0, { GPFSEL1, 27 }, { GPSET0, 19 }, { GPCLR0, 19 }, { GPLEV0, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL20", 0, { GPFSEL2, 0 }, { GPSET0, 20 }, { GPCLR0, 20 }, { GPLEV0, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL21", 0, { GPFSEL2, 3 }, { GPSET0, 21 }, { GPCLR0, 21 }, { GPLEV0, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL22", 0, { GPFSEL2, 6 }, { GPSET0, 22 }, { GPCLR0, 22 }, { GPLEV0, 22 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL23", 0, { GPFSEL2, 9 }, { GPSET0, 23 }, { GPCLR0, 23 }, { GPLEV0, 23 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL24", 0, { GPFSEL2, 12 }, { GPSET0, 24 }, { GPCLR0, 24 }, { GPLEV0, 24 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL25", 0, { GPFSEL2, 15 }, { GPSET0, 25 }, { GPCLR0, 25 }, { GPLEV0, 25 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL26", 0, { GPFSEL2, 18 }, { GPSET0, 26 }, { GPCLR0, 26 }, { GPLEV0, 26 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL27", 0, { GPFSEL2, 21 }, { GPSET0, 27 }, { GPCLR0, 27 }, { GPLEV0, 27 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL28", 0, { GPFSEL2, 24 }, { GPSET0, 28 }, { GPCLR0, 28 }, { GPLEV0, 28 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL29", 0, { GPFSEL2, 27 }, { GPSET0, 29 }, { GPCLR0, 29 }, { GPLEV0, 29 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL30", 0, { GPFSEL3, 0 }, { GPSET0, 30 }, { GPCLR0, 30 }, { GPLEV0, 30 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL31", 0, { GPFSEL3, 3 }, { GPSET0, 31 }, { GPCLR0, 31 }, { GPLEV0, 31 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL32", 0, { GPFSEL3, 6 }, { GPSET1, 0 }, { GPCLR1, 0 }, { GPLEV1, 0 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL33", 0, { GPFSEL3, 9 }, { GPSET1, 1 }, { GPCLR1, 1 }, { GPLEV1, 1 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL34", 0, { GPFSEL3, 12 }, { GPSET1, 2 }, { GPCLR1, 2 }, { GPLEV1, 2 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL35", 0, { GPFSEL3, 15 }, { GPSET1, 3 }, { GPCLR1, 3 }, { GPLEV1, 3 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL36", 0, { GPFSEL3, 18 }, { GPSET1, 4 }, { GPCLR1, 4 }, { GPLEV1, 4 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL37", 0, { GPFSEL3, 21 }, { GPSET1, 5 }, { GPCLR1, 5 }, { GPLEV1, 5 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL38", 0, { GPFSEL3, 24 }, { GPSET1, 6 }, { GPCLR1, 6 }, { GPLEV1, 6 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL39", 0, { GPFSEL3, 27 }, { GPSET1, 7 }, { GPCLR1, 7 }, { GPLEV1, 7 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL40", 0, { GPFSEL4, 0 }, { GPSET1, 8 }, { GPCLR1, 8 }, { GPLEV1, 8 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL41", 0, { GPFSEL4, 3 }, { GPSET1, 9 }, { GPCLR1, 9 }, { GPLEV1, 9 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL42", 0, { GPFSEL4, 6 }, { GPSET1, 10 }, { GPCLR1, 10 }, { GPLEV1, 10 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL43", 0, { GPFSEL4, 9 }, { GPSET1, 11 }, { GPCLR1, 11 }, { GPLEV1, 11 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL44", 0, { GPFSEL4, 12 }, { GPSET1, 12 }, { GPCLR1, 12 }, { GPLEV1, 12 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL45", 0, { GPFSEL4, 15 }, { GPSET1, 13 }, { GPCLR1, 13 }, { GPLEV1, 13 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL46", 0, { GPFSEL4, 18 }, { GPSET1, 14 }, { GPCLR1, 14 }, { GPLEV1, 14 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL47", 0, { GPFSEL4, 21 }, { GPSET1, 15 }, { GPCLR1, 15 }, { GPLEV1, 15 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL48", 0, { GPFSEL4, 24 }, { GPSET1, 16 }, { GPCLR1, 16 }, { GPLEV1, 16 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL49", 0, { GPFSEL4, 27 }, { GPSET1, 17 }, { GPCLR1, 17 }, { GPLEV1, 17 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL50", 0, { GPFSEL5, 0 }, { GPSET1, 18 }, { GPCLR1, 18 }, { GPLEV1, 18 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL51", 0, { GPFSEL5, 3 }, { GPSET1, 19 }, { GPCLR1, 19 }, { GPLEV1, 19 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL52", 0, { GPFSEL5, 6 }, { GPSET1, 20 }, { GPCLR1, 20 }, { GPLEV1, 20 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
	{ "FSEL53", 0, { GPFSEL5, 9 }, { GPSET1, 21 }, { GPCLR1, 21 }, { GPLEV1, 21 }, FUNCTION_DIGITAL, PINMODE_NOT_SET, 0 },
};

static int broadcom2835Setup(void) {
	if((broadcom2835->fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open /dev/mem for raw memory access");
		return -1;
	}

	if((broadcom2835->gpio[0] = (unsigned char *)mmap(0, broadcom2835->page_size, PROT_READ|PROT_WRITE, MAP_SHARED, broadcom2835->fd, broadcom2835->base_addr[0])) == NULL) {
		wiringXLog(LOG_ERR, "wiringX failed to map the %s %s GPIO memory address", broadcom2835->brand, broadcom2835->chip);
		return -1;
	}

	return 0;
}

static char *broadcom2835GetPinName(int pin) {
	return broadcom2835->layout[pin].name;
}

static void broadcom2835SetMap(int *map) {
	broadcom2835->map = map;
}

static void broadcom2835SetIRQ(int *irq) {
	broadcom2835->irq = irq;
}

static int broadcom2835DigitalWrite(int i, enum digital_value_t value) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;

	pin = &broadcom2835->layout[broadcom2835->map[i]];

	if(broadcom2835->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", broadcom2835->brand, broadcom2835->chip);
		return -1; 
	}
	if(broadcom2835->fd <= 0 || broadcom2835->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", broadcom2835->brand, broadcom2835->chip);
		return -1;
	}
	if(pin->mode != PINMODE_OUTPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to output mode", broadcom2835->brand, broadcom2835->chip, i);
		return -1;
	}

	if(value == HIGH) {
		addr = (unsigned long)(broadcom2835->gpio[pin->addr] + broadcom2835->base_offs[pin->addr] + pin->set.offset);
		soc_writel(addr, (1 << pin->set.bit));
	} else {
		addr = (unsigned long)(broadcom2835->gpio[pin->addr] + broadcom2835->base_offs[pin->addr] + pin->clear.offset);
		soc_writel(addr, (1 << pin->clear.bit)); 
	}
	return 0;
}

static int broadcom2835DigitalRead(int i) {
	void *gpio = NULL;
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	unsigned long val = 0;

	pin = &broadcom2835->layout[broadcom2835->map[i]];
	gpio = broadcom2835->gpio[pin->addr];
	addr = (unsigned long)(gpio + broadcom2835->base_offs[pin->addr] + pin->level.offset);

	if(broadcom2835->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", broadcom2835->brand, broadcom2835->chip);
		return -1; 
	}
	if(broadcom2835->fd <= 0 || broadcom2835->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", broadcom2835->brand, broadcom2835->chip);
		return -1;
	}
	if(pin->mode != PINMODE_INPUT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to input mode", broadcom2835->brand, broadcom2835->chip, i);
		return -1;
	}

	val = soc_readl(addr);

	return (int)((val & (1 << pin->level.bit)) >> pin->level.bit);
}

static int broadcom2835PinMode(int i, enum pinmode_t mode) {
	struct layout_t *pin = NULL;
	unsigned long addr = 0;
	unsigned long val = 0;

	if(broadcom2835->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", broadcom2835->brand, broadcom2835->chip);
		return -1; 
	} 
	if(broadcom2835->fd <= 0 || broadcom2835->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", broadcom2835->brand, broadcom2835->chip);
		return -1;
	}

	pin = &broadcom2835->layout[broadcom2835->map[i]];
	addr = (unsigned long)(broadcom2835->gpio[pin->addr] + broadcom2835->base_offs[pin->addr] + pin->select.offset);
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

static int broadcom2835ISR(int i, enum isr_mode_t mode) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];

	if(broadcom2835->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", broadcom2835->brand, broadcom2835->chip);
		return -1; 
	} 
	if(broadcom2835->fd <= 0 || broadcom2835->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", broadcom2835->brand, broadcom2835->chip);
		return -1;
	}

	pin = &broadcom2835->layout[broadcom2835->map[i]];

	sprintf(path, "/sys/class/gpio/gpio%d", broadcom2835->map[i]);
	if((soc_sysfs_check_gpio(broadcom2835, path)) == -1) {
		sprintf(path, "/sys/class/gpio/export");
		if(soc_sysfs_gpio_export(broadcom2835, path, broadcom2835->map[i]) == -1) {
			return -1;
		}
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", broadcom2835->map[i]);
	if(soc_sysfs_set_gpio_direction(broadcom2835, path, "in") == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/edge", broadcom2835->map[i]);
	if(soc_sysfs_set_gpio_interrupt_mode(broadcom2835, path, mode) == -1) {
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", broadcom2835->map[i]);
	if((pin->fd = soc_sysfs_gpio_reset_value(broadcom2835, path)) == -1) {
		return -1;
	}
	pin->mode = PINMODE_INTERRUPT; 

	return 0;
}

static int broadcom2835WaitForInterrupt(int i, int ms) {
	struct layout_t *pin = &broadcom2835->layout[broadcom2835->map[i]];

	if(pin->mode != PINMODE_INTERRUPT) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d is not set to interrupt mode", broadcom2835->brand, broadcom2835->chip, i);
		return -1;
	}
	if(pin->fd <= 0) {
		wiringXLog(LOG_ERR, "The %s %s GPIO %d has not been opened for reading", broadcom2835->brand, broadcom2835->chip, i);
		return -1; 
	}

	return soc_wait_for_interrupt(broadcom2835, pin->fd, ms);
}

static int broadcom2835GC(void) {
	struct layout_t *pin = NULL;
	char path[PATH_MAX];
	int i = 0, l = 0;

	if(broadcom2835->map != NULL) {
		l = sizeof(broadcom2835->map)/sizeof(broadcom2835->map[0]);

		for(i=0;i<l;i++) {
			pin = &broadcom2835->layout[broadcom2835->map[i]];
			if(pin->mode == PINMODE_OUTPUT) {
				pinMode(i, PINMODE_INPUT);
			} else if(pin->mode == PINMODE_INTERRUPT) {
				sprintf(path, "/sys/class/gpio/gpio%d", broadcom2835->map[i]);
				if((soc_sysfs_check_gpio(broadcom2835, path)) == 0) {
					sprintf(path, "/sys/class/gpio/unexport");
					soc_sysfs_gpio_unexport(broadcom2835, path, i);
				}
			}
			if(pin->fd > 0) {
				close(pin->fd);
				pin->fd = 0;
			}
		}
	}
	if(broadcom2835->gpio[0] != NULL) {
		munmap(broadcom2835->gpio[0], broadcom2835->page_size);
	} 
	return 0;
}

static int broadcom2835SelectableFd(int i) {
	struct layout_t *pin = NULL;

	if(broadcom2835->map == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been mapped", broadcom2835->brand, broadcom2835->chip);
		return -1; 
	} 
	if(broadcom2835->fd <= 0 || broadcom2835->gpio == NULL) {
		wiringXLog(LOG_ERR, "The %s %s has not yet been setup by wiringX", broadcom2835->brand, broadcom2835->chip);
		return -1;
	}

	pin = &broadcom2835->layout[broadcom2835->map[i]];
	return pin->fd;
}

void broadcom2835Init(void) {
	soc_register(&broadcom2835, "Broadcom", "2835");

	broadcom2835->layout = layout;

	broadcom2835->support.isr_modes = ISR_MODE_RISING | ISR_MODE_FALLING | ISR_MODE_BOTH | ISR_MODE_NONE;

	broadcom2835->page_size = (4*1024);
	broadcom2835->base_addr[0] = 0x20200000;
	broadcom2835->base_offs[0] = 0x00000000;

	broadcom2835->gc = &broadcom2835GC;
	broadcom2835->selectableFd = &broadcom2835SelectableFd;

	broadcom2835->pinMode = &broadcom2835PinMode;
	broadcom2835->setup = &broadcom2835Setup;
	broadcom2835->digitalRead = &broadcom2835DigitalRead;
	broadcom2835->digitalWrite = &broadcom2835DigitalWrite;
	broadcom2835->getPinName = &broadcom2835GetPinName;
	broadcom2835->setMap = &broadcom2835SetMap;
	broadcom2835->setIRQ = &broadcom2835SetIRQ;
	broadcom2835->isr = &broadcom2835ISR;
	broadcom2835->waitForInterrupt = &broadcom2835WaitForInterrupt;
}
