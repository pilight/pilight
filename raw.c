/*
	Copyright (C) 2013 - 2014 CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "pilight.h"
#include "protocol.h"
#include "common.h"
#include "config.h"
#include "hardware.h"
#include "log.h"
#include "datetime.h"
#include "ssdp.h"
#include "socket.h"
#include "wiringX.h"
#include "threads.h"
#include "irq.h"
#include "dso.h"
#include "gc.h"

static unsigned short main_loop = 1;

int main_gc(void) {
	main_loop = 0;

	log_shell_disable();

	datetime_gc();
	ssdp_gc();
	protocol_gc();
	options_gc();
	socket_gc();
	dso_gc();

	config_gc();
	whitelist_free();
	threads_gc();

	wiringXGC();
	log_gc();
	gc_clear();

	FREE(progname);
	xfree();

	return EXIT_SUCCESS;
}

void *receive_code(void *param) {
	int duration = 0;

	struct hardware_t *hw = (hardware_t *)param;
	while(main_loop && hw->receive) {
		duration = hw->receive();
		if(duration > 0) {
			printf("%s: %d\n", hw->id, duration);
		}
	};
	return NULL;
}

int main(int argc, char **argv) {
	// memtrack();

	struct options_t *options = NULL;
	char *args = NULL;
	char *configtmp = MALLOC(strlen(CONFIG_FILE)+1);
	pid_t pid = 0;

	strcpy(configtmp, CONFIG_FILE);

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	wiringXLog = logprintf;

	if(!(progname = MALLOC(12))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-raw");

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'C', "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

	while (1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &args);
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
				goto close;
			break;
			case 'V':
				printf("%s %s\n", progname, VERSION);
				goto close;
			break;
			case 'C':
				configtmp = REALLOC(configtmp, strlen(args)+1);
				strcpy(configtmp, args);
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				goto close;
			break;
		}
	}
	options_delete(options);

	char *pilight_daemon = MALLOC(strlen("pilight-daemon")+1);
	if(!pilight_daemon) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(pilight_daemon, "pilight-daemon");
	if((pid = findproc(pilight_daemon, NULL, 1)) > 0) {
		logprintf(LOG_ERR, "pilight-daemon instance found (%d)", (int)pid);
		FREE(pilight_daemon);
		goto close;
	}
	FREE(pilight_daemon);

	char *pilight_debug = MALLOC(strlen("pilight-debug")+1);
	if(!pilight_debug) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(pilight_debug, "pilight-debug");
	if((pid = findproc(pilight_debug, NULL, 1)) > 0) {
		logprintf(LOG_ERR, "pilight-debug instance found (%d)", (int)pid);
		FREE(pilight_debug);
		goto close;
	}
	FREE(pilight_debug);

	if(config_set_file(configtmp) == EXIT_FAILURE) {
		FREE(configtmp);
		return EXIT_FAILURE;
	}

	protocol_init();
	config_init();
	if(config_read() != EXIT_SUCCESS) {
		FREE(configtmp);
		goto close;
	}
	FREE(configtmp);

	/* Start threads library that keeps track of all threads used */
	threads_start();

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->init() == EXIT_FAILURE) {
			logprintf(LOG_ERR, "could not initialize %s hardware mode", tmp_confhw->hardware->id);
			goto close;
		}
		threads_register(tmp_confhw->hardware->id, &receive_code, (void *)tmp_confhw->hardware, 0);
		tmp_confhw = tmp_confhw->next;
	}

	while(main_loop) {
		sleep(1);
	}

close:
	if(args != NULL) {
		FREE(args);
	}
	if(main_loop == 1) {
		main_gc();
	}
	return (EXIT_FAILURE);
}
