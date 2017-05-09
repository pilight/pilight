/*
	Copyright (c) 2014 CurlyMo <curlymoo1@gmail.com>
								2012 Gordon Henderson

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

#include "wiringX.h"
#ifndef __FreeBSD__
	#include "i2c-dev.h"
#endif
#include "raspberrypi.h"

#define WPI_MODE_PINS 0
#define WPI_MODE_GPIO 1
#define WPI_MODE_GPIO_SYS 2
#define WPI_MODE_PHYS 3
#define WPI_MODE_PIFACE 4
#define WPI_MODE_UNINITIALISED -1

#define PI_GPIO_MASK (0xFFFFFFC0)

#define NUM_PINS 32

#define PI_MODEL_A 0
#define PI_MODEL_B 1
#define PI_MODEL_AP 2
#define PI_MODEL_BP 3
#define PI_MODEL_2 4
#define PI_ALPHA 5
#define PI_MODEL_CM 6
#define PI_MODEL_07 7
#define PI_MODEL_3 8
#define PI_MODEL_ZERO 9
#define PI_MODEL_CM3 10
#define PI_MODEL_ZERO_W 12

#define PI_VERSION_1 0
#define PI_VERSION_1_1 1
#define PI_VERSION_1_2 2
#define PI_VERSION_2 3

#define PI_MAKER_SONY 0
#define PI_MAKER_EGOMAN 1
#define PI_MAKER_EMBEST 2
#define PI_MAKER_UNKNOWN 3

static volatile unsigned int GPIO_BASE;

#define GPIO_PERI_BASE_OLD 0x20000000
#define GPIO_PERI_BASE_NEW 0x3F000000

static volatile unsigned int piGpioBase = 0;

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

static volatile uint32_t *gpio;

static int pinModes[NUM_PINS];

static uint8_t gpioToShift[] = {
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27,
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27,
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27,
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27,
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27,
};

static uint8_t gpioToGPFSEL[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};

static int *pinToGpio;

static int pinToGpioR1[64] = {
	17, 18, 21, 22, 23, 24, 25, 4,	// From the Original Wiki - GPIO 0 through 7:	wpi  0 -  7
	0,  1,							// I2C  - SDA1, SCL1							wpi  8 -  9
	8,  7,							// SPI  - CE1, CE0								wpi 10 - 11
	10,  9, 11, 					// SPI  - MOSI, MISO, SCLK						wpi 12 - 14
	14, 15,							// UART - Tx, Rx								wpi 15 - 16
	// Padding:
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 31
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 47
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 63
};

static int pinToGpioR2[64] = {
	17, 18, 27, 22, 23, 24, 25, 4,	// From the Original Wiki - GPIO 0 through 7:	wpi  0 -  7
	2, 3,							// I2C  - SDA0, SCL0							wpi  8 -  9
	8, 7,							// SPI  - CE1, CE0								wpi	 10 - 11
	10, 9, 11, 						// SPI  - MOSI, MISO, SCLK						wpi 12 - 14
	14, 15,							// UART - Tx, Rx								wpi 15 - 16
	28, 29, 30, 31,					// Rev 2: New GPIOs 8 though 11					wpi 17 - 20
	5, 6, 13, 19, 26,				// B+											wpi 21, 22, 23, 24, 25
	12, 16, 20, 21,					// B+											wpi 26, 27, 28, 29
	0, 1,							// B+											wpi 30, 31
	// Padding:
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 47
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 63
};

static int *physToGpio;

static int physToGpioR1[64] = {
	-1,		// 0
	-1, -1,	// 1, 2
	0, -1,
	1, -1,
	4, 14,
	-1, 15,
	17, 18,
	21, -1,
	22, 23,
	-1, 24,
	10, -1,
	9, 25,
	11,  8,
	-1,  7,	// 25, 26
	-1, -1, -1, -1, -1,	// ... 31
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 47
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	// ... 63
} ;

static int physToGpioR2[64] = {
	-1,		// 0
	-1, -1,	// 1, 2
	2, -1,
	3, -1,
	4, 14,
	-1, 15,
	17, 18,
	27, -1,
	22, 23,
	-1, 24,
	10, -1,
	9, 25,
	11,  8,
	-1,  7,	// 25, 26
// B+
	0,  1,
	5, -1,
	6, 12,
	13, -1,
	19, 16,
	26, 20,
	-1, 21,
// the P5 connector on the Rev 2 boards:
	-1, -1,
	-1, -1,
	-1, -1,
	-1, -1,
	-1, -1,
	28, 29,
	30, 31,
	-1, -1,
	-1, -1,
	-1, -1,
	-1, -1,
};

static uint8_t gpioToGPSET[] = {
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
} ;

static uint8_t gpioToGPCLR[] = {
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
};

static uint8_t gpioToGPLEV[] = {
	13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
};

static int sysFds[64] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/* SPI Bus Parameters */
#ifndef __FreeBSD__
static uint8_t     spiMode   = 0;
static uint8_t     spiBPW    = 8;
static uint16_t    spiDelay  = 0;
static uint32_t    spiSpeeds[2];
static int         spiFds[2];
#endif

