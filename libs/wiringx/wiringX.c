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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

#include "../pilight/core/common.h"
#include "../pilight/core/log.h"
#include "wiringX.h"
#include "hummingboard.h"
#include "raspberrypi.h"
#include "bananapi.h"
#include "ci20.h"

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

static struct platform_t *platform = NULL;
#ifndef _WIN32
	static int setup = -2;
#endif

void _fprintf(int prio, const char *format_str, ...) {
	char line[1024];
	va_list ap;
	va_start(ap, format_str);
	vsprintf(line, format_str, ap);
	strcat(line, "\n");
	fprintf(stderr, line);
	va_end(ap);
}

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

void platform_register(struct platform_t **dev, const char *name) {
	*dev = malloc(sizeof(struct platform_t));
	(*dev)->name = NULL;
	(*dev)->pinMode = NULL;
	(*dev)->digitalWrite = NULL;
	(*dev)->digitalRead = NULL;
	(*dev)->identify = NULL;
	(*dev)->waitForInterrupt = NULL;
	(*dev)->isr = NULL;
	(*dev)->I2CRead = NULL;
	(*dev)->I2CReadReg8 = NULL;
	(*dev)->I2CReadReg16 = NULL;
	(*dev)->I2CWrite = NULL;
	(*dev)->I2CWriteReg8 = NULL;
	(*dev)->I2CWriteReg16 = NULL;
	(*dev)->SPIGetFd = NULL;
	(*dev)->SPIDataRW = NULL;

	if(((*dev)->name = malloc(strlen(name)+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy((*dev)->name, name);
	(*dev)->next = platforms;
	platforms = (*dev);
}

int wiringXGC(void) {
	int i = 0;
	if(platform != NULL) {
		i = platform->gc();
	}
	platform = NULL;
	struct platform_t *tmp = platforms;
	while(platforms) {
		tmp = platforms;
		free(platforms->name);
		platforms = platforms->next;
		free(tmp);
	}
	if(platforms != NULL) {
		free(platforms);
	}

	wiringXLog(LOG_DEBUG, "garbage collected wiringX library");
	return i;
}

void pinMode(int pin, int mode) {
	if(platform != NULL) {
		if(platform->pinMode) {
			if(platform->pinMode(pin, mode) == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling pinMode", platform->name);
				wiringXGC();
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support pinMode", platform->name);
			wiringXGC();
		}
	}
}

void digitalWrite(int pin, int value) {
	if(platform != NULL) {
		if(platform->digitalWrite) {
			if(platform->digitalWrite(pin, value) == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling digitalWrite", platform->name);
				wiringXGC();
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support digitalWrite", platform->name);
			wiringXGC();
		}
	}
}

int digitalRead(int pin) {
	if(platform != NULL) {
		if(platform->digitalRead) {
			int x = platform->digitalRead(pin);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling digitalRead", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support digitalRead", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int waitForInterrupt(int pin, int ms) {
	if(platform != NULL) {
		if(platform->waitForInterrupt) {
			int x = platform->waitForInterrupt(pin, ms);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling waitForInterrupt", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support waitForInterrupt", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXISR(int pin, int mode) {
	if(platform != NULL) {
		if(platform->isr) {
			int x = platform->isr(pin, mode);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling isr", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support isr", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CRead(int fd) {
	if(platform != NULL) {
		if(platform->I2CRead) {
			int x = platform->I2CRead(fd);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling I2CRead", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support I2CRead", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CReadReg8(int fd, int reg) {
	if(platform != NULL) {
		if(platform->I2CReadReg8) {
			int x = platform->I2CReadReg8(fd, reg);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling I2CReadReg8", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support I2CReadReg8", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CReadReg16(int fd, int reg) {
	if(platform != NULL) {
		if(platform->I2CReadReg16) {
			int x = platform->I2CReadReg16(fd, reg);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling I2CReadReg16", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support I2CReadReg16", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CWrite(int fd, int data) {
	if(platform != NULL) {
		if(platform->I2CWrite) {
			int x = platform->I2CWrite(fd, data);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling I2CWrite", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support I2CWrite", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CWriteReg8(int fd, int reg, int data) {
	if(platform != NULL) {
		if(platform->I2CWriteReg8) {
			int x = platform->I2CWriteReg8(fd, reg, data);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling I2CWriteReg8", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support I2CWriteReg8", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CWriteReg16(int fd, int reg, int data) {
	if(platform != NULL) {
		if(platform->I2CWriteReg16) {
			int x = platform->I2CWriteReg16(fd, reg, data);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling I2CWriteReg16", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support I2CWriteReg16", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CSetup(int devId) {
	if(platform != NULL) {
		if(platform->I2CSetup) {
			int x = platform->I2CSetup(devId);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling I2CSetup", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support I2CSetup", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXSPIGetFd(int channel) {
	if(platform != NULL) {
		if(platform->SPIGetFd) {
			int x = platform->SPIGetFd(channel);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling SPIGetFd", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support SPIGetFd", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXSPIDataRW(int channel, unsigned char *data, int len) {
	if(platform != NULL) {
		if(platform->SPIDataRW) {
			int x = platform->SPIDataRW(channel, data, len);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling SPIDataRW", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support SPIDataRW", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXSPISetup(int channel, int speed) {
	if(platform != NULL) {
		if(platform->SPISetup) {
			int x = platform->SPISetup(channel, speed);
			if(x == -1) {
				wiringXLog(LOG_ERR, "%s: error while calling SPISetup", platform->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support SPISetup", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

char *wiringXPlatform(void) {
	return platform->name;
}

int wiringXValidGPIO(int gpio) {
	if(platform != NULL) {
		if(platform->validGPIO) {
			return platform->validGPIO(gpio);
		} else {
			wiringXLog(LOG_ERR, "%s: platform doesn't support gpio number validation", platform->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXSupported(void) {
#if defined(__arm__) || defined(__mips__)
	return 0;
#else
	return -1;
#endif
}

int wiringXSetup(void) {
#ifndef _WIN32	
	if(wiringXLog == NULL) {
		wiringXLog = _fprintf;
	}

	if(wiringXSupported() == 0) {
		if(setup == -2) {
			hummingboardInit();
			raspberrypiInit();
			bananapiInit();
			ci20Init();

			int match = 0;
			struct platform_t *tmp = platforms;
			while(tmp) {
				if(tmp->identify() >= 0) {
					platform = tmp;
					match = 1;
					break;
				}
				tmp = tmp->next;
			}

			if(match == 0) {
				wiringXLog(LOG_ERR, "hardware not supported");
				wiringXGC();
				return -1;
			} else {
				wiringXLog(LOG_DEBUG, "running on a %s", platform->name);
			}
			setup = platform->setup();
			return setup;
		} else {
			return setup;
		}
	}
#endif
	return -1;
}
