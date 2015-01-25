/*
	Copyright (c) 2014 CurlyMo <curlymoo1@gmail.com>
	              2014 lemaker http://www.lemaker.org

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

#include "wiringX.h"
#ifndef __FreeBSD__
	#include "i2c-dev.h"
#endif
#include "bananapi.h"

#define	WPI_MODE_PINS		 0
#define	WPI_MODE_GPIO		 1
#define	WPI_MODE_GPIO_SYS	 2
#define	WPI_MODE_PHYS		 3
#define	WPI_MODE_PIFACE		 4
#define	WPI_MODE_UNINITIALISED	-1

#define	PAGE_SIZE		(4*1024)
#define	BLOCK_SIZE		(4*1024)

#define MAP_SIZE		(4096*2)
#define MAP_MASK		(MAP_SIZE - 1)

#define	PI_GPIO_MASK	(0xFFFFFFC0)

#define SUNXI_GPIO_BASE		(0x01C20800)
#define GPIO_BASE_BP		(0x01C20000)

#define	NUM_PINS		32

static int wiringBPMode = WPI_MODE_UNINITIALISED;

static volatile uint32_t *gpio;

static int pinModes[278];

static int BP_PIN_MASK[9][32] = {
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PA
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PB
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PC
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PD
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PE
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PF
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PG
	{0, 1, 2, 3, -1, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PH
	{-1, -1, -1, 3, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, -1, 16, 17, 18, 19, 20, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, //PI
};

static int edge[64] = {
	-1, -1, -1, -1, -1, -1, -1, 7,
	8, 9, 10, 11, -1,-1, 14, 15,
	-1, 17, 18, -1, -1, 21, 22, 23,
	24, 25, -1, 27, 28, -1, 30, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static int sysFds[64] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static int *pinToGpio;

static int pinToGpioR2[64] = {
	17, 18, 27, 22, 23, 24, 25, 4,	// From the Original Wiki - GPIO 0 through 7:	wpi  0 -  7
	2, 3,							// I2C  - SDA0, SCL0							wpi  8 -  9
	8, 7,							// SPI  - CE1, CE0								wpi	10 - 11
	10, 9, 11, 						// SPI  - MOSI, MISO, SCLK						wpi 12 - 14
	14, 15,							// UART - Tx, Rx								wpi 15 - 16
	28, 29, 30, 31,					// New GPIOs 8 though 11						wpi 17 - 20
	// Padding:
						-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 31
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 47
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 63
};

static int pinToGpioR3[64] = {
	275, 226,
	274, 273,
	244, 245,
	272, 259,
	53, 52,
	266, 270,
	268, 269,
	267, 224,
	225, 229,
	277, 227,
	276,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 31
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
};

static int pinToBCMR3[64] = {
	53, 52,
	53, 52,
	259, -1,
	-1, 270,
	266, 269, //9
	268, 267,
	-1, -1,
	224, 225,
	-1, 275,
	226, -1, //19
	-1,
	274, 273, 244, 245, 272, -1, 274, 229, 277, 227, // ... 31
	276, -1,-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
};

static int *physToGpio;

static int physToGpioR3[64] = {
	-1,  // 0
	-1, -1, // 1, 2
	53, -1,
	52, -1,
	259, 224,
	-1, 225,
	275, 226,
	274, -1,
	273, 244,
	-1, 245,
	268, -1,
	269, 272,
	267, 266,
	-1, 270, // 25, 26
	-1, -1,
	229, 277,
	227, 276,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 48
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
};

int bananapiValidGPIO(int pin) {
	if(pinToGpio[pin] != -1) {
		return 0;
	}
	return -1;
}

static uint32_t readl(uint32_t addr) {
	uint32_t val = 0;
	uint32_t mmap_base = (addr & ~MAP_MASK);
	uint32_t mmap_seek = ((addr - mmap_base) >> 2);
	val = *(gpio + mmap_seek);
	return val;
}

static void writel(uint32_t val, uint32_t addr) {
	uint32_t mmap_base = (addr & ~MAP_MASK);
	uint32_t mmap_seek = ((addr - mmap_base) >> 2);
	*(gpio + mmap_seek) = val;
}

static int changeOwner(char *file) {
	uid_t uid = getuid();
	uid_t gid = getgid();

	if(chown(file, uid, gid) != 0) {
		if(errno == ENOENT)	{
			wiringXLog(LOG_ERR, "bananapi->changeOwner: File not present: %s", file);
			return -1;
		} else {
			wiringXLog(LOG_ERR, "bananapi->changeOwner: Unable to change ownership of %s: %s", file, strerror (errno));
			return -1;
		}
	}

	return 0;
}

static int bananapiISR(int pin, int mode) {
	int i = 0, fd = 0, match = 0, count = 0;
	int npin = pinToGpioR2[pin];
	const char *sMode = NULL;
	char path[35], c, line[120];
	FILE *f = NULL;

	if(bananapiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "bananapi->isr: Invalid pin number %d", pin);
		return -1;
	}

	pinModes[pin] = SYS;

	if(mode == INT_EDGE_FALLING) {
		sMode = "falling" ;
	} else if(mode == INT_EDGE_RISING) {
		sMode = "rising" ;
	} else if(mode == INT_EDGE_BOTH) {
		sMode = "both";
	} else {
		wiringXLog(LOG_ERR, "bananapi->isr: Invalid mode. Should be INT_EDGE_BOTH, INT_EDGE_RISING, or INT_EDGE_FALLING");
		return -1;
	}

	if(edge[npin] == -1) {
		wiringXLog(LOG_ERR, "bananapi->isr: Invalid GPIO: %d", pin);
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", npin);
	if((fd = open(path, O_RDWR)) < 0) {
		if((f = fopen("/sys/class/gpio/export", "w")) == NULL) {
			wiringXLog(LOG_ERR, "bananapi->isr: Unable to open GPIO export interface");
			return -1;
		}

		fprintf(f, "%d\n", npin);
		fclose(f);
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", npin);
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "bananapi->isr: Unable to open GPIO direction interface for pin %d: %s", pin, strerror(errno));
		return -1;
	}

	fprintf(f, "in\n");
	fclose(f);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", npin);
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "bananapi->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
		return -1;
	}

	if(strcasecmp(sMode, "none") == 0) {
		fprintf(f, "none\n");
	} else if(strcasecmp(sMode, "rising") == 0) {
		fprintf(f, "rising\n");
	} else if(strcasecmp(sMode, "falling") == 0) {
		fprintf(f, "falling\n");
	} else if(strcasecmp (sMode, "both") == 0) {
		fprintf(f, "both\n");
	} else {
		wiringXLog(LOG_ERR, "bananapi->isr: Invalid mode: %s. Should be rising, falling or both", sMode);
		return -1;
	}
	fclose(f);

	if((f = fopen(path, "r")) == NULL) {
		wiringXLog(LOG_ERR, "bananapi->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
		return -1;
	}

	while(fgets(line, 120, f) != NULL) {
		if(strstr(line, sMode) != NULL) {
			match = 1;
			break;
		}
	}
	fclose(f);

	if(match == 0) {
		wiringXLog(LOG_ERR, "bananapi->isr: Failed to set interrupt edge to %s", sMode);
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", npin);
	if((sysFds[pin] = open(path, O_RDONLY)) < 0) {
		wiringXLog(LOG_ERR, "bananapi->isr: Unable to open GPIO value interface: %s", strerror(errno));
		return -1;
	}
	changeOwner(path);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", npin);
	changeOwner(path);

	ioctl(fd, FIONREAD, &count);
	for(i=0; i<count; ++i) {
		read(fd, &c, 1);
	}
	close(fd);

	return 0;
}

static int bananapiWaitForInterrupt(int pin, int ms) {
	int x = 0;
	uint8_t c = 0;
	struct pollfd polls;

	if(bananapiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "bananapi->waitForInterrupt: Invalid pin number %d", pin);
		return -1;
	}

	if(pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "bananapi->waitForInterrupt: Trying to read from pin %d, but it's not configured as interrupt", pin);
		return -1;
	}

	if(sysFds[pin] == -1) {
		wiringXLog(LOG_ERR, "bananapi->waitForInterrupt: GPIO %d not set as interrupt", pin);
		return -1;
	}

	polls.fd = sysFds[pin];
	polls.events = POLLPRI;

	x = poll(&polls, 1, ms);

	/* Don't react to signals */
	if(x == -1 && errno == EINTR) {
		x = 0;
	}

	(void)read(sysFds[pin], &c, 1);
	lseek(sysFds[pin], 0, SEEK_SET);

	return x;
}