int raspberrypiValidGPIO(int pin) {
	if(pinToGpio[pin] != -1) {
		return 0;
	}
	return -1;
}

static int changeOwner(char *file) {
	uid_t uid = getuid();
	uid_t gid = getgid();

	if(chown(file, uid, gid) != 0) {
		if(errno == ENOENT)	{
			wiringXLog(LOG_ERR, "raspberrypi->changeOwner: File not present: %s", file);
			return -1;
		} else {
			wiringXLog(LOG_ERR, "raspberrypi->changeOwner: Unable to change ownership of %s: %s", file, strerror (errno));
			return -1;
		}
	}

	return 0;
}

static int piGpioLayout(void) {
	FILE *cpuFd;
	char line[120];
	char *c ;
	static int gpioLayout = -1;

	if(gpioLayout != -1) {
		return gpioLayout;
	}

	if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: unable to open /proc/cpuinfo");
		return -1;
	}

	while(fgets(line, 120, cpuFd) != NULL) {
		if(strncmp(line, "Hardware", 8) == 0) {
			break;
		}
	}

	if(strncmp(line, "Hardware", 8) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: no \"Hardware\" line");
		return -1;
	}

	if(!(strstr(line, "BCM2708") || strstr (line, "BCM2709") || strstr (line, "BCM2835"))) {
		return -1;
	}

	rewind(cpuFd);
	while(fgets(line, 120, cpuFd) != NULL) {
		if(strncmp (line, "Revision", 8) == 0) {
			break;
		}
	}

	fclose(cpuFd);

	if(strncmp(line, "Revision", 8) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: no \"Revision\" line");
		return -1;
	}

	for(c = &line[strlen(line) - 1]; (*c == '\n') || (*c == '\r') ; --c) {
		*c = 0;
	}

	for(c = line; *c; ++c) {
		if(*c == ':') {
			break;
		}
	}

	if(*c != ':') {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: bogus \"Revision\" line (no colon)");
		return -1;
	}

	++c;
	while(isspace(*c)) {
		++c;
	}

	if(!isxdigit(*c)) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: bogus \"Revision\" line (no hex digit at start of revision)");
		return -1;
	}

	if(strlen(c) < 4) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: bogus revision line (too small)");
		return -1;
	}

	c = c + strlen(c) - 4;

	if((strcmp(c, "0002") == 0) || (strcmp(c, "0003") == 0)) {
		gpioLayout = 1;
	} else {
		gpioLayout = 2;
	}

	return gpioLayout;
}

static int piBoardRev(void) {
	return piGpioLayout();
}

