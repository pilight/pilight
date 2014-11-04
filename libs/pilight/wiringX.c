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

#include "log.h"
#include "wiringX.h"
#include "hummingboard.h"
#include "raspberrypi.h"
#include "bananapi.h"
#include "radxa.h"

static struct devices_t *device = NULL;
static int setup = -2;

void device_register(struct devices_t **dev, const char *name) {
	*dev = malloc(sizeof(struct devices_t));
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

	if(!((*dev)->name = malloc(strlen(name)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(0);
	}
	strcpy((*dev)->name, name);
	(*dev)->next = devices;
	devices = (*dev);
}

int wiringXGC(void) {
	int i = 0;
	if(device) {
		i = device->gc();
	}
	device = NULL;
	struct devices_t *tmp = devices;
	while(devices) {
		tmp = devices;
		free(devices->name);
		devices = devices->next;
		free(tmp);
	}
	free(devices);
	logprintf(LOG_DEBUG, "garbage collected wiringX library");
	return i;
}

void pinMode(int pin, int mode) {
	if(device) {
		if(device->pinMode) {
			if(device->pinMode(pin, mode) == -1) {
				logprintf(LOG_ERR, "%s: error while calling pinMode", device->name);
				wiringXGC();
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support pinMode", device->name);
			wiringXGC();
		}
	}
}

void digitalWrite(int pin, int value) {
	if(device) {
		if(device->digitalWrite) {
			if(device->digitalWrite(pin, value) == -1) {
				logprintf(LOG_ERR, "%s: error while calling digitalWrite", device->name);
				wiringXGC();
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support digitalWrite", device->name);
			wiringXGC();
		}
	}
}

int digitalRead(int pin) {
	if(device) {
		if(device->digitalRead) {
			int x = device->digitalRead(pin);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling digitalRead", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support digitalRead", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int waitForInterrupt(int pin, int ms) {
	if(device) {
		if(device->waitForInterrupt) {
			int x = device->waitForInterrupt(pin, ms);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling waitForInterrupt", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support waitForInterrupt", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXISR(int pin, int mode) {
	if(device) {
		if(device->isr) {
			int x = device->isr(pin, mode);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling isr", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support isr", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CRead(int fd) {
	if(device) {
		if(device->I2CRead) {
			int x = device->I2CRead(fd);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling I2CRead", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support I2CRead", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CReadReg8(int fd, int reg) {
	if(device) {
		if(device->I2CReadReg8) {
			int x = device->I2CReadReg8(fd, reg);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling I2CReadReg8", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support I2CReadReg8", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CReadReg16(int fd, int reg) {
	if(device) {
		if(device->I2CReadReg16) {
			int x = device->I2CReadReg16(fd, reg);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling I2CReadReg16", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support I2CReadReg16", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CWrite(int fd, int data) {
	if(device) {
		if(device->I2CWrite) {
			int x = device->I2CWrite(fd, data);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling I2CWrite", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support I2CWrite", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CWriteReg8(int fd, int reg, int data) {
	if(device) {
		if(device->I2CWriteReg8) {
			int x = device->I2CWriteReg8(fd, reg, data);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling I2CWriteReg8", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support I2CWriteReg8", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CWriteReg16(int fd, int reg, int data) {
	if(device) {
		if(device->I2CWriteReg16) {
			int x = device->I2CWriteReg16(fd, reg, data);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling I2CWriteReg16", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support I2CWriteReg16", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXI2CSetup(int devId) {
	if(device) {
		if(device->I2CSetup) {
			int x = device->I2CSetup(devId);
			if(x == -1) {
				logprintf(LOG_ERR, "%s: error while calling I2CSetup", device->name);
				wiringXGC();
			} else {
				return x;
			}
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support I2CSetup", device->name);
			wiringXGC();
		}
	}
	return -1;
}

char *wiringXPlatform(void) {
	return device->name;
}

int wiringXValidGPIO(int gpio) {
	if(device) {
		if(device->validGPIO) {
			return device->validGPIO(gpio);
		} else {
			logprintf(LOG_ERR, "%s: device doesn't support gpio number validation", device->name);
			wiringXGC();
		}
	}
	return -1;
}

int wiringXSetup(void) {
	if(setup == -2) {
		hummingboardInit();
		raspberrypiInit();
		bananapiInit();
		radxaInit();

		int match = 0;
		struct devices_t *tmp = devices;
		while(tmp) {
			if(tmp->identify() >= 0) {
				device = tmp;
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(match == 0) {
			logprintf(LOG_ERR, "hardware not supported");
			wiringXGC();
			return -1;
		} else {
			logprintf(LOG_DEBUG, "running on a %s", device->name);
		}
		setup = device->setup();
		return setup;
	} else {
		return setup;
	}
}
