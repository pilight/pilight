/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/firmware.h"
#include "libs/libuv/uv.h"

#include "libs/pilight/events/events.h"

#ifndef _WIN32
	#include <wiringx.h>
#endif

int main(int argc, char **argv) { 
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	atomicinit();
	log_shell_enable();
	log_file_disable();

	pilight.process = PROCESS_CLIENT;

	struct options_t *options = NULL;
	char *fconfig = NULL;
	char *args = NULL;
	char *fwfile = NULL;
	char comport[255];

	memset(&comport, '\0', 255);

	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(fconfig, CONFIG_FILE);

	if((progname = MALLOC(15)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(progname, "pilight-flash");

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "C", "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "f", "file", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "p", "comport", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

	while (1) {
		int c;
		c = options_parse1(&options, argc, argv, 1, &args, NULL);
		if(c == -1)
			break;
		if(c == -2)
			c = 'H';
		switch (c) {
			case 'H':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");
				printf("\t -C --config\t\tconfig file\n");
				printf("\t -p --comport\t\tserial COM port\n");
				printf("\t -f --file=firmware\tfirmware file\n");
				goto close;
			break;
			case 'V':
				printf("%s v%s\n", progname, PILIGHT_VERSION);
				goto close;
			break;
			case 'C':
				if((fconfig = REALLOC(fconfig, strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(fconfig, args);
			break;
			case 'p':
				strcpy(comport, args);
			break;
			case 'f':
				if(access(args, F_OK) != -1) {
					fwfile = REALLOC(fwfile, strlen(args)+1);
					strcpy(fwfile, args);
				} else {
					fprintf(stderr, "%s: the firmware file %s does not exist\n", progname, args);
					goto close;
				}
			break;
			default:
				printf("Usage: %s -f pilight_firmware_tX5_v3.hex\n", progname);
				goto close;
			break;
		}
	}

	storage_init();
	if(storage_read(fconfig, CONFIG_SETTINGS) != 0) {
		FREE(fconfig);
		goto close;
	}

#ifdef _WIN32
	if(fwfile == NULL || strlen(fwfile) == 0 || strlen(comport) == 0) {
		printf("Usage: %s -f pilight_firmware_XXX_vX.hex -p comX\n", progname);
		goto close;
	}
#else
	if(fwfile == NULL || strlen(fwfile) == 0) {
		printf("Usage: %s -f pilight_firmware_XXX_vX.hex\n", progname);
		goto close;
	}
#endif

	log_level_set(LOG_DEBUG);
	logprintf(LOG_INFO, "**** START UPD. FW ****");
	firmware_getmp(comport);

	if(firmware_update(fwfile, comport) != 0) {
		logprintf(LOG_INFO, "**** FAILED UPD. FW ****");
	} else {
		logprintf(LOG_INFO, "**** DONE UPD. FW ****");
	}

close:
	log_shell_disable();
	options_delete(options);
	if(fwfile != NULL) {
		FREE(fwfile);
	}
	FREE(fconfig);
	log_shell_disable();
	log_level_set(LOG_ERR);
	storage_gc();
	options_gc();
#ifdef EVENTS
	events_gc();
#endif
#ifndef _WIN32
	wiringXGC();
#endif
	log_gc();
	FREE(progname);
	return (EXIT_SUCCESS);
}
