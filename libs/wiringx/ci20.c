/*
	Copyright (c) 	2015 Gary Sims
					2014 CurlyMo <curlymoo1@gmail.com>
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
#include "ci20.h"

#define NUM_PINS	16

#define	GPIO_BASE	0x10010000
#define PAGE_SIZE	0x1000
#define PAGE_MASK 	(PAGE_SIZE - 1)

#define PIN 	0x00 // PORT PIN Level Register
#define INTC	0x18 // PORT Interrupt Clear Register
#define MSKS	0x24 // PORT Interrupt Mask Set Register
#define PAT1S	0x34 // PORT Pattern 1 Set Register
#define PAT1C	0x38 // PORT Pattern 1 Clear Register
#define PAT0	0x40 // PORT Pattern 0 Register
#define PAT0S 	0x44 // PORT Pattern 0 Set Register
#define PAT0C 	0x48 // PORT Pattern 0 Clear  Register

static int pinModes[NUM_PINS];

static int pinToGpio[NUM_PINS] = {
		124, // wiringX # 0 - Physical pin  7 - GPIO 1
		122, // wiringX # 1 - Physical pin  11 - GPIO 2
		123, // wiringX # 2 - Physical pin  13 - GPIO 3
		125, // wiringX # 3 - Physical pin  15 - GPIO 4
		161, // wiringX # 4 - Physical pin  16 - GPIO 5
		162, // wiringX # 5 - Physical pin  18 - GPIO 6
		136, // wiringX # 6 - Physical pin  22 - GPIO 7
		126, // wiringX # 7 - Physical pin  3 - IC21_SDA
		127, // wiringX # 8 - Physical pin  5 - I2C1_SCK
		144, // wiringX # 9 - Physical pin  24 - SSI1_CE0
		146, // wiringX # 10 - Physical pin  26 - SSI1_CE1
		145, // wiringX # 11 - Physical pin  19 - SSI0_DT
		142, // wiringX # 12 - Physical pin  21 - SSI0_DR
		143, // wiringX # 13 - Physical pin  23 - SSI0_CLK
		163, // wiringX # 14 - Physical pin  8 - UART0_TXD
		160, // wiringX # 15 - Physical pin  10 - UART0_RXD
};

static int sysFds[64] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

#ifndef __FreeBSD__
/* SPI Bus Parameters */
static uint8_t     spiMode   = 0;
static uint8_t     spiBPW    = 8;
static uint16_t    spiDelay  = 0;
static uint32_t    spiSpeeds[2];
static int         spiFds[2];
#endif

static volatile unsigned char *gpio;

static unsigned int gpioReadl(unsigned int gpio_offset) {
	return *(unsigned int *)(gpio + gpio_offset);
}

static void gpioWritel(unsigned int gpio_offset, unsigned int gpio_val) {
	*(unsigned int *)(gpio + gpio_offset) = gpio_val;
}

int ci20ValidGPIO(int pin) {
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
			wiringXLog(LOG_ERR, "ci20->changeOwner: File not present: %s", file);
			return -1;
		} else {
			wiringXLog(LOG_ERR, "ci20->changeOwner: Unable to change ownership of %s: %s", file, strerror (errno));
			return -1;
		}
	}

	return 0;
}

static int identify(void) {
	FILE *cpuFd;
	char line[120], revision[120], hardware[120], name[120];

	if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
		wiringXLog(LOG_ERR, "ci20->identify: Unable open /proc/cpuinfo");
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

	if(strstr(name, "CI20") != NULL) {
		return 0;
	} else {
		return -1;
	}
}

static int setup(void) {
	int fd;

	if((fd = open("/dev/mem", O_RDWR | O_SYNC )) < 0) {
		wiringXLog(LOG_ERR, "ci20->setup: Unable to open /dev/mem");
		return -1;
	}
	off_t addr = GPIO_BASE & ~PAGE_MASK;
	gpio = (unsigned char *)mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, addr);

	if((int32_t)gpio == -1) {
		wiringXLog(LOG_ERR, "ci20->setup: mmap (GPIO) failed");
		return -1;
	}

	return 0;
}

