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
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#ifndef __FreeBSD__
	#include <linux/spi/spidev.h>
#endif
#include <signal.h>
#include <setjmp.h>

#include "wiringX.h"
#ifndef __FreeBSD__
	#include "i2c-dev.h"
#endif
#include "hummingboard.h"

#define FUNC_GPIO								0x5
#define MX6Q_IOMUXC_BASE_ADDR		0x02000000
#define GPIO_MUX_REG						0x000e0224
#define GPIO_MUX_CTRL						0x000e05f4
#define NUM_PINS								8
#define MAP_SIZE 								1024*1024

volatile void *gpio = NULL;

static int pinModes[NUM_PINS];
static int sysFds[NUM_PINS] = { 0 };
static int pinsToGPIO[NUM_PINS] = { 73, 72, 71, 70, 194, 195, 67, 1 };
static int pinToBin[NUM_PINS] = { 9, 8, 7, 6, 2, 3, 3, 1 } ;
static int pinToMuxAddr[NUM_PINS] = { 0, 0, 0, 0, 4, 4, 4, 4 };

static int pinToGPIOAddr[NUM_PINS] = {
	0xa4000, 0xa4000, 0xa4000, 0xa4000, // GPIO 0, 1, 2, 3 --> GPIO3_DR
	0xb4000, 0xb4000, // GPIO 4, 5 --> GPIO7_DR
	0xa4000, // GPIO 6 --> GPIO3_DR
	0x9c000 // GPIO 7 --> GPIO1_DR
};

#ifndef __FreeBSD__
/* SPI Bus Parameters */
static uint8_t     spiMode   = 0;
static uint8_t     spiBPW    = 8;
static uint16_t    spiDelay  = 0;
static uint32_t    spiSpeeds[2];
static int         spiFds[2];
#endif

int hummingboardValidGPIO(int pin) {
	if(pin >= 0 && pin <= 7) {
		return 0;
	}
	return -1;
}

static int changeOwner(char *file) {
	uid_t uid = getuid();
	uid_t gid = getgid();

	if(chown(file, uid, gid) != 0) {
		if(errno == ENOENT)	{
			wiringXLog(LOG_ERR, "hummingboard->changeOwner: File not present: %s", file);
			return -1;
		} else {
			wiringXLog(LOG_ERR, "hummingboard->changeOwner: Unable to change ownership of %s: %s", file, strerror (errno));
			return -1;
		}
	}
	return 0;
}

static int setup(void) {
	int fd = 0;

#ifdef O_CLOEXEC
	if((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0) {
#else
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
#endif
		wiringXLog(LOG_ERR, "hummingboard->setup: Unable to open /dev/mem: %s", strerror(errno));
		return -1;
	}

	if((int32_t)(gpio = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, MX6Q_IOMUXC_BASE_ADDR)) == -1) {
		wiringXLog(LOG_ERR, "hummingboard->setup: mmap (GPIO) failed: %s", strerror (errno));
		close(fd);
		return -1;
	}

	return 0;
}

static int hummingboardPinMode(int pin, int direction) {
	if(!gpio) {
		wiringXLog(LOG_ERR, "hummingboard->pinMode: Please run wiringXSetup before running pinMode");
		return -1;
	}

	if(hummingboardValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "hummingboard->pinMode: Invalid pin number %d", pin);
		return -1;
	}

	*((unsigned long *)(gpio + GPIO_MUX_REG + pinToMuxAddr[pin] )) = FUNC_GPIO;
	*((unsigned long *)(gpio + GPIO_MUX_CTRL + pinToMuxAddr[pin] )) = 0xF;

	if(direction == INPUT) {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) = *((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) & ~(1 << pinToBin[pin]);
	} else if(direction == OUTPUT) {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) = *((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) | (1 << pinToBin[pin]);
	} else {
		wiringXLog(LOG_ERR, "hummingboard->pinMode: Pin modes can only be OUTPUT or INPUT");
		return -1;
	}
	pinModes[pin] = direction;
	return 0;
}

static int identify(void) {
	FILE *cpuFd;
	char line[120], revision[120], hardware[120], name[120];

	memset(revision, '\0', 120);
	memset(hardware, '\0', 120);

	if((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL) {
		wiringXLog(LOG_ERR, "hummingboard->identify: Unable open /proc/cpuinfo");
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

	if(strlen(hardware) == 0 || strlen(revision) == 0) {
		wiringXLog(LOG_ERR, "hummingboard->identify: /proc/cpuinfo has no hardware and revision line");
		return -1;
	}

	sscanf(hardware, "Hardware%*[ \t]:%*[ ]%[a-zA-Z0-9 ./()]%*[\n]", name);
	if(strstr(name, "Freescale i.MX6") != NULL
	   || strstr(name, "SolidRun i.MX6") != NULL
	   || strstr(name, "HummingBoard") != NULL) {
		return 0;
	} else {
		return -1;
	}
}

static int hummingboardDigitalWrite(int pin, int value) {
	if(pinModes[pin] != OUTPUT) {
		wiringXLog(LOG_ERR, "hummingboard->digitalWrite: Trying to write to pin %d, but it's not configured as output", pin);
		return -1;
	}

	if(hummingboardValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "hummingboard->digitalWrite: Invalid pin number %d", pin);
		return -1;
	}

	if(value) {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin])) = *((unsigned long *)(gpio + pinToGPIOAddr[pin])) | (1 << pinToBin[pin]);
	} else {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin])) = *((unsigned long *)(gpio + pinToGPIOAddr[pin])) & ~(1 << pinToBin[pin]);
	}

	return 0;
}

