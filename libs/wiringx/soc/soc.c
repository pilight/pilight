/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include "../wiringX.h"
#include "soc.h"

static struct soc_t *socs = NULL;

void soc_register(struct soc_t **soc, char *brand, char *type) {
	int i = 0;

	if((*soc = malloc(sizeof(struct soc_t))) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	strcpy((*soc)->brand, brand);
	strcpy((*soc)->chip, type);

	(*soc)->map = NULL;
	(*soc)->map_size = 0;
	(*soc)->irq = NULL;
	(*soc)->irq_size = 0;
	(*soc)->layout = NULL;
	(*soc)->support.isr_modes = 0;

	(*soc)->fd = 0;

	(*soc)->page_size = 0;

	(*soc)->digitalWrite = NULL;
	(*soc)->digitalRead = NULL;
	(*soc)->pinMode = NULL;
	(*soc)->isr = NULL;
	(*soc)->waitForInterrupt = NULL;

	(*soc)->setup = NULL;
	(*soc)->setMap = NULL;
	(*soc)->setIRQ = NULL;
	(*soc)->getPinName = NULL;

	(*soc)->validGPIO = NULL;
	(*soc)->selectableFd = NULL;
	(*soc)->gc = NULL;

	for (i = 0; i < MAX_REG_AREA; ++i) {
		(*soc)->gpio[i] = NULL;
		(*soc)->base_addr[i] = 0;
		(*soc)->base_offs[i] = 0;
	}

	(*soc)->next = socs;
	socs = *soc;
}

void soc_writel(uintptr_t addr, uint32_t val) {
	*((volatile uint32_t *)(addr)) = val;
}

uint32_t soc_readl(uintptr_t addr) {
	return *((volatile uint32_t *)(addr));
}

int soc_sysfs_check_gpio(struct soc_t *soc, char *path) {
	struct stat s;
	int err = stat(path, &s);
	if(err == -1) {
    if(ENOENT == errno) {
			return -1;
    } else {
			wiringXLog(LOG_ERR, "wiringX encountered an unexpected error while changing onwership of %s (%s)", path, strerror(errno));
			return -1;
    }
	} else {
    if(S_ISDIR(s.st_mode) || S_ISLNK(s.st_mode)) {
			return 0;
    } else {
			wiringXLog(LOG_ERR, "The %s %s path %s exists but is not a folder or link (%s)", soc->brand, soc->chip, path, strerror(errno));
			return -1;
    }
	}
}

int soc_sysfs_gpio_export(struct soc_t *soc, char *path, int pin) {
	char out[4];
	int fd = 0;
	if((fd = open(path, O_WRONLY)) <= 0) {
		wiringXLog(LOG_ERR, "The %s %s cannot open %s for gpio exporting (%s)", soc->brand, soc->chip, path, strerror(errno));
		return -1;
	}
	int l = snprintf(out, 4, "%d", pin);
	if(write(fd, out, l) != l) {
		wiringXLog(LOG_ERR, "The %s %s failed to write to %s for gpio exporting (%s)", soc->brand, soc->chip, path, strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int soc_sysfs_gpio_unexport(struct soc_t *soc, char *path, int pin) {
	char out[4];
	int fd = 0;
	if((fd = open(path, O_WRONLY)) <= 0) {
		wiringXLog(LOG_ERR, "The %s %s cannot open %s for gpio unexporting (%s)", soc->brand, soc->chip, path, strerror(errno));
		return -1;
	}
	int l = snprintf(out, 4, "%d", pin);
	if(write(fd, out, l) != l) {
		wiringXLog(LOG_ERR, "The %s %s failed to write to %s for gpio unexporting (%s)", soc->brand, soc->chip, path, strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int soc_sysfs_set_gpio_interrupt_mode(struct soc_t *soc, char *path, enum isr_mode_t mode) {
	const char *sMode = NULL;

	switch(mode) {
		case ISR_MODE_FALLING:
			sMode = "falling";
		break;
		case ISR_MODE_RISING:
			sMode = "rising";
		break;
		case ISR_MODE_BOTH:
			sMode = "both";
		break;
		case ISR_MODE_NONE:
			sMode = "none";
		break;
		default:
			wiringXLog(LOG_ERR, "The %s %s does not support this interrupt mode", soc->brand, soc->chip);
			return -1;
		break;
	}

	if((soc->support.isr_modes & mode) == 0) {
		wiringXLog(LOG_ERR, "The %s %s does not support interrupt %s mode", soc->brand, soc->chip, sMode);
		return -1;
	}

	int fd = 0;
	if((fd = open(path, O_WRONLY)) <= 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open %s for gpio edge (%s)", path, strerror(errno));
		return -1;
	} else {
		int l = strlen(sMode);
		if(write(fd, sMode, l) == l) {
			close(fd);
			return 0;
		}
		wiringXLog(LOG_ERR, "wiringX failed to write to %s for gpio edge (%s)", path, strerror(errno));
		close(fd);
	}
	return -1;
}

int soc_sysfs_set_gpio_direction(struct soc_t *soc, char *path, char *dir) {
	int fd = 0;
	if((fd = open(path, O_WRONLY)) <= 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open %s for gpio direction (%s)", path, strerror(errno));
		return -1;
	} else {
		int l = strlen(dir);
		if(write(fd, dir, l) == l) {
			close(fd);
			return 0;
		}
		wiringXLog(LOG_ERR, "wiringX failed to write %s to %s (%s)", dir, path, strerror(errno));
		close(fd);
	}
	return -1;
}

static int changeOwner(char *file) {
	uid_t uid = getuid();
	uid_t gid = getgid();

	if(chown(file, uid, gid) != 0) {
		if(errno == ENOENT)	{
			wiringXLog(LOG_ERR, "wiringX failed to change the ownership of %s (%s)", file, strerror(errno));
			return -1;
		} else {
			wiringXLog(LOG_ERR, "wiringX failed to change the ownership of %s (%s)", file, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int soc_sysfs_gpio_reset_value(struct soc_t *soc, char *path) {
	unsigned int c = 0;
	int fd = 0, i = 0, count = 0;
	if(changeOwner(path) == -1) {
		return -1;
	}
	if((fd = open(path, O_RDWR)) <= 0) {
		wiringXLog(LOG_ERR, "wiringX failed to open %s for gpio reading (%s)", path, strerror(errno));
		return -1;
	} else {
		ioctl(fd, FIONREAD, &count);
		for(i=0; i<count; ++i) {
			int x = read(fd, &c, 1);
			if(x != 1) {
				continue;
			}
		}
		lseek(fd, 0, SEEK_SET);
	}

	return fd;
}

int soc_wait_for_interrupt(struct soc_t *soc, int fd, int ms) {
	unsigned int c = 0;
	struct pollfd polls;
	int x = 0;

	polls.fd = fd;
	polls.events = POLLPRI;

	x = read(fd, &c, 1);
	if(x != 1) {
		return -1;
	}
	lseek(fd, 0, SEEK_SET);

	x = poll(&polls, 1, ms);

	/* Don't react to signals */
	if(x == -1 && errno == EINTR) {
		x = 0;
	}
	return x;
}

struct soc_t *soc_get(char *brand, char *chip) {
	struct soc_t *tmp = socs;
	while(tmp) {
		if(strcmp(tmp->brand, brand) == 0 &&
			strcmp(tmp->chip, chip) == 0) {
			return tmp;
		}
		tmp = tmp->next;
	}
	return NULL;
}

int soc_gc(void) {
	struct soc_t *tmp = NULL;
	while(socs) {
		tmp = socs;
		socs = socs->next;
		free(tmp);
	}
	return 0;
}