static void piBoardId(int *model, int *rev, int *mem, int *maker, int *warranty) {
	FILE *cpuFd;
	char line[120];
	char *c;
	unsigned int revision = 0;
	int bRev = 0, bType = 0, /* bProc = 0, */ bMfg = 0, bMem = 0, bWarranty = 0;

	if((cpuFd = fopen ("/proc/cpuinfo", "r")) == NULL) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: unable to open /proc/cpuinfo");
		return;
	}

	while(fgets (line, 120, cpuFd) != NULL) {
		if(strncmp(line, "Revision", 8) == 0) {
			break;
		}
	}
	fclose(cpuFd) ;

	if(strncmp(line, "Revision", 8) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: no \"Revision\" line");
		return;
	}

	for(c = &line[strlen(line) - 1] ; (*c == '\n') || (*c == '\r') ; --c) {
		*c = 0;
	}

	for(c = line; *c; ++c) {
		if(*c == ':') {
			break;
		}
	}

	if(*c != ':') {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: bogus \"Revision\" line (no colon)");
		return;
	}

	++c;
	while(isspace(*c)) {
		++c;
	}

	if(!isxdigit(*c)) {
		wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: bogus \"Revision\" line (no hex digit at start of revision)");
		return;
	}

	revision = (unsigned int)strtol(c, NULL, 16);

	if((revision & (1 << 23)) != 0) {
		bRev = (revision & (0x0F << 0)) >> 0;
		bType = (revision & (0xFF << 4)) >> 4;
		// bProc = (revision & (0x0F << 12)) >> 12;
		bMfg = (revision & (0x0F << 16)) >> 16;
		bMem = (revision & (0x07 << 20)) >> 20;
		bWarranty = (revision & (0x03 << 24)) != 0;

		*model    = bType ;
		*rev      = bRev ;
		*mem      = bMem ;
		*maker    = bMfg  ;
		*warranty = bWarranty ;
	} else {
		if(!isdigit(*c)) {
			wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: bogus \"Revision\" line (no digit at start of revision)");
			return;
		}

		if(strlen(c) < 4) {
			wiringXLog(LOG_ERR, "raspberrypi->piGpioLayout: bogus \"Revision\" line (not long enough)");
			return;
		}

		*warranty = strlen (c) > 4 ;
		c = c + strlen (c) - 4 ;

		if(strcmp(c, "0002") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1;
			*mem = 0;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0003") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1_1;
			*mem = 0;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0004") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1_2;
			*mem = 0;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "0005") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1_2;
			*mem = 0;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0006") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1_2;
			*mem = 0;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0007") == 0) {
			*model = PI_MODEL_A;
			*rev = PI_VERSION_1_2;
			*mem = 0;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0008") == 0) {
			*model = PI_MODEL_A;
			*rev = PI_VERSION_1_2;
			*mem = 0;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "0009") == 0) {
			*model = PI_MODEL_A;
			*rev = PI_VERSION_1_2;
			*mem = 0;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "000d") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1_2;
			*mem = 1;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "000e") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1_2;
			*mem = 1;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "000f") == 0) {
			*model = PI_MODEL_B;
			*rev = PI_VERSION_1_2;
			*mem = 1;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0010") == 0) {
			*model = PI_MODEL_BP;
			*rev = PI_VERSION_1_2;
			*mem = 1;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "0013") == 0) {
			*model = PI_MODEL_BP;
			*rev = PI_VERSION_1_2;
			*mem = 1;
			*maker = PI_MAKER_EMBEST;
		} else if(strcmp(c, "0016") == 0) {
			*model = PI_MODEL_BP;
			*rev = PI_VERSION_1_2;
			*mem = 1;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "0019") == 0) {
			*model = PI_MODEL_BP;
			*rev = PI_VERSION_1_2;
			*mem = 1;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0011") == 0) {
			*model = PI_MODEL_CM;
			*rev = PI_VERSION_1_1;
			*mem = 1;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "0014") == 0) {
			*model = PI_MODEL_CM;
			*rev = PI_VERSION_1_1;
			*mem = 1;
			*maker = PI_MAKER_EMBEST;
		} else if(strcmp(c, "0017") == 0) {
			*model = PI_MODEL_CM;
			*rev = PI_VERSION_1_1;
			*mem = 1;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "001a") == 0) {
			*model = PI_MODEL_CM;
			*rev = PI_VERSION_1_1;
			*mem = 1;
			*maker = PI_MAKER_EGOMAN;
		} else if(strcmp(c, "0012") == 0) {
			*model = PI_MODEL_AP;
			*rev = PI_VERSION_1_1;
			*mem = 0;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "0015") == 0) {
			*model = PI_MODEL_AP;
			*rev = PI_VERSION_1_1;
			*mem = 1;
			*maker = PI_MAKER_EMBEST;
		} else if(strcmp(c, "0018") == 0) {
			*model = PI_MODEL_AP;
			*rev = PI_VERSION_1_1;
			*mem = 0;
			*maker = PI_MAKER_SONY;
		} else if(strcmp(c, "001b") == 0) {
			*model = PI_MODEL_AP;
			*rev = PI_VERSION_1_1;
			*mem = 0;
			*maker = PI_MAKER_EGOMAN;
		} else {
			*model = 0;
			*rev = 0;
			*mem = 0;
			*maker = 0;
		}
	}
}