static int hummingboardDigitalRead(int pin) {
	if(pinModes[pin] != INPUT && pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "hummingboard->digitalRead: Trying to write to pin %d, but it's not configured as input", pin);
		return -1;
	}

	if(hummingboardValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "hummingboard->digitalRead: Invalid pin number %d", pin);
		return -1;
	}

	if((*((int *)(gpio + pinToGPIOAddr[pin] + 8)) & (1 << pinToBin[pin])) != 0) {
		return 1;
	} else {
		return 0;
	}
}

static int hummingboardISR(int pin, int mode) {
	int i = 0, fd = 0, match = 0, count = 0;
	const char *sMode = NULL;
	char path[35], c, line[120];
	pinModes[pin] = SYS;
	FILE *f = NULL;

	if(hummingboardValidGPIO(pin) != 0) {
		wiringXLog(LOG_ERR, "hummingboard->isr: Invalid pin number %d", pin);
		return -1;
	}

	if(mode == INT_EDGE_FALLING) {
		sMode = "falling" ;
	} else if(mode == INT_EDGE_RISING) {
		sMode = "rising" ;
	} else if(mode == INT_EDGE_BOTH) {
		sMode = "both";
	} else {
		wiringXLog(LOG_ERR, "hummingboard->isr: Invalid mode. Should be INT_EDGE_BOTH, INT_EDGE_RISING, or INT_EDGE_FALLING");
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", pinsToGPIO[pin]);
	fd = open(path, O_RDWR);

	if(fd < 0) {
		if((f = fopen("/sys/class/gpio/export", "w")) == NULL) {
			wiringXLog(LOG_ERR, "hummingboard->isr: Unable to open GPIO export interface: %s", strerror(errno));
			return -1;
		}

		fprintf(f, "%d\n", pinsToGPIO[pin]);
		fclose(f);
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", pinsToGPIO[pin]);
	if((f = fopen(path, "r")) == NULL) {
		wiringXLog(LOG_ERR, "hummingboard->isr: Unable to open GPIO direction interface for pin %d: %s", pin, strerror(errno));
		return -1;
	} else {
		fgets(line, 120, f);
		fclose(f);
	}

	if(strstr(line, "in") == NULL) {
		if((f = fopen(path, "w")) == NULL) {
			wiringXLog(LOG_ERR, "hummingboard->isr: Unable to open GPIO direction interface for pin %d: %s", pin, strerror(errno));
			return -1;
		}
		fprintf(f, "in");
		fclose(f);
	}

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinsToGPIO[pin]);
	if((f = fopen(path, "w")) == NULL) {
		wiringXLog(LOG_ERR, "hummingboard->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
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
		wiringXLog(LOG_ERR, "hummingboard->isr: Invalid mode: %s. Should be rising, falling or both", sMode);
		return -1;
	}
	fclose(f);

	if((f = fopen(path, "r")) == NULL) {
		wiringXLog(LOG_ERR, "hummingboard->isr: Unable to open GPIO edge interface for pin %d: %s", pin, strerror(errno));
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
		wiringXLog(LOG_ERR, "hummingboard->isr: Failed to set interrupt edge to %s", sMode);
		return -1;
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", pinsToGPIO[pin]);
	if((sysFds[pin] = open(path, O_RDONLY)) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->isr: Unable to open GPIO value interface: %s", strerror(errno));
		return -1;
	}
	changeOwner(path);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinsToGPIO[pin]);
	changeOwner(path);

	ioctl(fd, FIONREAD, &count);
	for(i=0; i<count; ++i) {
		read(fd, &c, 1);
	}
	close(fd);

	return 0;
}

static int hummingboardWaitForInterrupt(int pin, int ms) {
	int x = 0;
	uint8_t c = 0;
	struct pollfd polls;

	if(pinModes[pin] != SYS) {
		wiringXLog(LOG_ERR, "hummingboard->waitForInterrupt: Trying to read from pin %d, but it's not configured as interrupt", pin);
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

static int hummingboardGC(void) {
	int i = 0, fd = 0;
	char path[35];
	FILE *f = NULL;

	for(i=0;i<NUM_PINS;i++) {
		if(pinModes[i] == OUTPUT) {
			pinMode(i, INPUT);
		} else if(pinModes[i] == SYS) {
			sprintf(path, "/sys/class/gpio/gpio%d/value", pinsToGPIO[i]);
			if((fd = open(path, O_RDWR)) > 0) {
				if((f = fopen("/sys/class/gpio/unexport", "w")) == NULL) {
					wiringXLog(LOG_ERR, "hummingboard->gc: Unable to open GPIO unexport interface: %s", strerror(errno));
				}

				fprintf(f, "%d\n", pinsToGPIO[i]);
				fclose(f);
				close(fd);
			}
		}
		if(sysFds[i] > 0) {
			close(sysFds[i]);
		}
	}

	if(gpio) {
		munmap((void *)gpio, MAP_SIZE);
	}
	return 0;
}

#ifndef __FreeBSD__
static int hummingboardI2CRead(int fd) {
	return i2c_smbus_read_byte(fd);
}

static int hummingboardI2CReadReg8(int fd, int reg) {
	return i2c_smbus_read_byte_data(fd, reg);
}

static int hummingboardI2CReadReg16(int fd, int reg) {
	return i2c_smbus_read_word_data(fd, reg);
}

static int hummingboardI2CWrite(int fd, int data) {
	return i2c_smbus_write_byte(fd, data);
}

static int hummingboardI2CWriteReg8(int fd, int reg, int data) {
	return i2c_smbus_write_byte_data(fd, reg, data);
}

static int hummingboardI2CWriteReg16(int fd, int reg, int data) {
	return i2c_smbus_write_word_data(fd, reg, data);
}

static int hummingboardI2CSetup(int devId) {
	int fd = 0;
	const char *device = NULL;

	device = "/dev/i2c-2";

	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->I2CSetup: Unable to open %s: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, I2C_SLAVE, devId) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->I2CSetup: Unable to set %s to slave mode: %s", device, strerror(errno));
		return -1;
	}

	return fd;
}

int hummingboardSPIGetFd(int channel) {
	return spiFds[channel & 1];
}

int hummingboardSPIDataRW(int channel, unsigned char *data, int len) {
	struct spi_ioc_transfer spi;
	memset(&spi, 0, sizeof(spi));
	channel &= 1;

	spi.tx_buf = (unsigned long)data;
	spi.rx_buf = (unsigned long)data;
	spi.len = len;
	spi.delay_usecs = spiDelay;
	spi.speed_hz = spiSpeeds[channel];
	spi.bits_per_word = spiBPW;

	if(ioctl(spiFds[channel], SPI_IOC_MESSAGE(1), &spi) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPIDataRW: Unable to read/write from channel %d: %s", channel, strerror(errno));
		return -1;
	}
	return 0;
}

int hummingboardSPISetup(int channel, int speed) {
	int fd;
	const char *device = NULL;

	channel &= 1;

	if(channel == 0) {
		device = "/dev/spidev1.0";
	} else {
		device = "/dev/spidev1.1";
	}

	if((fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPISetup: Unable to open %s: %s", device, strerror(errno));
		return -1;
	}

	spiSpeeds[channel] = speed;
	spiFds[channel] = fd;

	if(ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPISetup: Unable to set %s to write mode: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MODE, &spiMode) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPISetup: Unable to set %s to read mode: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPISetup: Unable to set %s write bits_per_word: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &spiBPW) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPISetup: Unable to set %s read bits_per_word: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPISetup: Unable to set %s write max_speed: %s", device, strerror(errno));
		return -1;
	}

	if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
		wiringXLog(LOG_ERR, "hummingboard->SPISetup: Unable to set %s read max_speed: %s", device, strerror(errno));
		return -1;
	}

	return fd;
}

