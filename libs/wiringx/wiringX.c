/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#ifndef __FreeBSD__
	#include <linux/spi/spidev.h>
	#include "i2c-dev.h"
#endif

#include "wiringX.h"

#include "soc/allwinner/a10.h"
#include "soc/allwinner/a31s.h"
#include "soc/nxp/imx6dqrm.h"
#include "soc/nxp/imx6sdlrm.h"
#include "soc/broadcom/2835.h"
#include "soc/broadcom/2836.h"
#include "soc/amlogic/s805.h"
#include "soc/amlogic/s905.h"
#include "soc/samsung/exynos5422.h"

#include "platform/linksprite/pcduino1.h"
#include "platform/lemaker/bananapi.h"
#include "platform/lemaker/bananapim2.h"
#include "platform/solidrun/hummingboard_gate_edge_sdl.h"
#include "platform/solidrun/hummingboard_gate_edge_dq.h"
#include "platform/solidrun/hummingboard_base_pro_sdl.h"
#include "platform/solidrun/hummingboard_base_pro_dq.h"
#include "platform/raspberrypi/raspberrypi1b1.h"
#include "platform/raspberrypi/raspberrypi1b2.h"
#include "platform/raspberrypi/raspberrypi1b+.h"
#include "platform/raspberrypi/raspberrypi2.h"
#include "platform/raspberrypi/raspberrypi3.h"
#include "platform/hardkernel/odroidc1.h"
#include "platform/hardkernel/odroidc2.h"
#include "platform/hardkernel/odroidxu4.h"

static struct platform_t *platform = NULL;
static int namenr = 0;
void (*wiringXLog)(int, const char *, ...) = NULL;

static int issetup = 0;

#ifndef __FreeBSD__
/* SPI Bus Parameters */

struct spi_t {
	uint8_t mode;
	uint8_t bits_per_word;
	uint16_t delay;
	uint32_t speed;
	int fd;
} spi_t;

static struct spi_t spi[2] = {
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 }
};
#endif

