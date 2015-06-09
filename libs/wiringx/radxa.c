/*
	Copyright (c) 2014 CurlyMo <curlymoo1@gmail.com>
	              2014 Radxa Limited. http://radxa.com

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#ifndef __FreeBSD__
	#include <linux/spi/spidev.h>
#endif

#include "radxa.h"
#ifndef __FreeBSD__
	#include "i2c-dev.h"
#endif

#define NUM_PINS	37
#define NUM_LEDS	3

#define pinBase 160

#define RK_FUNC_GPIO	0

/* GPIO control registers */
#define GPIO_SWPORT_DR		0x00
#define GPIO_SWPORT_DDR		0x04
#define GPIO_EXT_PORT		0x50

#define BIT(nr) (1UL << (nr))

/**
 * Encode variants of iomux registers into a type variable
 */
#define IOMUX_GPIO_ONLY		BIT(0)
#define IOMUX_WIDTH_4BIT	BIT(1)
#define IOMUX_SOURCE_PMU	BIT(2)
#define IOMUX_UNROUTED		BIT(3)

/**
 * @type: iomux variant using IOMUX_* constants
 * @offset: if initialized to -1 it will be autocalculated, by specifying
 *	    an initial offset value the relevant source offset can be reset
 *	    to a new value for autocalculating the following iomux registers.
 */
static struct rockchip_iomux {
	int type;
	int offset;
};

/**
 * @reg_base: register base of the gpio bank
 * @reg_pull: optional separate register for additional pull settings
 * @clk: clock of the gpio bank
 * @irq: interrupt of the gpio bank
 * @pin_base: first pin number
 * @nr_pins: number of pins in this bank
 * @name: name of the bank
 * @bank_num: number of the bank, to account for holes
 * @iomux: array describing the 4 iomux sources of the bank
 * @valid: are all necessary informations present
 * @of_node: dt node of this bank
 * @drvdata: common pinctrl basedata
 * @domain: irqdomain of the gpio bank
 * @gpio_chip: gpiolib chip
 * @grange: gpio range
 * @slock: spinlock for the gpio bank
 */
static struct rockchip_pin_bank {
	void *reg_base;
	int irq;
	int pin_base;
	int nr_pins;
	char *name;
	int bank_num;
	struct rockchip_iomux iomux[4];
	int valid;
	int toggle_edge_mode;
	void *reg_mapped_base;
};

static struct rockchip_pin_ctrl {
	struct rockchip_pin_bank	*pin_banks;
	unsigned int nr_banks;
	unsigned int nr_pins;
	char *label;
	int grf_mux_offset;
	int pmu_mux_offset;
	void *grf_base;
	void *pmu_base;
	void *grf_mapped_base;
	void *pmu_mapped_base;
};

#define PIN_BANK(id, addr, pins, label)			\
	{						\
		.bank_num	= id,			\
		.reg_base	= (void *)addr,		\
		.nr_pins	= pins,			\
		.name		= label,		\
		.iomux		= {			\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
		},					\
	}

#define PIN_BANK_IOMUX_FLAGS(id, addr, pins, label, iom0, iom1, iom2, iom3)	\
	{								\
		.bank_num	= id,					\
		.reg_base	= (void *)addr,			\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
	}


#ifndef __raw_readl
static inline unsigned int __raw_readl(const volatile void *addr) {
	return *(const volatile unsigned int *) addr;
}
#endif

#define readl(addr) __raw_readl(addr)

#ifndef __raw_writel
static inline void __raw_writel(unsigned int b, volatile void *addr) {
	*(volatile unsigned int *) addr = b;
}
#endif

#define writel(b,addr) __raw_writel(b,addr)

#define E_MEM_OPEN			612
#define E_MEM_MAP			613
#define E_MUX_INVAL			614
#define E_MUX_UNROUTED		614
#define E_MUX_GPIOONLY		615

static int pinModes[128];

static struct rockchip_pin_bank rk3188_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 0x2000a000, 32, "gpio0", IOMUX_GPIO_ONLY, IOMUX_GPIO_ONLY, 0, 0),
	PIN_BANK(1, 0x2003c000, 32, "gpio1"),
	PIN_BANK(2, 0x2003e000, 32, "gpio2"),
	PIN_BANK(3, 0x20080000, 32, "gpio3"),
};