#endif

void hummingboardInit(void) {

	memset(pinModes, -1, NUM_PINS);

	platform_register(&hummingboard, "hummingboard");
	hummingboard->setup=&setup;
	hummingboard->pinMode=&hummingboardPinMode;
	hummingboard->digitalWrite=&hummingboardDigitalWrite;
	hummingboard->digitalRead=&hummingboardDigitalRead;
	hummingboard->identify=&identify;
	hummingboard->isr=&hummingboardISR;
	hummingboard->waitForInterrupt=&hummingboardWaitForInterrupt;
#ifndef __FreeBSD__
	hummingboard->I2CRead=&hummingboardI2CRead;
	hummingboard->I2CReadReg8=&hummingboardI2CReadReg8;
	hummingboard->I2CReadReg16=&hummingboardI2CReadReg16;
	hummingboard->I2CWrite=&hummingboardI2CWrite;
	hummingboard->I2CWriteReg8=&hummingboardI2CWriteReg8;
	hummingboard->I2CWriteReg16=&hummingboardI2CWriteReg16;
	hummingboard->I2CSetup=&hummingboardI2CSetup;
	hummingboard->SPIGetFd=&hummingboardSPIGetFd;
	hummingboard->SPIDataRW=&hummingboardSPIDataRW;
	hummingboard->SPISetup=&hummingboardSPISetup;
#endif
	hummingboard->gc=&hummingboardGC;
	hummingboard->validGPIO=&hummingboardValidGPIO;
}