static int piBoardRev(void) {
	FILE *cpuFd;
	char line[120];
	char *d;

	memset(line, '\0', 120);
	
	if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
		wiringXLog(LOG_ERR, "bananapi->identify: Unable open /proc/cpuinfo");
		return -1;
	}

	while(fgets(line, 120, cpuFd) != NULL) {
		if(strncmp(line, "Hardware", 8) == 0) {
			break;
		}
	}

	fclose(cpuFd);

	if(strlen(line) == 0) {
		return -1;
	}

	if(strncmp(line, "Hardware", 8) != 0) {
		wiringXLog(LOG_ERR, "bananapi->identify: /proc/cpuinfo has no hardware line");
		return -1;
	}

	for(d = &line[strlen(line) - 1]; (*d == '\n') || (*d == '\r') ; --d)
		*d = 0 ;

	if(strstr(line, "sun7i") != NULL || strstr(line, "sun4i") != NULL) {
		return 0;
	} else {
		return -1;
	}
}

static int setup(void)	{
	int fd;
	int boardRev;

	boardRev = piBoardRev();
	if(boardRev == 0) {
		pinToGpio = pinToGpioR2;
		physToGpio = physToGpioR3;
	}

	if((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0) {
		wiringXLog(LOG_ERR, "bananapi->setup: Unable to open /dev/mem");
		return -1;
	}

	if(boardRev == 0) {
		gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE_BP);

		if((int32_t)gpio == -1) {
			wiringXLog(LOG_ERR, "bananapi->setup: mmap (GPIO) failed");
			return -1;
		}
	}

	wiringBPMode = WPI_MODE_PINS;

	return 0;
}