static int setup(void) {
	int fd;
	int model, rev, mem, maker, overVolted;

	piBoardId(&model, &rev, &mem, &maker, &overVolted);

	if(model == 0 && rev == 0 && mem == 0 && maker == 0 && overVolted == 0) {
		return -1;
	}

	if(piGpioLayout() == 1) {
		pinToGpio = pinToGpioR1;
		physToGpio = physToGpioR1;
	} else {
		pinToGpio = pinToGpioR2;
		physToGpio = physToGpioR2;
	}

	switch(model) {
		case PI_MODEL_A:
		case PI_MODEL_B:
		case PI_MODEL_AP:
		case PI_MODEL_BP:
		case PI_ALPHA:
		case PI_MODEL_CM:
		case PI_MODEL_ZERO:
		case PI_MODEL_ZERO_W:
			piGpioBase = GPIO_PERI_BASE_OLD;
		break;
		default:
			piGpioBase = GPIO_PERI_BASE_NEW;
		break;
	}

#ifdef O_CLOEXEC
	if((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0) {
#else
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
#endif
		wiringXLog(LOG_ERR, "raspberrypi->setup: Unable to open /dev/mem: %s", strerror(errno));
		return -1;
	}

	GPIO_BASE = piGpioBase + 0x00200000;

	gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);
	if((int32_t)gpio == -1) {
		wiringXLog(LOG_ERR, "raspberrypi->setup: mmap (GPIO) failed: %s", strerror(errno));
		return -1;
	}

	return 0;
}

static int raspberrypiDigitalRead(int pin) {
	if(pinModes[pin] != INPUT && pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "raspberrypi->digitalRead: Trying to write to pin %d, but it's not configured as input", pin);
		return -1;
	}

	if(raspberrypiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->digitalRead: Invalid pin number %d", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {
		pin = pinToGpio[pin];

		if((*(gpio + gpioToGPLEV[pin]) & (1 << (pin & 31))) != 0) {
			return HIGH;
		} else {
			return LOW;
		}
	}
	return 0;
}

static int raspberrypiDigitalWrite(int pin, int value) {
	if(pinModes[pin] != OUTPUT) {
		wiringXLog(LOG_ERR, "raspberrypi->digitalWrite: Trying to write to pin %d, but it's not configured as output", pin);
		return -1;
	}

	if(raspberrypiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->digitalWrite: Invalid pin number %d", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {
		pin = pinToGpio[pin];

		if(value == LOW)
			*(gpio + gpioToGPCLR [pin]) = 1 << (pin & 31);
		else
			*(gpio + gpioToGPSET [pin]) = 1 << (pin & 31);
	}
	return 0;
}

static int raspberrypiPinMode(int pin, int mode) {
	int fSel, shift;

	if(raspberrypiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->pinMode: Invalid pin number %d", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {
		pinModes[pin] = mode;
		pin = pinToGpio[pin];

		fSel = gpioToGPFSEL[pin];
		shift = gpioToShift[pin];

		if(mode == INPUT) {
			*(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift));
		} else if(mode == OUTPUT) {
			*(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift)) | (1 << shift);
		}
	}
	return 0;
}