static struct rockchip_pin_ctrl rk3188_pin_ctrl = {
	.pin_banks = rk3188_pin_banks,
	.nr_banks = sizeof(rk3188_pin_banks)/sizeof(struct rockchip_pin_bank),
	.label = "RK3188-GPIO",
	.grf_mux_offset	= 0x60,
	.pmu_base = (void *)0x20004000,
	.grf_base = (void *)0x20008000,
};

static int sysFds[NUM_PINS+1] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1,
};

static int pinToGPIO[NUM_PINS+1] = {
	7, 6, 9, 1, 125, 124, 32, 33, 34, 35, 31,
	45, 28, 42, 30, 43, 44, 5, 29, 57, 56,
	90, 91, 89, 88, 8, 2, 126, 47, 39, 36, 38, 37,
	12, 14, 15
};

static int validGPIO[NUM_PINS+1] = {
	0, 1, 2, // 3,
	4, 5, 6, 7, 8, 9,
	10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32,
	33, 34, 35
};

static int onboardLEDs[NUM_LEDS+1] = {
	12, 14, 15
};

static struct rockchip_pin_ctrl *rkxx_pin_ctrl = &rk3188_pin_ctrl;

static struct rockchip_pin_bank *pin_to_bank(unsigned pin) {
	struct rockchip_pin_bank *b = rkxx_pin_ctrl->pin_banks;

	while(pin >= (b->pin_base + b->nr_pins)) {
		b++;
	}

	return b;
}

#ifndef __FreeBSD__
/* SPI Bus Parameters */
static uint8_t     spiMode   = 0;
static uint8_t     spiBPW    = 8;
static uint16_t    spiDelay  = 0;
static uint32_t    spiSpeeds[2];
static int         spiFds[2];
#endif