static int bananapiDigitalRead(int pin) {
	uint32_t regval = 0, phyaddr = 0;
	int bank = 0, i = 0;

	if(bananapiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "bananapi->digitalRead: Invalid pin number %d", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {
		if(wiringBPMode == WPI_MODE_PINS)
			pin = pinToGpioR3[pin];
		else if (wiringBPMode == WPI_MODE_PHYS)
			pin = physToGpioR3[pin];
		else if(wiringBPMode == WPI_MODE_GPIO)
			pin = pinToBCMR3[pin]; //need map A20 to bcm
		else
			return -1;

		regval = 0;
		bank = pin >> 5;
		i = pin - (bank << 5);
		phyaddr = SUNXI_GPIO_BASE + (bank * 36) + 0x10; // +0x10 -> data reg

		if(BP_PIN_MASK[bank][i] != -1) {
			if(pinModes[pin] != INPUT && pinModes[pin] != SYS) {
				wiringXLog(LOG_ERR, "bananapi->digitalRead: Trying to write to pin %d, but it's not configured as input", pin);
				return -1;
			}

			regval = readl(phyaddr);
			regval = regval >> i;
			regval &= 1;
			return regval;
		}
	}
	return 0;
}

static int bananapiDigitalWrite(int pin, int value) {
	uint32_t regval = 0, phyaddr = 0;
	int bank = 0, i = 0;

	if(bananapiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "bananapi->digitalWrite: Invalid pin number %d", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {
		if(wiringBPMode == WPI_MODE_PINS)
			pin = pinToGpioR3[pin];
		else if (wiringBPMode == WPI_MODE_PHYS)
			pin = physToGpioR3[pin];
		else if(wiringBPMode == WPI_MODE_GPIO)
			pin = pinToBCMR3[pin]; //need map A20 to bcm
		else
			return -1;

		regval = 0;
		bank = pin >> 5;
		i = pin - (bank << 5);
		phyaddr = SUNXI_GPIO_BASE + (bank * 36) + 0x10; // +0x10 -> data reg

		if(BP_PIN_MASK[bank][i] != -1) {
			if(pinModes[pin] != OUTPUT) {
				wiringXLog(LOG_ERR, "bananapi->digitalWrite: Trying to write to pin %d, but it's not configured as output", pin);
				return -1;
			}
			regval = readl(phyaddr);

			if(value == LOW) {
				regval &= ~(1 << i);
				writel(regval, phyaddr);
				regval = readl(phyaddr);
			} else {
				regval |= (1 << i);
				writel(regval, phyaddr);
				regval = readl(phyaddr);
			}
		}
	}
	return 0;
}

static int bananapiPinMode(int pin, int mode) {
	uint32_t regval = 0, phyaddr = 0;
	int bank = 0, i = 0, offset = 0;

	if(bananapiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "bananapi->pinMode: Invalid pin number %d", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {

		if(wiringBPMode == WPI_MODE_PINS)
			pin = pinToGpioR3[pin] ;
		else if(wiringBPMode == WPI_MODE_PHYS)
			pin = physToGpioR3[pin] ;
		else if(wiringBPMode == WPI_MODE_GPIO)
			pin = pinToBCMR3[pin]; //need map A20 to bcm
		else
			return -1;

		regval = 0;
		bank = pin >> 5;
		i = pin - (bank << 5);
		offset = ((i - ((i >> 3) << 3)) << 2);
		phyaddr = SUNXI_GPIO_BASE + (bank * 36) + ((i >> 3) << 2);

		if(BP_PIN_MASK[bank][i] != -1) {
			pinModes[pin] = mode;

			regval = readl(phyaddr);

			if(mode == INPUT) {
				regval &= ~(7 << offset);
				writel(regval, phyaddr);
				regval = readl(phyaddr);
			} else if(mode == OUTPUT) {
			   regval &= ~(7 << offset);
			   regval |= (1 << offset);
			   writel(regval, phyaddr);
			   regval = readl(phyaddr);
			} else {
				return -1;
			}
		}
	}
	return 0;
}

static int bananapiGC(void) {
	int i = 0, fd = 0;
	char path[35];
	FILE *f = NULL;

	for(i=0;i<NUM_PINS;i++) {
		if(pinModes[i] == OUTPUT) {
			pinMode(i, INPUT);
		} else if(pinModes[i] == SYS) {
			sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpioR2[i]);
			if((fd = open(path, O_RDWR)) > 0) {
				if((f = fopen("/sys/class/gpio/unexport", "w")) == NULL) {
					wiringXLog(LOG_ERR, "bananapi->gc: Unable to open GPIO unexport interface: %s", strerror(errno));
				}

				fprintf(f, "%d\n", pinToGpioR2[i]);
				fclose(f);
				close(fd);
			}
		}
		if(sysFds[i] > 0) {
			close(sysFds[i]);
		}
	}

	if(gpio) {
		munmap((void *)gpio, BLOCK_SIZE);
	}
	return 0;
}

#ifndef __FreeBSD__
static int bananapiI2CRead(int fd) {
	return i2c_smbus_read_byte(fd);
}

static int bananapiI2CReadReg8(int fd, int reg) {
	return i2c_smbus_read_byte_data(fd, reg);
}

static int bananapiI2CReadReg16(int fd, int reg) {
	return i2c_smbus_read_word_data(fd, reg);
}

static int bananapiI2CWrite(int fd, int data) {
	return i2c_smbus_write_byte(fd, data);
}

static int bananapiI2CWriteReg8(int fd, int reg, int data) {
	return i2c_smbus_write_byte_data(fd, reg, data);
}

static int bananapiI2CWriteReg16(int fd, int reg, int data) {
	return i2c_smbus_write_word_data(fd, reg, data);
}

static int bananapiI2CSetup(int devId) {
	int rev = 0, fd = 0;
	const char *device = NULL;

	if((rev = piBoardRev()) < 0) {
		wiringXLog(LOG_ERR, "bananapi->I2CSetup: Unable to determine Pi board revision");
		return -1;
	}

	if(rev == 0)
		device = "/dev/i2c-2";
	else
		device = "/dev/i2c-3";

	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "bananapi->I2CSetup: Unable to open %s", device);
		return -1;
	}

	if(ioctl(fd, I2C_SLAVE, devId) < 0) {
		wiringXLog(LOG_ERR, "bananapi->I2CSetup: Unable to set %s to slave mode", device);
		return -1;
	}

	return fd;
}
#endif

void bananapiInit(void) {

	memset(pinModes, -1, 278);

	platform_register(&bananapi, "bananapi");
	bananapi->setup=&setup;
	bananapi->pinMode=&bananapiPinMode;
	bananapi->digitalWrite=&bananapiDigitalWrite;
	bananapi->digitalRead=&bananapiDigitalRead;
	bananapi->identify=&piBoardRev;
	bananapi->isr=&bananapiISR;
	bananapi->waitForInterrupt=&bananapiWaitForInterrupt;
#ifndef __FreeBSD__
	bananapi->I2CRead=&bananapiI2CRead;
	bananapi->I2CReadReg8=&bananapiI2CReadReg8;
	bananapi->I2CReadReg16=&bananapiI2CReadReg16;
	bananapi->I2CWrite=&bananapiI2CWrite;
	bananapi->I2CWriteReg8=&bananapiI2CWriteReg8;
	bananapi->I2CWriteReg16=&bananapiI2CWriteReg16;
	bananapi->I2CSetup=&bananapiI2CSetup;
#endif
	bananapi->gc=&bananapiGC;
	bananapi->validGPIO=&bananapiValidGPIO;
}