static int raspberrypiISR(int pin, int mode) {
	int i = 0, fd = 0, match = 0, count = 0;
	const char *sMode = NULL;
	char path[35], c, line[120];
	FILE *f = NULL;

	if(raspberrypiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->isr: Invalid pin number %d", pin);
		return -1;
	}

	if(mode == INT_EDGE_FALLING) {
		sMode = "falling" ;
	} else if(mode == INT_EDGE_RISING) {
		sMode = "rising" ;
	} else if(mode == INT_EDGE_BOTH) {
		sMode = "both";
	} else {
		wiringXLog(LOG_ERR, "raspberrypi->isr: Invalid mode. Should be INT_EDGE_BOTH, INT_EDGE_RISING, or INT_EDGE_FALLING");
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpio[pin]);
	fd = open(path, O_RDWR);

	if(fd < 0) {
		if((f = fopen("/sys/class/gpio/export", "w")) == NULL) {
			wiringXLog(LOG_ERR, "raspberrypi->isr: Unable to open GPIO export interface for pin %d: %s", pin, strerror(errno));
			return -1;
		}

		fprintf(f, "%d\n", pinToGpio[pin]);
		fclose(f);
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", pinToGpio[pin]);
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "raspberrypi->isr: Unable to open GPIO direction interface for pin %d: %s", pin, strerror(errno));
		close(fd);
		return -1;
	}

	fprintf(f, "in\n");
	fclose(f);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGpio[pin]);
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "raspberrypi->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
		close(fd);
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
		wiringXLog(LOG_ERR, "raspberrypi->isr: Invalid mode: %s. Should be rising, falling or both", sMode);
		close(fd);
		return -1;
	}
	fclose(f);

	if((f = fopen(path, "r")) == NULL) {
		wiringXLog(LOG_ERR, "raspberrypi->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
		close(fd);
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
		wiringXLog(LOG_ERR, "raspberrypi->isr: Failed to set interrupt edge to %s", sMode);
		close(fd);
		return -1;
	}

	if(sysFds[pin] == -1) {
		sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpio[pin]);
		if((sysFds[pin] = open(path, O_RDONLY)) < 0) {
			wiringXLog(LOG_ERR, "raspberrypi->isr: Unable to open GPIO value interface: %s", strerror(errno));
			close(fd);
			return -1;
		}
		changeOwner(path);
	}

	pinModes[pin] = SYS;

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGpio[pin]);
	changeOwner(path);

	ioctl(fd, FIONREAD, &count);
	for(i=0; i<count; ++i) {
		read(fd, &c, 1);
	}
	close(fd);

	return 0;
}

static int raspberrypiWaitForInterrupt(int pin, int ms) {
	int x = 0;
	uint8_t c = 0;
	struct pollfd polls;

	if(raspberrypiValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "raspberrypi->waitForInterrupt: Invalid pin number %d", pin);
		return -1;
	}

	if(pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "raspberrypi->waitForInterrupt: Trying to read from pin %d, but it's not configured as interrupt", pin);
		return -1;
	}

	if(sysFds[pin] == -1) {
		wiringXLog(LOG_ERR, "raspberrypi->waitForInterrupt: GPIO %d not set as interrupt", pin);
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

static int raspberrypiGC(void) {
	int i = 0, fd = 0;
	char path[35];
	FILE *f = NULL;

	for(i=0;i<NUM_PINS;i++) {
		if(pinModes[i] == OUTPUT) {
			pinMode(i, INPUT);
		} else if(pinModes[i] == SYS) {
			sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpio[i]);
			if((fd = open(path, O_RDWR)) > 0) {
				if((f = fopen("/sys/class/gpio/unexport", "w")) == NULL) {
					wiringXLog(LOG_ERR, "raspberrypi->gc: Unable to open GPIO unexport interface: %s", strerror(errno));
				}
				else {
					fprintf(f, "%d\n", pinToGpio[i]);
					fclose(f);
				}
				close(fd);
			}
		}
		if(sysFds[i] >= 0) {
			close(sysFds[i]);
			sysFds[i] = -1;
		}
	}

	if(gpio) {
		munmap((void *)gpio, BLOCK_SIZE);
	}
	return 0;
}

#ifndef __FreeBSD__
static int raspberrypiI2CRead(int fd) {
	return i2c_smbus_read_byte(fd);
}

static int raspberrypiI2CReadReg8(int fd, int reg) {
	return i2c_smbus_read_byte_data(fd, reg);
}