static int map_reg(void *reg, void **reg_mapped) {
	int fd;
	unsigned int addr_start, addr_offset;
	unsigned int pagesize, pagemask;
	void *pc;

#ifdef O_CLOEXEC
	if((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0) {
#else
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
#endif
	if(fd < 0) {
		return -E_MEM_OPEN;
	}

	pagesize = sysconf(_SC_PAGESIZE);
	pagemask = (~(pagesize - 1));

	addr_start = (unsigned int)reg & pagemask;
	addr_offset = (unsigned int)reg & ~pagemask;

	pc = (void *)mmap(0, pagesize * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr_start);

	if(pc == MAP_FAILED) {
		return -E_MEM_MAP;
	}
	close(fd);

	*reg_mapped = (pc + addr_offset);

	return 0;
}

static int rockchip_gpio_set_mux(unsigned int pin, unsigned int mux) {
	struct rockchip_pin_ctrl *info = rkxx_pin_ctrl;
	struct rockchip_pin_bank *bank = pin_to_bank(pin);
	int iomux_num = (pin / 8);
	unsigned int data, offset, mask;
	unsigned char bit;
	void *reg_mapped_base;

	if(iomux_num > 3) {
		return -E_MUX_INVAL;
	}

	if(bank->iomux[iomux_num].type & IOMUX_UNROUTED) {
		return -E_MUX_UNROUTED;
	}

	if(bank->iomux[iomux_num].type & IOMUX_GPIO_ONLY) {
		if(mux != RK_FUNC_GPIO) {
			return -E_MUX_GPIOONLY;
		} else {
			return 0;
		}
	}

	reg_mapped_base = (bank->iomux[iomux_num].type & IOMUX_SOURCE_PMU)
				? info->pmu_mapped_base : info->grf_mapped_base;

	/* get basic quadrupel of mux registers and the correct reg inside */
	mask = (bank->iomux[iomux_num].type & IOMUX_WIDTH_4BIT) ? 0xf : 0x3;
	offset = bank->iomux[iomux_num].offset;
	if(bank->iomux[iomux_num].type & IOMUX_WIDTH_4BIT) {
		if((pin % 8) >= 4) {
			offset += 0x4;
		}
		bit = (pin % 4) * 4;
	} else {
		bit = (pin % 8) * 2;
	}

	data = (mask << (bit + 16));
	data |= (mux & mask) << bit;
	writel(data, reg_mapped_base + offset);

	return 0;
}

static int changeOwner(char *file) {
	uid_t uid = getuid();
	uid_t gid = getgid();

	if(chown(file, uid, gid) != 0) {
		if(errno == ENOENT)	{
			wiringXLog(LOG_ERR, "radxa->changeOwner: File not present: %s", file);
			return -1;
		} else {
			wiringXLog(LOG_ERR, "radxa->changeOwner: Unable to change ownership of %s: %s", file, strerror (errno));
			return -1;
		}
	}

	return 0;
}

static char radxaKernelVer(void) {
	FILE *versionFd = NULL;
	char line[120];
	char verFlag = 0x00;
	int majorVer=0, minorVer=0, patchVer=0;

	if((versionFd = fopen("/proc/version", "r")) == NULL) {
		wiringXLog(LOG_ERR, "radxa->identify: Unable open /proc/version");
		return -1;
	}

	fgets(line, 120, versionFd);
	fclose(versionFd);

	if(sscanf(line, "%*[^0-9]%d.%d.%d", &majorVer, &minorVer, &patchVer) < 3) {
		wiringXLog(LOG_DEBUG, "radxa->identify: Kernel version cannot be determined.");
		return -1;
	}	else {
		if(majorVer < 3) {
			verFlag |= 0x01;
			verFlag |= 0x02;
		} else if(majorVer > 3) {
			verFlag &= 0xfe;
			verFlag &= 0xfd;
		} else {
			if(minorVer < 12) {
				verFlag |= 0x01;
				if(minorVer < 10) {
					verFlag |= 0x02;
				} else {
					verFlag &= 0xfd;
				}
			} else {
				verFlag &= 0xfe;
				verFlag &= 0xfd;
			}
		}
		wiringXLog(LOG_DEBUG, "radxa->identify: Kernel version is %d.%d.%d.", majorVer, minorVer, patchVer);
		return verFlag;
	}
}

static int radxaISR(int pin, int mode) {
	int i = 0, fd = 0, count = 0;
	int match = 0;
	char kernelVer = 0, pinbaseFlag = 0, edgeFlag = 0;
	const char *sMode = NULL;
	char path[35], c, line[120];
	FILE *f = NULL;

	if(pin < 0 || pin > 35) {
		wiringXLog(LOG_ERR, "radxa->isr: Invalid pin number %d", pin);
		return -1;
	}

	for(i=0;i<NUM_LEDS;i++) {
		if(onboardLEDs[i] == pinToGPIO[pin]) {
			wiringXLog(LOG_ERR, "radxa->isr: The onboard LEDs cannot be used as interrupts");
			return -1;
		}
	}
	for(i=0;i<NUM_PINS;i++) {
		if(validGPIO[i] == pin) {
			match = 1;
			break;
		}
	}

	if(match == 0) {
		wiringXLog(LOG_ERR, "radxa->isr: Invalid pin number %d", pin);
		return -1;
	}

	pinModes[pin] = SYS;

	kernelVer = radxaKernelVer();
	pinbaseFlag = kernelVer & 0x02;
	edgeFlag = kernelVer & 0x01;

	if(mode == INT_EDGE_FALLING) {
		sMode = "falling" ;
	} else if(mode == INT_EDGE_RISING) {
		sMode = "rising" ;
	} else if(mode == INT_EDGE_BOTH) {
		if(edgeFlag != 0) {
			wiringXLog(LOG_ERR, "radxa->isr: Kernel version below 3.12 doesn's support both edge interruption.", sMode);
			return -1;
		} else {
			sMode = "both";
		}
	} else {
		wiringXLog(LOG_ERR, "radxa->isr: Invalid mode. Should be INT_EDGE_RISING, INT_EDGE_FALLING, or INT_EDGE_BOTH");
		return -1;
	}

	if(pinbaseFlag != 0) {
		sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGPIO[pin]+pinBase);
	} else {
		sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGPIO[pin]);
	}

	if((fd = open(path, O_RDWR)) < 0) {
		if((f = fopen("/sys/class/gpio/export", "w")) == NULL) {
			wiringXLog(LOG_ERR, "radxa->isr: Unable to open GPIO export interface");
			return -1;
		}

		if(pinbaseFlag !=0){
			fprintf(f, "%d", pinToGPIO[pin]+pinBase);
		}else{
			fprintf(f, "%d", pinToGPIO[pin]);
		}
		fclose(f);
	}

	if(pinbaseFlag != 0) {
		sprintf(path, "/sys/class/gpio/gpio%d/direction", pinToGPIO[pin]+pinBase);
	} else {
		sprintf(path, "/sys/class/gpio/gpio%d/direction", pinToGPIO[pin]);
	}
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "radxa->isr: Unable to open GPIO direction interface for pin %d: %s", pin, strerror(errno));
		return -1;
	}

	fprintf(f, "in\n");
	fclose(f);

	if(pinbaseFlag != 0) {
		sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGPIO[pin]+pinBase);
	} else {
		sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGPIO[pin]);
	}
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "radxa->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
		return -1;
	}

	if(strcasecmp(sMode, "none") == 0) {
		fprintf(f, "none\n");
	} else if(strcasecmp(sMode, "rising") == 0) {
		fprintf(f, "rising\n");
	} else if(strcasecmp(sMode, "falling") == 0) {
		fprintf(f, "falling\n");
	} else if(strcasecmp(sMode, "both") == 0) {
		fprintf(f, "both\n");
	} else {
		wiringXLog(LOG_ERR, "radxa->isr: Could not set interrupt mode to %s", sMode);
		return -1;
	}
	fclose(f);

	if((f = fopen(path, "r")) == NULL) {
		wiringXLog(LOG_ERR, "radxa->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
		return -1;
	}

	match = 0;
	while(fgets(line, 120, f) != NULL) {
		if(strstr(line, sMode) != NULL) {
			match = 1;
			break;
		}
	}
	fclose(f);

	if(match == 0) {
		wiringXLog(LOG_ERR, "radxa->isr: Failed to set interrupt edge to %s", sMode);
		return -1;	
	}

	if(pinbaseFlag != 0) {
		sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGPIO[pin]+pinBase);
	} else {
		sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGPIO[pin]);
	}
	if((sysFds[pin] = open(path, O_RDONLY)) < 0) {
		wiringXLog(LOG_ERR, "radxa->isr: Unable to open GPIO value interface: %s", strerror(errno));
		return -1;
	}
	changeOwner(path);

	if(pinbaseFlag != 0) {
		sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGPIO[pin]+pinBase);
	} else {
		sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGPIO[pin]);
	}
	changeOwner(path);

	ioctl(fd, FIONREAD, &count);
	for(i=0; i<count; ++i) {
		read(fd, &c, 1);
	}
	close(fd);

	return 0;
}

