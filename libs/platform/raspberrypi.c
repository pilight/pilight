/*
	Copyright (c) 2014 CurlyMo <curlymoo1@gmail.com>

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

#include "log.h"
#include "wiringX.h"
#include "i2c-dev.h"
#include "raspberrypi.h"

#define	WPI_MODE_PINS		 0
#define	WPI_MODE_GPIO		 1
#define	WPI_MODE_GPIO_SYS	 2
#define	WPI_MODE_PHYS		 3
#define	WPI_MODE_PIFACE		 4
#define	WPI_MODE_UNINITIALISED	-1

#define	PI_GPIO_MASK	(0xFFFFFFC0)

#define NUM_PINS		17

#define	PI_MODEL_UNKNOWN	0
#define	PI_MODEL_A		1
#define	PI_MODEL_B		2
#define	PI_MODEL_BP		3
#define	PI_MODEL_CM		4

#define	PI_VERSION_UNKNOWN	0
#define	PI_VERSION_1		1
#define	PI_VERSION_1_1		2
#define	PI_VERSION_1_2		3
#define	PI_VERSION_2		4

#define	PI_MAKER_UNKNOWN	0
#define	PI_MAKER_EGOMAN		1
#define	PI_MAKER_SONY		2
#define	PI_MAKER_QISDA		3

#define BCM2708_PERI_BASE			   0x20000000
#define GPIO_PADS		(BCM2708_PERI_BASE + 0x00100000)
#define CLOCK_BASE		(BCM2708_PERI_BASE + 0x00101000)
#define GPIO_BASE		(BCM2708_PERI_BASE + 0x00200000)

#define	PAGE_SIZE		(4*1024)
#define	BLOCK_SIZE		(4*1024)

static int wiringPiMode = WPI_MODE_UNINITIALISED;

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

static int changeOwner(char *file) {
	uid_t uid = getuid();
	uid_t gid = getgid();

	if(chown(file, uid, gid) != 0) {
		if(errno == ENOENT)	{
			logprintf(LOG_ERR, "raspberrypi->changeOwner: File not present: %s", file);
			return -1;
		} else {
			logprintf(LOG_ERR, "raspberrypi->changeOwner: Unable to change ownership of %s", file);
			return -1;
		}
	}

	return 0;
}

static int piBoardRev(void) {
	FILE *cpuFd;
	char line[120], revision[120], hardware[120], name[120];
	char *c;
	static int boardRev = -1;

	if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
		logprintf(LOG_ERR, "raspberrypi->identify: Unable open /proc/cpuinfo");
		return -1;
	}

	while(fgets(line, 120, cpuFd) != NULL) {
		if(strncmp(line, "Revision", 8) == 0) {
			strcpy(revision, line);
		}
		if(strncmp(line, "Hardware", 8) == 0) {
			strcpy(hardware, line);
		}
	}

	fclose(cpuFd);

	sscanf(hardware, "Hardware%*[ \t]:%*[ ]%[a-zA-Z0-9 ./()]%*[\n]", name);

	if(strstr(name, "BCM2708") != NULL) {
		if(boardRev != -1) {
			return boardRev;
		}

		if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
			logprintf(LOG_ERR, "raspberrypi->identify: Unable to open /proc/cpuinfo");
			return -1;
		}

		while(fgets(line, 120, cpuFd) != NULL) {
			if(strncmp(line, "Revision", 8) == 0) {
				break;
			}
		}

		fclose(cpuFd);

		if(strncmp(line, "Revision", 8) != 0) {
			logprintf(LOG_ERR, "raspberrypi->identify: No \"Revision\" line");
			return -1;
		}

		for(c = &line[strlen(line) - 1] ; (*c == '\n') || (*c == '\r'); --c) {
			*c = 0;
		}

		for(c = line; *c; ++c) {
			if(isdigit(*c)) {
				break;
			}
		}

		if(!isdigit(*c)) {
			logprintf(LOG_ERR, "raspberrypi->identify: No numeric revision string");
			return -1;
		}

		if(strlen(c) < 4) {
			logprintf(LOG_ERR, "raspberrypi->identify: Bogus \"Revision\" line (too small)");
			return -1;
		}

		c = c + strlen(c) - 4;

		if((strcmp(c, "0002") == 0) || (strcmp(c, "0003") == 0)) {
			boardRev = 1;
		} else {
			boardRev = 2;
		}
		return boardRev;
	} else {
		return -1;
	}
}

static int piBoardId(int *model, int *rev, int *mem, int *maker, int *overVolted) {
	FILE *cpuFd ;
	char line [120] ;
	char *c ;

	(void)piBoardRev();	// Call this first to make sure all's OK. Don't care about the result.

	if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
		logprintf(LOG_ERR, "raspberrypi->piBoardId: Unable to open /proc/cpuinfo");
		return -1;
	}

	while(fgets (line, 120, cpuFd) != NULL) {
		if(strncmp (line, "Revision", 8) == 0) {
			break;
		}
	}

	fclose(cpuFd);

	if(strncmp(line, "Revision", 8) != 0) {
		logprintf(LOG_ERR, "raspberrypi->piBoardId: No \"Revision\" line");
		return -1;
	}

	// Chomp trailing CR/NL
	for(c = &line[strlen(line) - 1]; (*c == '\n') || (*c == '\r'); --c) {
		*c = 0;
	}

	// Scan to first digit
	for(c = line; *c; ++c) {
		if(isdigit(*c)) {
			break;
		}
	}

	// Make sure its long enough
	if(strlen(c) < 4) {
		logprintf(LOG_ERR, "raspberrypi->piBoardId: Bogus \"Revision\" line");
		return -1;
	}

	// If longer than 4, we'll assume it's been overvolted
	*overVolted = strlen(c) > 4;

	// Extract last 4 characters:
	c = c + strlen(c) - 4;

	// Fill out the replys as appropriate

	if(strcmp(c, "0002") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_1;
		*mem = 256;
		*maker = PI_MAKER_EGOMAN;
	} else if(strcmp(c, "0003") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_1_1;
		*mem = 256;
		*maker = PI_MAKER_EGOMAN;
	} else if(strcmp(c, "0004") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_2;
		*mem = 256;
		*maker = PI_MAKER_SONY;
	} else if(strcmp(c, "0005") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_2;
		*mem = 256;
		*maker = PI_MAKER_QISDA;
	} else if(strcmp(c, "0006") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_2;
		*mem = 256;
		*maker = PI_MAKER_EGOMAN;
	} else if(strcmp(c, "0007") == 0) {
		*model = PI_MODEL_A;
		*rev = PI_VERSION_2;
		*mem = 256;
		*maker = PI_MAKER_EGOMAN;
	} else if(strcmp(c, "0008") == 0) {
		*model = PI_MODEL_A;
		*rev = PI_VERSION_2;
		*mem = 256;
		*maker = PI_MAKER_SONY;
	} else if(strcmp(c, "0009") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_2;
		*mem = 256;
		*maker = PI_MAKER_QISDA;
	} else if(strcmp(c, "000d") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_2;
		*mem = 512;
		*maker = PI_MAKER_EGOMAN;
	} else if(strcmp(c, "000e") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_2;
		*mem = 512;
		*maker = PI_MAKER_SONY;
	} else if(strcmp(c, "000f") == 0) {
		*model = PI_MODEL_B;
		*rev = PI_VERSION_2;
		*mem = 512;
		*maker = PI_MAKER_EGOMAN;
	} else if(strcmp(c, "0010") == 0) {
		*model = PI_MODEL_BP;
		*rev = PI_VERSION_1_2;
		*mem = 512;
		*maker = PI_MAKER_SONY;
	} else if(strcmp(c, "0011") == 0) {
		*model = PI_MODEL_CM;
		*rev = PI_VERSION_1_2;
		*mem = 512;
		*maker = PI_MAKER_SONY;
	} else {
		*model = 0;
		*rev = 0;
		*mem = 0;
		*maker = 0;
	}

	return 0;
}

static int setup(void) {
	int fd;
	int boardRev;
	int model, rev, mem, maker, overVolted;

	boardRev = piBoardRev();

	if(boardRev == 1) {
		pinToGpio =  pinToGpioR1;
		physToGpio = physToGpioR1;
	} else {
		pinToGpio =  pinToGpioR2;
		physToGpio = physToGpioR2;
	}

	if((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0) {
		logprintf(LOG_ERR, "raspberrypi->setup: Unable to open /dev/mem");
		return -1;
	}

	gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);
	if((int32_t)gpio == -1) {
		logprintf(LOG_ERR, "raspberrypi->setup: mmap (GPIO) failed");
		return -1;
	}

	if(piBoardId(&model, &rev, &mem, &maker, &overVolted) == -1) {
		return -1;
	}
	if(model == PI_MODEL_CM) {
		wiringPiMode = WPI_MODE_GPIO;
	} else {
		wiringPiMode = WPI_MODE_PINS;
	}

	return 0;
}

static int raspberrypiDigitalRead(int pin) {
	if(pinModes[pin] != INPUT) {
		logprintf(LOG_ERR, "raspberrypi->digitalRead: Trying to write to pin %d, but it's not configured as input", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {
		if(wiringPiMode == WPI_MODE_PINS)
			pin = pinToGpio[pin] ;
		else if (wiringPiMode == WPI_MODE_PHYS)
			pin = physToGpio[pin] ;
		else if (wiringPiMode != WPI_MODE_GPIO)
			return -1;

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
		logprintf(LOG_ERR, "raspberrypi->digitalWrite: Trying to write to pin %d, but it's not configured as output", pin);
		return -1;
	}

	if((pin & PI_GPIO_MASK) == 0) {
		if(wiringPiMode == WPI_MODE_PINS)
			pin = pinToGpio[pin] ;
		else if(wiringPiMode == WPI_MODE_PHYS)
			pin = physToGpio[pin] ;
		else if(wiringPiMode != WPI_MODE_GPIO)
			return -1;

		if(value == LOW)
			*(gpio + gpioToGPCLR [pin]) = 1 << (pin & 31);
		else
			*(gpio + gpioToGPSET [pin]) = 1 << (pin & 31);
	}
	return 0;
}

static int raspberrypiPinMode(int pin, int mode) {
	int fSel, shift;

	if((pin & PI_GPIO_MASK) == 0) {
		pinModes[pin] = mode;
		if(wiringPiMode == WPI_MODE_PINS)
			pin = pinToGpio[pin];
		else if(wiringPiMode == WPI_MODE_PHYS)
			pin = physToGpio[pin];
		else if(wiringPiMode != WPI_MODE_GPIO)
			return -1;

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
	char path[35], c;

	pinModes[pin] = SYS;

	if(mode == INT_EDGE_FALLING) {
		sMode = "falling" ;
	} else if(mode == INT_EDGE_RISING) {
		sMode = "rising" ;
	} else if(mode == INT_EDGE_BOTH) {
		sMode = "both";
	} else {
		logprintf(LOG_ERR, "raspberrypi->isr: Invalid mode. Should be INT_EDGE_BOTH, INT_EDGE_RISING, or INT_EDGE_FALLING");
		return -1;
	}

	FILE *f = NULL;
	for(i=0;i<NUM_PINS;i++) {
		if(pin == i) {
			sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpio[i]);
			fd = open(path, O_RDWR);
			match = 1;
		}
	}

	if(!match) {
		logprintf(LOG_ERR, "raspberrypi->isr: Invalid GPIO: %d", pin);
		exit(0);
	}

	if(fd < 0) {
		if((f = fopen("/sys/class/gpio/export", "w")) == NULL) {
			logprintf(LOG_ERR, "raspberrypi->isr: Unable to open GPIO export interface");
			exit(0);
		}

		fprintf(f, "%d\n", pinToGpio[pin]);
		fclose(f);
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", pinToGpio[pin]);
	if((f = fopen(path, "w")) == NULL) {
		logprintf(LOG_ERR, "raspberrypi->isr: Unable to open GPIO direction interface for pin %d", pin);
		return -1;
	}

	fprintf(f, "in\n");
	fclose(f);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGpio[pin]);
	if((f = fopen(path, "w")) == NULL) {
		logprintf(LOG_ERR, "raspberrypi->isr: Unable to open GPIO edge interface for pin %d", pin);
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
		logprintf(LOG_ERR, "raspberrypi->isr: Invalid mode: %s. Should be rising, falling or both", sMode);
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpio[pin]);
	if((sysFds[pin] = open(path, O_RDONLY)) < 0) {
		logprintf(LOG_ERR, "raspberrypi->isr: Unable to open GPIO value interface");
		return -1;
	}
	changeOwner(path);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGpio[pin]);
	changeOwner(path);

	fclose(f);

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

	if(pinModes[pin] != SYS) {
		logprintf(LOG_ERR, "raspberrypi->waitForInterrupt: Trying to read from pin %d, but it's not configured as interrupt", pin);
		return -1;
	}

	if(sysFds[pin] == -1) {
		logprintf(LOG_ERR, "raspberrypi->waitForInterrupt: GPIO %d not set as interrupt", pin);
		return -1;
	}

	polls.fd = sysFds[pin];
	polls.events = POLLPRI;

	x = poll(&polls, 1, ms);

	(void)read(sysFds[pin], &c, 1);
	lseek(sysFds[pin], 0, SEEK_SET);

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
					logprintf(LOG_ERR, "raspberrypi->gc: Unable to open GPIO unexport interface");
				}

				fprintf(f, "%d\n", pinToGpio[i]);
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

	if((rev = piBoardRev ()) < 0) {
		logprintf(LOG_ERR, "raspberrypi->I2CSetup: Unable to determine Pi board revision");
		return -1;
	}

	if(rev == 1)
		device = "/dev/i2c-0";
	else
		device = "/dev/i2c-1";

	if((fd = open(device, O_RDWR)) < 0) {
		logprintf(LOG_ERR, "raspberrypi->I2CSetup: Unable to open %s", device);
		return -1;
	}

	if(ioctl(fd, I2C_SLAVE, devId) < 0) {
		logprintf(LOG_ERR, "raspberrypi->I2CSetup: Unable to set %s to slave mode", device);
		return -1;
	}

	return fd;
}

void raspberrypiInit(void) {

	memset(pinModes, -1, NUM_PINS);

	device_register(&raspberrypi, "raspberrypi");
	raspberrypi->setup=&setup;
	raspberrypi->pinMode=&raspberrypiPinMode;
	raspberrypi->digitalWrite=&raspberrypiDigitalWrite;
	raspberrypi->digitalRead=&raspberrypiDigitalRead;
	raspberrypi->identify=&piBoardRev;
	raspberrypi->isr=&raspberrypiISR;
	raspberrypi->waitForInterrupt=&raspberrypiWaitForInterrupt;
	raspberrypi->I2CRead=&raspberrypiI2CRead;
	raspberrypi->I2CReadReg8=&raspberrypiI2CReadReg8;
	raspberrypi->I2CReadReg16=&raspberrypiI2CReadReg16;
	raspberrypi->I2CWrite=&raspberrypiI2CWrite;
	raspberrypi->I2CWriteReg8=&raspberrypiI2CWriteReg8;
	raspberrypi->I2CWriteReg16=&raspberrypiI2CWriteReg16;
	raspberrypi->I2CSetup=&raspberrypiI2CSetup;
	raspberrypi->gc=&raspberrypiGC;
}