static int raspberrypiI2CReadReg16(int fd, int reg) {
	return i2c_smbus_read_word_data(fd, reg);
}

static int raspberrypiI2CWrite(int fd, int data) {
	return i2c_smbus_write_byte(fd, data);
}

static int raspberrypiI2CWriteReg8(int fd, int reg, int data) {
	return i2c_smbus_write_byte_data(fd, reg, data);
}

static int raspberrypiI2CWriteReg16(int fd, int reg, int data) {
	return i2c_smbus_write_word_data(fd, reg, data);
}

static int raspberrypiI2CSetup(int devId) {
	int rev = 0, fd = 0;
	const char *device = NULL;

	if((rev = piBoardRev()) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->I2CSetup: Unable to determine Pi board revision");
		return -1;
	}

	if(rev == 1)
		device = "/dev/i2c-0";
	else
		device = "/dev/i2c-1";

	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->I2CSetup: Unable to open %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, I2C_SLAVE, devId) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->I2CSetup: Unable to set %s to slave mode: %s", device, strerror(errno));
		return -1;
	}

	return fd;
}

int raspberrypiSPIGetFd(int channel) {
	return spiFds[channel & 1];
}

int raspberrypiSPIDataRW(int channel, unsigned char *data, int len) {
	struct spi_ioc_transfer spi;
	memset(&spi, 0, sizeof(spi)); // found at http://www.raspberrypi.org/forums/viewtopic.php?p=680665#p680665
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
		wiringXLog(LOG_ERR, "raspberrypi->SPIDataRW: Unable to read/write from channel %d: %s", channel, strerror(errno));
		return -1;
	}
	return 0;
}

int raspberrypiSPISetup(int channel, int speed) {
	int fd;
	const char *device = NULL;

	channel &= 1;

	if(channel == 0) {
		device = "/dev/spidev0.0";
	} else {
		device = "/dev/spidev0.1";
	}

	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->SPISetup: Unable to open device %s: %s", device, strerror(errno));
		return -1;
	}

	spiSpeeds[channel] = speed;
	spiFds[channel] = fd;

	if(ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->SPISetup: Unable to set write mode for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->SPISetup: Unable to set read mode for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->SPISetup: Unable to set write bits_per_word for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->SPISetup: Unable to set read bits_per_word for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->SPISetup: Unable to set write max_speed for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "raspberrypi->SPISetup: Unable to set read max_speed for device %s: %s", device, strerror(errno));
		return -1;
	}

	return fd;
}
#endif

void raspberrypiInit(void) {

	memset(pinModes, -1, NUM_PINS);

	platform_register(&raspberrypi, "raspberrypi");
	raspberrypi->setup=&setup;
	raspberrypi->pinMode=&raspberrypiPinMode;
	raspberrypi->digitalWrite=&raspberrypiDigitalWrite;
	raspberrypi->digitalRead=&raspberrypiDigitalRead;
	raspberrypi->identify=&piBoardRev;
	raspberrypi->isr=&raspberrypiISR;
	raspberrypi->waitForInterrupt=&raspberrypiWaitForInterrupt;
#ifndef __FreeBSD__
	raspberrypi->I2CRead=&raspberrypiI2CRead;
	raspberrypi->I2CReadReg8=&raspberrypiI2CReadReg8;
	raspberrypi->I2CReadReg16=&raspberrypiI2CReadReg16;
	raspberrypi->I2CWrite=&raspberrypiI2CWrite;
	raspberrypi->I2CWriteReg8=&raspberrypiI2CWriteReg8;
	raspberrypi->I2CWriteReg16=&raspberrypiI2CWriteReg16;
	raspberrypi->I2CSetup=&raspberrypiI2CSetup;
	raspberrypi->SPIGetFd=&raspberrypiSPIGetFd;
	raspberrypi->SPIDataRW=&raspberrypiSPIDataRW;
	raspberrypi->SPISetup=&raspberrypiSPISetup;
#endif
	raspberrypi->gc=&raspberrypiGC;
	raspberrypi->validGPIO=&raspberrypiValidGPIO;
}