static int radxaWaitForInterrupt(int pin, int ms) {
	int x = 0, i = 0, match = 0;
	uint8_t c = 0;
	struct pollfd polls;

	if(pin < 0 || pin > 35) {
		wiringXLog(LOG_ERR, "radxa->waitForInterrupt: Invalid pin number %d", pin);
		return -1;
	}

	for(i=0;i<NUM_LEDS;i++) {
		if(onboardLEDs[i] == pinToGPIO[pin]) {
			wiringXLog(LOG_ERR, "radxa->waitForInterrupt: The onboard LEDs cannot be used as interrupts");
			return -1;
		}
	}
	for(i=0;i<NUM_PINS;i++) {
		if(validGPIO[i] == pin) {
			match = 1;
			break;
		}
	}

	if(match == 0) {
		wiringXLog(LOG_ERR, "radxa->waitForInterrupt: Invalid pin number %d", pin);
		return -1;
	}

	if(pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "radxa->waitForInterrupt: Trying to read from pin %d, but it's not configured as interrupt", pin);
		return -1;
	}

	if(sysFds[pin] == -1) {
		wiringXLog(LOG_ERR, "radxa->waitForInterrupt: GPIO %d not set as interrupt", pin);
		return -1;
	}

	polls.fd = sysFds[pin];
	polls.events = POLLPRI;

	(void)read(sysFds[pin], &c, 1);
	lseek(sysFds[pin], 0, SEEK_SET);

	x = poll(&polls, 1, ms);

	/* Don't react to signals */
	if(x == -1 && errno == EINTR) {
		x = 0;
	}

	return x;
}

