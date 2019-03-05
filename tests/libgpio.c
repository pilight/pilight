/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#define __USE_GNU
#include <dlfcn.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/mem.h"
#include "../libs/libuv/uv.h"

static int state[255] = { 0 };
static int init = 0;
static uv_os_fd_t fd[255] = { -1 };
static int (* ioctl_callback)(int fd, unsigned long req, void *cmd) = NULL;

#ifdef _WIN32
__declspec(dllexport)
#endif
int wiringXSetup(char *name, void (*func)(int, char *, int, const char *, ...)) {
	if(name != NULL && strcmp(name, "test") == 0) {
		return -999;
	}
	int i = 0;
	for(i=0;i<255;i++) {
		fd[i] = -1;
		state[i] = 0;
	}
	if(name == NULL || strcmp(name, "gpio-stub") == 0) {
		init = 1;
		return 0;
	}
	return -1;
}

int wiringXValidGPIO(int gpio) {
	if(gpio == 0 || gpio == 1 || gpio == 2 || gpio == 14 || gpio == 10) {
		return 0;
	}
	return -1;
}

int digitalWrite(int gpio, int mode) {
	int ret = -1;
	if(fd[gpio] > 0) {
		ret = ((send(fd[gpio], "a", 1, 0) == 1) ? 0 : -1);
	}
	return ret;
}

char *wiringXPlatform(void) {
	return "gpio-stub";
}

int digitalRead(int gpio) {
	state[gpio] ^= 1;
	return state[gpio];
}

int wiringXSelectableFd(int gpio) {
	return fd[gpio];
}

int pinMode(int gpio, int mode) {
	struct sockaddr_un address;

	if(fd[gpio] == -1) {
		if((fd[gpio] = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
			logprintf(LOG_ERR, "socket");
			return -1;
		}

		/* start with a clean address structure */
		memset(&address, 0, sizeof(struct sockaddr_un));

		address.sun_family = AF_UNIX;
		snprintf(address.sun_path, 128, "/dev/gpio%d", gpio);

		if(connect(fd[gpio], (struct sockaddr *)&address, sizeof(struct sockaddr_un)) != 0) {
			logprintf(LOG_ERR, "connect");
			return -1;
		}
	}

	return 0;
}

void wiringXIOCTLCallback(int (*callback)(int fd, unsigned long req, void *cmd)) {
	ioctl_callback = callback;
}

int ioctl(int fd, unsigned long req, void *cmd) {
	if(ioctl_callback != NULL) {
		return ioctl_callback(fd, req, cmd);
	} else {
		int (* func)(int, unsigned long, void *);

		func = dlsym(RTLD_NEXT, "ioctl");

		return (func(fd, req, cmd));
	}
}

int wiringXGC(void) {
	init = 0;
	int *(*real)(void) = dlsym(RTLD_NEXT, "wiringXGC");
	if(NULL == real) {
		fprintf(stderr, "dlsym");
	}
	real();
	int i = 0;
	for(i=0;i<255;i++) {
		if(fd[i] > 0) {
			close(fd[i]);
			fd[i] = -1;
		}
	}
  return 0;
}

int wiringXISR(int gpio, int mode) {
	if(gpio == 2) {
		return -1;
	}
	if(fd[gpio] == -1) {
		return pinMode(gpio, mode);
	} else {
		return 0;
	}
}