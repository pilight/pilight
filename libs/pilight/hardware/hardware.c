/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <regex.h>
	#include <sys/ioctl.h>
	#include <dlfcn.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pthread.h>
	#include <libgen.h>
	#include <dirent.h>
#endif
#include <sys/stat.h>
#include <time.h>

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/dso.h"
#include "../config/settings.h"
#include "hardware.h"

struct hardware_t *hardware = NULL;

static int init = 0;

void hardware_register(struct hardware_t **hw) {
}

void hardware_set_id(hardware_t *hw, const char *id) {
}

int hardware_gc(void) {
	init = 0;

	logprintf(LOG_DEBUG, "garbage collected config hardware library");
	return EXIT_SUCCESS;
}

void hardware_init(void) {
	if(init == 1) {
		return;
	}
	init = 1;
	plua_init();

	char path[PATH_MAX];
	char *f = STRDUP(__FILE__);
	struct dirent *file = NULL;
	DIR *d = NULL;
	struct stat s;
	char *hardware_root = HARDWARE_ROOT;

	if(f == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	int ret = config_setting_get_string("hardware-root", 0, &hardware_root);

	if((d = opendir(hardware_root))) {
		while((file = readdir(d)) != NULL) {
			memset(path, '\0', PATH_MAX);
			sprintf(path, "%s%s", hardware_root, file->d_name);
			if(stat(path, &s) == 0) {
				/* Check if file */
				if(S_ISREG(s.st_mode)) {
					if(strstr(file->d_name, ".lua") != NULL) {
						plua_module_load(path, HARDWARE);
					}
				}
			}
		}
	}
	if(ret == 0 || hardware_root != (void *)HARDWARE_ROOT) {
		FREE(hardware_root);
	}
	closedir(d);
	FREE(f);
}