static int radxaBoardRev(void) {
	FILE *cpuFd;
	char line[120];
	char *d;

	if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
		wiringXLog(LOG_ERR, "radxa->identify: Unable open /proc/cpuinfo");
		return -1;
	}

	while(fgets(line, 120, cpuFd) != NULL) {
		if(strncmp(line, "Hardware", 8) == 0) {
			break;
		}
	}

	fclose(cpuFd);

	if(strncmp(line, "Hardware", 8) != 0) {
		wiringXLog(LOG_ERR, "radxa->identify: /proc/cpuinfo has no hardware line");
		return -1;
	}

	for(d = &line[strlen(line) - 1]; (*d == '\n') || (*d == '\r') ; --d)
		*d = 0 ;

	if(strstr(line, "RK30board") != NULL ||
	   strstr(line, "Rockchip") != NULL) {
		return 0;
	} else {
		return -1;
	}
}

static int radxaSetup(void)	{
	int grf_offs, pmu_offs, i, j;
	int ret;

	struct rockchip_pin_ctrl *ctrl = rkxx_pin_ctrl;
	struct rockchip_pin_bank *bank = ctrl->pin_banks;

	grf_offs = ctrl->grf_mux_offset;
	pmu_offs = ctrl->pmu_mux_offset;

	for(i = 0; i < ctrl->nr_banks; ++i, ++bank) {
		int bank_pins = 0;

		bank->pin_base = ctrl->nr_pins;
		ctrl->nr_pins += bank->nr_pins;

		/* calculate iomux offsets */
		for(j = 0; j < 4; j++) {
			struct rockchip_iomux *iom = &bank->iomux[j];
			int inc;

			if(bank_pins >= bank->nr_pins)
				break;

			/* preset offset value, set new start value */
			if(iom->offset >= 0) {
				if(iom->type & IOMUX_SOURCE_PMU) {
					pmu_offs = iom->offset;
				} else {
					grf_offs = iom->offset;
				}
			} else { /* set current offset */
				iom->offset = (iom->type & IOMUX_SOURCE_PMU) ? pmu_offs : grf_offs;
			}

			/*
			 * Increase offset according to iomux width.
			 * 4bit iomux'es are spread over two registers.
			 */
			inc = (iom->type & IOMUX_WIDTH_4BIT) ? 8 : 4;
			if(iom->type & IOMUX_SOURCE_PMU) {
				pmu_offs += inc;
			} else {
				grf_offs += inc;
			}

			bank_pins += 8;
		}

		ret = map_reg(bank->reg_base, &bank->reg_mapped_base);
		if(ret < 0) {
			return ret;
		}
	}

	ret = map_reg(ctrl->pmu_base, &ctrl->pmu_mapped_base);
	if(ret < 0) {
		return ret;
	}
	ret = map_reg(ctrl->grf_base, &ctrl->grf_mapped_base);
	if(ret < 0) {
		return ret;
	}

	return 0;
}

static int radxaDigitalRead(int pin) {
	unsigned int data;
	int i = 0, match = 0;
	struct rockchip_pin_bank *bank = pin_to_bank(pinToGPIO[pin]);
	int offset = pinToGPIO[pin] - bank->pin_base;

	if(pin < 0 || pin > 35) {
		wiringXLog(LOG_ERR, "radxa->digitalRead: Invalid pin number %d", pin);
		return -1;
	}

	for(i=0;i<NUM_LEDS;i++) {
		if(onboardLEDs[i] == pinToGPIO[pin]) {
			wiringXLog(LOG_ERR, "radxa->digitalRead: The onboard LEDs cannot be used as input");
			return -1;
		}
	}
	for(i=0;i<NUM_PINS;i++) {
		if(validGPIO[i] == pin) {
			match = 1;
			break;
		}
	}

	if(match == 0) {
		wiringXLog(LOG_ERR, "radxa->digitalRead: Invalid pin number %d", pin);
		return -1;
	}

	if(pinModes[pin] != INPUT && pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "radxa->digitalRead: Trying to write to pin %d, but it's not configured as input", pin);
		return -1;
	}

	data = readl(bank->reg_mapped_base + GPIO_EXT_PORT);
	data >>= offset;

	return (data & 0x1);
}