#ifdef _WIN32
#define timeradd(a, b, result) \
	do { \
		(result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
		(result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
		if ((result)->tv_usec >= 1000000L) { \
			++(result)->tv_sec; \
			(result)->tv_usec -= 1000000L; \
		} \
	} while (0)

#define timersub(a, b, result) \
	do { \
		(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
		(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
		if ((result)->tv_usec < 0) { \
			--(result)->tv_sec; \
			(result)->tv_usec += 1000000L; \
		} \
	} while (0)
#endif

/* Both the delayMicroseconds and the delayMicrosecondsHard
   are taken from wiringPi */
static void delayMicrosecondsHard(unsigned int howLong) {
	struct timeval tNow, tLong, tEnd ;

	gettimeofday(&tNow, NULL);
#ifdef _WIN32
	tLong.tv_sec  = howLong / 1000000;
	tLong.tv_usec = howLong % 1000000;
#else
	tLong.tv_sec  = (__time_t)howLong / 1000000;
	tLong.tv_usec = (__suseconds_t)howLong % 1000000;
#endif
	timeradd(&tNow, &tLong, &tEnd);

	while(timercmp(&tNow, &tEnd, <)) {
		gettimeofday(&tNow, NULL);
	}
}

void delayMicroseconds(unsigned int howLong) {
	struct timespec sleeper;
#ifdef _WIN32
	long int uSecs = howLong % 1000000;
	unsigned int wSecs = howLong / 1000000;
#else
	long int uSecs = (__time_t)howLong % 1000000;
	unsigned int wSecs = howLong / 1000000;
#endif

	if(howLong == 0) {
		return;
	} else if(howLong  < 100) {
		delayMicrosecondsHard(howLong);
	} else {
#ifdef _WIN32
		sleeper.tv_sec = wSecs;
#else
		sleeper.tv_sec = (__time_t)wSecs;	
#endif
		sleeper.tv_nsec = (long)(uSecs * 1000L);
		nanosleep(&sleeper, NULL);
	}
}

void wiringXDefaultLog(int prio, const char *format_str, ...) {
	va_list ap, apcpy;
	char buf[64], *line = malloc(128);
	int save_errno = -1, pos = 0, bytes = 0;

	if(line == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(-1);
	}

	save_errno = errno;

	memset(line, '\0', 128);
	memset(buf, '\0',  64);

	switch(prio) {
		case LOG_WARNING:
			pos += sprintf(line, "WARNING: ");
		break;
		case LOG_ERR:
			pos += sprintf(line, "ERROR: ");
		break;
		case LOG_INFO:
			pos += sprintf(line, "INFO: ");
		break;
		case LOG_NOTICE:
			pos += sprintf(line, "NOTICE: ");
		break;
		case LOG_DEBUG:
			pos += sprintf(line, "DEBUG: ");
		break;
		default:
		break;
	}

	va_copy(apcpy, ap);
	va_start(apcpy, format_str);
#ifdef _WIN32
	bytes = _vscprintf(format_str, apcpy);
#else
	bytes = vsnprintf(NULL, 0, format_str, apcpy);
#endif
	if(bytes == -1) {
		fprintf(stderr, "ERROR: unproperly formatted wiringX log message %s\n", format_str);
	} else {
		va_end(apcpy);
		if((line = realloc(line, (size_t)bytes+(size_t)pos+3)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(-1);
		}
		va_start(ap, format_str);
		pos += vsprintf(&line[pos], format_str, ap);
		va_end(ap);
	}
	line[pos++]='\n';
	line[pos++]='\0';

	fprintf(stderr, "%s", line);

	free(line);
	errno = save_errno;
}

int wiringXSetup(char *name, void (*func)(int, const char *, ...)) {
	if(issetup == 0) {
		issetup = 1;
	} else {
		return 0;
	}

	if(func != NULL) {
		wiringXLog = func;
	} else {
		wiringXLog = wiringXDefaultLog;
	}

	/* Init all SoC's */
	allwinnerA10Init();
	allwinnerA31sInit();
	nxpIMX6DQRMInit();
	nxpIMX6SDLRMInit();
	broadcom2835Init();
	broadcom2836Init();
	amlogicS805Init();
	amlogicS905Init();
	exynos5422Init();

	/* Init all platforms */
	pcduino1Init();
	bananapiInit();
	bananapiM2Init();
	hummingboardBaseProSDLInit();
	hummingboardBaseProDQInit();
	hummingboardGateEdgeSDLInit();
	hummingboardGateEdgeDQInit();
	raspberrypi1b1Init();
	raspberrypi1b2Init();
	raspberrypi1bpInit();
	raspberrypi2Init();
	raspberrypi3Init();
	odroidc1Init();
	odroidc2Init();
	odroidxu4Init();

	if((platform = platform_get_by_name(name, &namenr)) == NULL) {
		char *tmp = NULL;
		char message[1024];
		int l = 0;
		l = snprintf(message, 1023-l, "The %s is an unsupported or unknown platform\n", name);
		l += snprintf(&message[l], 1023-l, "\tsupported wiringX platforms are:\n");
		int i = 0;
		while((tmp = platform_iterate_name(i++)) != NULL) {
			l += snprintf(&message[l], 1023-l, "\t- %s\n", tmp);
		}
		wiringXLog(LOG_ERR, message);
		return -1;
	}
	platform->setup();
	
	return 0;
}

char *wiringXPlatform(void) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	}
	return platform->name[namenr];
}

int pinMode(int pin, enum pinmode_t mode) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	} else if(platform->pinMode == NULL) {
		wiringXLog(LOG_ERR, "The %s does not support the pinMode functionality", platform->name);
		return -1;
	}
	return platform->pinMode(pin, mode);
}

int digitalWrite(int pin, enum digital_value_t value) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	}	else if(platform->digitalWrite == NULL) {
		wiringXLog(LOG_ERR, "The %s does not support the digitalWrite functionality", platform->name);
		return -1;
	}
	return platform->digitalWrite(pin, value);
}