static int ci20DigitalRead(int pin) {
	// p is the port number (0,1,2,3,4,5)
	// o is the pin offset (0-31) inside the port
	// n is the absolute number of a pin (0-127), regardless of the port
	unsigned int p, o, n;
	unsigned int r;

	n = pinToGpio[pin];
	p = (n) / 32;
	o = (n) % 32;

	r = gpioReadl((PIN + (p)*0x100));

	if(((r) & (1 << (o))) == 0) {
		return LOW;
	} else {
		return HIGH;
	}
}

static int ci20DigitalWrite(int pin, int value) {
	// p is the port number (0,1,2,3,4,5)
	// o is the pin offset (0-31) inside the port
	// n is the absolute number of a pin (0-127), regardless of the port
	unsigned int p, o, n;

	n = pinToGpio[pin];
	p = (n) / 32;
	o = (n) % 32;

	if(value==0) {
		gpioWritel((PAT0C + (p)*0x100), (1 << (o)));
	} else {
		gpioWritel((PAT0S + (p)*0x100), (1 << (o)));
	}

	return 0;
}

static int ci20PinMode(int pin, int mode) {
	// p is the port number (0,1,2,3,4,5)
	// o is the pin offset (0-31) inside the port
	// n is the absolute number of a pin (0-127), regardless of the port
	unsigned int p, o, n;

	n = pinToGpio[pin];
	p = (n) / 32;
	o = (n) % 32;

	if(mode==INPUT) {
		gpioWritel((INTC + (p)*0x100), (1 << (o)));
		gpioWritel((MSKS + (p)*0x100), (1 << (o)));
		gpioWritel((PAT1S + (p)*0x100), (1 << (o)));
		gpioWritel((PAT0C + (p)*0x100), (1 << (o)));
	} else {
		gpioWritel((INTC + (p)*0x100), (1 << (o)));
		gpioWritel((MSKS + (p)*0x100), (1 << (o)));
		gpioWritel((PAT1C + (p)*0x100), (1 << (o)));
		gpioWritel((PAT0C + (p)*0x100), (1 << (o)));
	}
	pinModes[pin] = mode;

	return 0;
}

static int ci20ISR(int pin, int mode) {
	int i = 0, fd = 0, match = 0, count = 0;
	const char *sMode = NULL;
	char path[35], c, line[120];
	FILE *f = NULL;

	if(ci20ValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "ci20->isr: Invalid pin number %d", pin);
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
		wiringXLog(LOG_ERR, "ci20->isr: Invalid mode. Should be INT_EDGE_BOTH, INT_EDGE_RISING, or INT_EDGE_FALLING");
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpio[pin]);
	fd = open(path, O_RDWR);

	if(fd < 0) {
		if((f = fopen("/sys/class/gpio/export", "w")) == NULL) {
			wiringXLog(LOG_ERR, "ci20->isr: Unable to open GPIO export interface: %s", strerror(errno));
			return -1;
		}

		fprintf(f, "%d\n", pinToGpio[pin]);
		fclose(f);
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", pinToGpio[pin]);
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "ci20->isr: Unable to open GPIO direction interface for pin %d: %s", pin, strerror(errno));
		return -1;
	}

	fprintf(f, "in\n");
	fclose(f);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGpio[pin]);
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "ci20->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
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
		wiringXLog(LOG_ERR, "ci20->isr: Invalid mode: %s. Should be rising, falling or both", sMode);
		return -1;
	}
	fclose(f);

	if((f = fopen(path, "r")) == NULL) {
		wiringXLog(LOG_ERR, "ci20->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
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
		wiringXLog(LOG_ERR, "ci20->isr: Failed to set interrupt edge to %s", sMode);
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", pinToGpio[pin]);
	if((sysFds[pin] = open(path, O_RDONLY)) < 0) {
		wiringXLog(LOG_ERR, "ci20->isr: Unable to open GPIO value interface: %s", strerror(errno));
		return -1;
	}
	changeOwner(path);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinToGpio[pin]);
	changeOwner(path);

	ioctl(fd, FIONREAD, &count);
	for(i=0; i<count; ++i) {
		read(fd, &c, 1);
	}
	close(fd);

	return 0;
}