static int radxaDigitalWrite(int pin, int value) {
	unsigned int data;
	int i = 0, match = 0;
	struct rockchip_pin_bank *bank = pin_to_bank(pinToGPIO[pin]);
	void *reg = bank->reg_mapped_base + GPIO_SWPORT_DR;
	int offset = pinToGPIO[pin] - bank->pin_base;

	if(pin < 0 || pin > 35) {
		wiringXLog(LOG_ERR, "radxa->digitalWrite: Invalid pin number %d", pin);
		return -1;
	}

	for(i=0;i<NUM_PINS;i++) {
		if(validGPIO[i] == pin) {
			match = 1;
			break;
		}
	}

	if(match == 0) {
		wiringXLog(LOG_ERR, "radxa->digitalWrite: Invalid pin number %d", pin);
		return -1;
	}

	if(pinModes[pin] != OUTPUT) {
		wiringXLog(LOG_ERR, "radxa->digitalWrite: Trying to write to pin %d, but it's not configured as output", pin);
		return -1;
	}

	data = readl(reg);
	data &= ~BIT(offset);
	if(value) {
		data |= BIT(offset);
	}

	writel(data, reg);
	return 0;
}

static int radxaPinMode(int pin, int mode) {
	int ret, offset, i = 0, match = 0;
	struct rockchip_pin_bank *bank = pin_to_bank(pin);
	unsigned int data;

	if(pin < 0 || pin > 35) {
		wiringXLog(LOG_ERR, "radxa->pinMode: Invalid pin number %d", pin);
		return -1;
	}

	for(i=0;i<NUM_LEDS;i++) {
		if(onboardLEDs[i] == pinToGPIO[pin] && mode == INPUT) {
			wiringXLog(LOG_ERR, "radxa->pinMode: The onboard LEDs cannot be used as input");
			return -1;
		}
	}
	for(i=0;i<NUM_PINS;i++) {
		if(validGPIO[i] == pin) {
			match = 1;
			break;
		}
	}

	if(match == 0) {
		wiringXLog(LOG_ERR, "radxa->pinMode: Invalid pin number %d", pin);
		return -1;
	}

	pinModes[pin] = mode;

	ret = rockchip_gpio_set_mux(pinToGPIO[pin], RK_FUNC_GPIO);
	if(ret < 0) {
		return ret;
	}

	data = readl(bank->reg_mapped_base + GPIO_SWPORT_DDR);

	/* set bit to 1 for output, 0 for input */
	offset = pinToGPIO[pin] - bank->pin_base;
	if(mode != INPUT) {
		data |= BIT(offset);
	} else {
		data &= ~BIT(offset);
	}

	writel(data, bank->reg_mapped_base + GPIO_SWPORT_DDR);

	return 0;
}

static int radxaGC(void) {
	int i = 0, fd = 0;
	char pinbaseFlag = 0;
	char path[35];
	FILE *f = NULL;

	pinbaseFlag = radxaKernelVer();
	pinbaseFlag &= 0x02;

	for(i=0;i<NUM_PINS;i++) {
		if(pinModes[i] == OUTPUT) {
			pinMode(i, INPUT);
		} else if(pinModes[i] == SYS) {
			if(pinbaseFlag != 0){
				sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGPIO[i]+pinBase);
			}else{
				sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGPIO[i]);
			}
			if((fd = open(path, O_RDWR)) > 0) {
				if((f = fopen("/sys/class/gpio/unexport", "w")) == NULL) {
					wiringXLog(LOG_ERR, "radxa->gc: Unable to open GPIO unexport interface: %s", strerror(errno));
				}

				fprintf(f, "%d", pinToGPIO[i]);
				fclose(f);
				close(fd);
			}
		}
		if(sysFds[i] > 0) {
			close(sysFds[i]);
		}
	}

	return 0;
}

#ifdef __FreeBSD__
static int radxaI2CRead(int fd) {
	return i2c_smbus_read_byte(fd);
}

static int radxaI2CReadReg8(int fd, int reg) {
	return i2c_smbus_read_byte_data(fd, reg);
}

static int radxaI2CReadReg16(int fd, int reg) {
	return i2c_smbus_read_word_data(fd, reg);
}

static int radxaI2CWrite(int fd, int data) {
	return i2c_smbus_write_byte(fd, data);
}