int digitalRead(int pin) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	}	else if(platform->digitalRead == NULL) {
		wiringXLog(LOG_ERR, "The %s does not support the digitalRead functionality", platform->name);
		return -1;
	}
	return platform->digitalRead(pin);
}

int wiringXISR(int pin, enum isr_mode_t mode) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	}	else if(platform->isr == NULL) {
		wiringXLog(LOG_ERR, "The %s does not support the wiringXISR functionality", platform->name);
		return -1;
	}
	return platform->isr(pin, mode);
}

int waitForInterrupt(int pin, int ms) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	}	else if(platform->waitForInterrupt == NULL) {
		wiringXLog(LOG_ERR, "The %s does not support the waitForInterrupt functionality", platform->name);
		return -1;
	}
	return platform->waitForInterrupt(pin, ms);
}

int wiringXValidGPIO(int pin) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	}	else if(platform->validGPIO == NULL) {
		wiringXLog(LOG_ERR, "The %s does not support the wiringXValidGPIO functionality", platform->name);
		return -1;
	}
	return platform->validGPIO(pin);
}

#ifndef __FreeBSD__
int wiringXI2CRead(int fd) {
	return i2c_smbus_read_byte(fd);
}

int wiringXI2CReadReg8(int fd, int reg) {
	return i2c_smbus_read_byte_data(fd, reg);
}

int wiringXI2CReadReg16(int fd, int reg) {
	return i2c_smbus_read_word_data(fd, reg);
}

int wiringXI2CWrite(int fd, int data) {
	return i2c_smbus_write_byte(fd, data);
}

int wiringXI2CWriteReg8(int fd, int reg, int data) {
	return i2c_smbus_write_byte_data(fd, reg, data);
}

int wiringXI2CWriteReg16(int fd, int reg, int data) {
	return i2c_smbus_write_word_data(fd, reg, data);
}