static int ci20WaitForInterrupt(int pin, int ms) {
	int x = 0;
	uint8_t c = 0;
	struct pollfd polls;

	if(ci20ValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "ci20->waitForInterrupt: Invalid pin number %d", pin);
		return -1;
	}

	if(pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "ci20->waitForInterrupt: Trying to read from pin %d, but it's not configured as interrupt", pin);
		return -1;
	}

	if(sysFds[pin] == -1) {
		wiringXLog(LOG_ERR, "ci20->waitForInterrupt: GPIO %d not set as interrupt", pin);
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

static int ci20GC(void) {
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
					wiringXLog(LOG_ERR, "ci20->gc: Unable to open GPIO unexport interface: %s", strerror(errno));
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
		munmap((void *)gpio, PAGE_SIZE);
	}
	return 0;
}

#ifndef __FreeBSD__
static int ci20I2CRead(int fd) {
	return i2c_smbus_read_byte(fd);
}

static int ci20I2CReadReg8(int fd, int reg) {
	return i2c_smbus_read_byte_data(fd, reg);
}

static int ci20I2CReadReg16(int fd, int reg) {
	return i2c_smbus_read_word_data(fd, reg);
}

static int ci20I2CWrite(int fd, int data) {
	return i2c_smbus_write_byte(fd, data);
}

static int ci20I2CWriteReg8(int fd, int reg, int data) {
	return i2c_smbus_write_byte_data(fd, reg, data);
}

static int ci20I2CWriteReg16(int fd, int reg, int data) {
	return i2c_smbus_write_word_data(fd, reg, data);
}

static int ci20I2CSetup(int devId) {
	int fd = 0;
	const char *device = NULL;

	device = "/dev/i2c-0";

	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "ci20->I2CSetup: Unable to open %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, I2C_SLAVE, devId) < 0) {
		wiringXLog(LOG_ERR, "ci20->I2CSetup: Unable to set %s to slave mode: %s", device, strerror(errno));
		return -1;
	}

	return fd;
}

int ci20SPIGetFd(int channel) {
	return spiFds[channel & 1];
}

int ci20SPIDataRW(int channel, unsigned char *data, int len) {
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
		wiringXLog(LOG_ERR, "ci20->SPIDataRW: Unable to read/write from channel %d: %s", channel, strerror(errno));
		return -1;
	}
	return 0;
}

int ci20SPISetup(int channel, int speed) {
	int fd;
	const char *device = NULL;

	channel &= 1;

	if(channel == 0) {
		device = "/dev/spidev0.0";
	} else {
		device = "/dev/spidev0.1";
	}

	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "ci20->SPISetup: Unable to open device %s: %s", device, strerror(errno));
		return -1;
	}

	spiSpeeds[channel] = speed;
	spiFds[channel] = fd;

	if(ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "ci20->SPISetup: Unable to set write mode for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "ci20->SPISetup: Unable to set read mode for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "ci20->SPISetup: Unable to set write bits_per_word for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "ci20->SPISetup: Unable to set read bits_per_word for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "ci20->SPISetup: Unable to set write max_speed for device %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "ci20->SPISetup: Unable to set read max_speed for device %s: %s", device, strerror(errno));
		return -1;
	}

	return fd;
}
#endif

void ci20Init(void) {

	memset(pinModes, -1, NUM_PINS);

	platform_register(&ci20, "ci20");
	ci20->setup=&setup;
	ci20->pinMode=&ci20PinMode;
	ci20->digitalWrite=&ci20DigitalWrite;
	ci20->digitalRead=&ci20DigitalRead;
	ci20->identify=&identify;
	ci20->isr=&ci20ISR;
	ci20->waitForInterrupt=&ci20WaitForInterrupt;
#ifndef __FreeBSD__
	ci20->I2CRead=&ci20I2CRead;
	ci20->I2CReadReg8=&ci20I2CReadReg8;
	ci20->I2CReadReg16=&ci20I2CReadReg16;
	ci20->I2CWrite=&ci20I2CWrite;
	ci20->I2CWriteReg8=&ci20I2CWriteReg8;
	ci20->I2CWriteReg16=&ci20I2CWriteReg16;
	ci20->I2CSetup=&ci20I2CSetup;
	ci20->SPIGetFd=&ci20SPIGetFd;
	ci20->SPIDataRW=&ci20SPIDataRW;
	ci20->SPISetup=&ci20SPISetup;
#endif
	ci20->gc=&ci20GC;
	ci20->validGPIO=&ci20ValidGPIO;
}