static int radxaI2CWriteReg8(int fd, int reg, int data) {
	return i2c_smbus_write_byte_data(fd, reg, data);
}

static int radxaI2CWriteReg16(int fd, int reg, int data) {
	return i2c_smbus_write_word_data(fd, reg, data);
}

static int radxaI2CSetup(int devId) {
	int rev = 0, fd = 0;
	const char *device = NULL;

	if((rev = radxaBoardRev()) < 0) {
		wiringXLog(LOG_ERR, "radxa->I2CSetup: Unable to determine Pi board revision");
		return -1;
	}

	if((fd = open("/dev/i2c-0", O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "radxa->I2CSetup: Unable to open %s", device);
		return -1;
	}

	if(ioctl(fd, I2C_SLAVE, devId) < 0) {
		wiringXLog(LOG_ERR, "radxa->I2CSetup: Unable to set %s to slave mode", device);
		return -1;
	}

	return fd;
}

int radxaValidGPIO(int pin) {
	int i = 0;
	for(i=0;i<NUM_PINS;i++) {
		if(validGPIO[i] == pin) {
			return 0;
		}
	}
	return -1;
}

static int radxaSPIGetFd(int channel) {
	return spiFds[channel & 1];
}

static int radxaSPIDataRW(int channel, unsigned char *data, int len) {
	struct spi_ioc_transfer spi;
	memset(&spi, 0, sizeof(spi));
	channel &= 1;

	spi.tx_buf = (unsigned long)data;
	spi.rx_buf = (unsigned long)data;
	spi.len = len;
	spi.delay_usecs = spiDelay;
	spi.speed_hz = spiSpeeds[channel];
	spi.bits_per_word = spiBPW;
#ifdef SPI_IOC_WR_MODE32
	spi.tx_nbits = 0;
#endif
#ifdef SPI_IOC_RD_MODE32
	spi.rx_nbits = 0;
#endif

	if(ioctl(spiFds[channel], SPI_IOC_MESSAGE(1), &spi) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPIDataRW: Unable to read/write from channel %d: %s", channel, strerror(errno));
		return -1;
	}
	return 0;
}

static int radxaSPISetup(int channel, int speed) {
	int fd;
	const char *device = NULL;

	channel &= 1;

	if(channel == 0) {
		device = "/dev/spidev0.0";
	} else {
		device = "/dev/spidev0.1";
	}
	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPISetup: Unable to open device %s: %s", device, strerror(errno));
		return -1;
	}

	spiSpeeds[channel] = speed;
	spiFds[channel] = fd;

	if(ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPISetup: Unable to set write mode for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPISetup: Unable to set read mode for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPISetup: Unable to set write bits_per_word for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPISetup: Unable to set read bits_per_word for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPISetup: Unable to set write max_speed for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "radxa->SPISetup: Unable to set read max_speed for device %s: %s", device, strerror(errno));
		return -1;
	}
	return fd;
}
#endif

void radxaInit(void) {

	memset(pinModes, -1, 128);

	platform_register(&radxa, "radxa");
	radxa->setup=&radxaSetup;
	radxa->pinMode=&radxaPinMode;
	radxa->digitalWrite=&radxaDigitalWrite;
	radxa->digitalRead=&radxaDigitalRead;
	radxa->identify=&radxaBoardRev;
	radxa->isr=&radxaISR;
	radxa->waitForInterrupt=&radxaWaitForInterrupt;
#ifndef __FreeBSD__	
	radxa->I2CRead=&radxaI2CRead;
	radxa->I2CReadReg8=&radxaI2CReadReg8;
	radxa->I2CReadReg16=&radxaI2CReadReg16;
	radxa->I2CWrite=&radxaI2CWrite;
	radxa->I2CWriteReg8=&radxaI2CWriteReg8;
	radxa->I2CWriteReg16=&radxaI2CWriteReg16;
	radxa->I2CSetup=&radxaI2CSetup;
	radxa->SPIGetFd=&radxaSPIGetFd;
	radxa->SPIDataRW=&radxaSPIDataRW;
	radxa->SPISetup=&radxaSPISetup;
#endif	
	radxa->gc=&radxaGC;
	radxa->validGPIO=&radxaValidGPIO;
}