int wiringXI2CSetup(char *path, int devId) {
	int fd = 0;

	if((fd = open(path, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open %s for reading and writing", path);
		return -1;
	}

	if(ioctl(fd, I2C_SLAVE, devId) < 0) {
		wiringXLog(LOG_ERR, "wiringX failed to set %s to slave mode", path);
		return -1;
	}

	return fd;
}

int wiringXSPIGetFd(int channel) {
	return spi[channel & 1].fd;
}

int wiringXSPIDataRW(int channel, unsigned char *data, int len) {
	struct spi_ioc_transfer tmp;
	memset(&tmp, 0, sizeof(tmp));
	channel &= 1;

	tmp.tx_buf = (unsigned long)data;
	tmp.rx_buf = (unsigned long)data;
	tmp.len = len;
	tmp.delay_usecs = spi[channel].delay;
	tmp.speed_hz = spi[channel].speed;
	tmp.bits_per_word = spi[channel].bits_per_word;
#ifdef SPI_IOC_WR_MODE32
	tmp.tx_nbits = 0;
#endif
#ifdef SPI_IOC_RD_MODE32
	tmp.rx_nbits = 0;
#endif

	if(ioctl(spi[channel].fd, SPI_IOC_MESSAGE(1), &tmp) < 0) {
		wiringXLog(LOG_ERR, "wiringX is unable to read/write from channel %d (%s)", channel, strerror(errno));
		return -1;
	}
	return 0;
}

int wiringXSPISetup(int channel, int speed) {
	const char *device = NULL;

	channel &= 1;

	if(channel == 0) {
		device = "/dev/spidev0.0";
	} else {
		device = "/dev/spidev0.1";
	}

	if((spi[channel].fd = open(device, O_RDWR)) < 0) {
		wiringXLog(LOG_ERR, "wiringX is unable to open SPI device %s (%s)", device, strerror(errno));
		return -1;
	}

	spi[channel].speed = speed;

	if(ioctl(spi[channel].fd, SPI_IOC_WR_MODE, &spi[channel].mode) < 0) {
		wiringXLog(LOG_ERR, "wiringX is unable to set write mode for device %s (%s)", device, strerror(errno));
		close(spi[channel].fd);
		return -1;
	}

	if(ioctl(spi[channel].fd, SPI_IOC_RD_MODE, &spi[channel].mode) < 0) {
		wiringXLog(LOG_ERR, "wiringX is unable to set read mode for device %s (%s)", device, strerror(errno));
		close(spi[channel].fd);
		return -1;
	}

	if(ioctl(spi[channel].fd, SPI_IOC_WR_BITS_PER_WORD, &spi[channel].bits_per_word) < 0) {
		wiringXLog(LOG_ERR, "wiringX is unable to set write bits_per_word for device %s (%s)", device, strerror(errno));
		close(spi[channel].fd);
		return -1;
	}

	if(ioctl(spi[channel].fd, SPI_IOC_RD_BITS_PER_WORD, &spi[channel].bits_per_word) < 0) {
		wiringXLog(LOG_ERR, "wiringX is unable to set read bits_per_word for device %s (%s)", device, strerror(errno));
		close(spi[channel].fd);
		return -1;
	}

	if(ioctl(spi[channel].fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi[channel].speed) < 0) {
		wiringXLog(LOG_ERR, "wiringX is unable to set write max_speed for device %s (%s)", device, strerror(errno));
		close(spi[channel].fd);
		return -1;
	}

	if(ioctl(spi[channel].fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi[channel].speed) < 0) {
		wiringXLog(LOG_ERR, "wirignX is unable to set read max_speed for device %s (%s)", device, strerror(errno));
		close(spi[channel].fd);
		return -1;
	}

	return spi[channel].fd;
}
#endif

int wiringXSerialOpen(char *device, struct wiringXSerial_t wiringXSerial) {
	struct termios options;
	speed_t myBaud;
	int status = 0, fd = 0;

	switch(wiringXSerial.baud) {
		case 50: myBaud = B50; break;
		case 75: myBaud = B75; break;
		case 110:	myBaud = B110; break;
		case 134:	myBaud = B134; break;
		case 150:	myBaud = B150; break;
		case 200:	myBaud = B200; break;
		case 300:	myBaud = B300; break;
		case 600:	myBaud = B600; break;
		case 1200: myBaud = B1200; break;
		case 1800: myBaud = B1800; break;
		case 2400: myBaud = B2400; break;
		case 4800: myBaud = B4800; break;
		case 9600: myBaud = B9600; break;
		case 19200: myBaud = B19200; break;
		case 38400: myBaud = B38400; break;
		case 57600: myBaud = B57600; break;
		case 115200: myBaud = B115200; break;
		case 230400: myBaud = B230400; break;
		default:
		return -1;
	}

	if((fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK)) == -1) {
		return -1;
	}

	fcntl(fd, F_SETFL, O_RDWR);

	tcgetattr(fd, &options);

	cfmakeraw(&options);
	cfsetispeed(&options, myBaud);
	cfsetospeed(&options, myBaud);

	options.c_cflag |= (CLOCAL | CREAD);

	options.c_cflag &= ~CSIZE;

	switch(wiringXSerial.databits) {
		case 7:
			options.c_cflag |= CS7;
		break;
		case 8:
			options.c_cflag |= CS8;
		break;
		default:
			wiringXLog(LOG_ERR, "wiringX serial interface can not handle the %d data size", wiringXSerial.databits);
		return -1;
	}
	switch(wiringXSerial.parity) {
		case 'n':
		case 'N':/*no parity*/
			options.c_cflag &= ~PARENB;
			options.c_iflag &= ~INPCK;
		break;
		case 'o':
		case 'O':
			options.c_cflag |= (PARODD | PARENB);
			options.c_iflag |= INPCK; /* Disable parity checking */
		break;
		case 'e':
		case 'E':
			options.c_cflag |= PARENB; /* Enable parity */
			options.c_cflag &= ~PARODD;
			options.c_iflag |= INPCK;
		break;
		case 'S':
		case 's': /*as no parity*/
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;
		break;
		default:
			wiringXLog(LOG_ERR, "wiringX serial interface can not handle the %d parity", wiringXSerial.parity);
		return -1;
	}
	switch(wiringXSerial.stopbits) {
		case 1:
			options.c_cflag &= ~CSTOPB;
		break;
		case 2:
			options.c_cflag |= CSTOPB;
		break;
		default:
			wiringXLog(LOG_ERR, "wiringX serial interface can not handle the %d stop bit", wiringXSerial.stopbits);
		return -1;
	}
	switch(wiringXSerial.flowcontrol) {
		case 'x':
		case 'X':
			options.c_iflag |= (IXON | IXOFF | IXANY);
		break;
		case 'n':
		case 'N':
			options.c_iflag &= ~(IXON | IXOFF | IXANY);
		break;
		default:
			wiringXLog(LOG_ERR, "wiringX serial interface can not handle the %d flowcontol", wiringXSerial.flowcontrol);
		return -1;
	}

	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;

	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 150;

	tcflush(fd,TCIFLUSH);

	tcsetattr(fd, TCSANOW | TCSAFLUSH, &options);

	ioctl(fd, TIOCMGET, &status);

	status |= TIOCM_DTR;
	status |= TIOCM_RTS;

	ioctl(fd, TIOCMSET, &status);

	return fd;
}

void wiringXSerialFlush(int fd) {
	if(fd > 0) {
		tcflush(fd, TCIOFLUSH);
	} else {
		wiringXLog(LOG_ERR, "wiringX serial interface has not been opened");
	}
}

void wiringXSerialClose(int fd) {
	if(fd > 0) {
		close(fd);
	}
}

void wiringXSerialPutChar(int fd, unsigned char c) {
	if(fd > 0) {
		int x = write(fd, &c, 1);
		if(x != 1) {
			wiringXLog(LOG_ERR, "wiringX failed to write to serial device");
		}
	} else {
		wiringXLog(LOG_ERR, "wiringX serial interface has not been opened");
	}
}

void wiringXSerialPuts(int fd, char *s) {
	if(fd > 0) {
		int x = write(fd, s, strlen(s));
		if(x != strlen(s)) {
			wiringXLog(LOG_ERR, "wiringX failed to write to serial device");
		}
	} else {
		wiringXLog(LOG_ERR, "wiringX serial interface has not been opened");
	}
}

void wiringXSerialPrintf(int fd, char *message, ...) {
	va_list argp;
	char buffer[1024];

	memset(&buffer, '\0', 1024);

	if(fd > 0) {
		va_start(argp, message);
		vsnprintf(buffer, 1023, message, argp);
		va_end(argp);

		wiringXSerialPuts(fd, buffer);
	} else {
		wiringXLog(LOG_ERR, "wiringX serial interface has not been opened");
	}
}

int wiringXSerialDataAvail(int fd) {
	int result = 0;

	if(fd > 0) {
		if(ioctl(fd, FIONREAD, &result) == -1) {
			return -1;
		}
		return result;
	} else {
		wiringXLog(LOG_ERR, "wiringX serial interface has not been opened");
		return -1;
	}
}

int wiringXSerialGetChar(int fd) {
	uint8_t x = 0;

	if(fd > 0) {
		if(read(fd, &x, 1) != 1) {
			return -1;
		}
		return ((int)x) & 0xFF;
	} else {
		wiringXLog(LOG_ERR, "wiringX serial interface has not been opened");
		return -1;
	}
}

int wiringXSelectableFd(int gpio) {
	if(platform == NULL) {
		wiringXLog(LOG_ERR, "wiringX has not been properly setup (no platform has been selected)");
	}	else if(platform->selectableFd == NULL) {
		wiringXLog(LOG_ERR, "The %s does not support the wiringXSelectableFd functionality", platform->name);
		return -1;
	}
	return platform->selectableFd(gpio);
}

int wiringXGC(void) {
	if(platform != NULL) {
		platform->gc();
	}
	platform_gc();
	soc_gc();
	return 0;
}
