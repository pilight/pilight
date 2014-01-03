/*
	Copyright (C) 2013 CurlyMo

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
#include "common.h"
#include "settings.h"
#include "hardware.h"
#include "log.h"
#include "wiringPi.h"
#include "threads.h"
#include "irq.h"
#include "gc.h"

unsigned short main_loop = 1;
pthread_t pth;

int main_gc(void) {
	log_shell_disable();

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {	
		if(tmp_confhw->hardware->deinit) {
			tmp_confhw->hardware->deinit();
		}
		tmp_confhw = tmp_confhw->next;
	}

	threads_gc();	
	options_gc();
	settings_gc();	
	hardware_gc();
	log_gc();

	sfree((void *)&progname);
	sfree((void *)&settingsfile);	

	return EXIT_SUCCESS;
}

void *receive_code(void *param) {
	int duration = 0;
	
	struct hardware_t *hardware = (hardware_t *)param;
	while(main_loop && hardware->receive) {
		duration = hardware->receive();
		if(duration > 0) {
			printf("%s: %d\n", hardware->id, duration);
		}
	};
	return NULL;
}

int main(int argc, char **argv) {

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;
	
	char *args = NULL;
	char *hwfile = NULL;

	settingsfile = malloc(strlen(SETTINGS_FILE)+1);
	strcpy(settingsfile, SETTINGS_FILE);	
	
	progname = malloc(12);
	strcpy(progname, "pilight-raw");	
	
	options_add(&options, 'H', "help", no_value, 0, NULL);
	options_add(&options, 'V', "version", no_value, 0, NULL);
	options_add(&options, 'S', "settings", has_value, 0, NULL);

	while (1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &args);
		if(c == -1 || c == -2)
			break;
		switch (c) {
			case 'H':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");		
				printf("\t -S --settings\t\tsettings file\n");
				return (EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s %s\n", progname, VERSION);
				return (EXIT_SUCCESS);
			break;	
			case 'S': 
				if(access(args, F_OK) != -1) {
					settingsfile = realloc(settingsfile, strlen(args)+1);
					strcpy(settingsfile, args);
					settings_set_file(args);
				} else {
					fprintf(stderr, "%s: the settings file %s does not exists\n", progname, args);
					return EXIT_FAILURE;
				}
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	options_delete(options);

	if(access(settingsfile, F_OK) != -1) {
		if(settings_read() != 0) {
			return EXIT_FAILURE;
		}
	}	
	
	hardware_init();
	
	if(settings_find_string("hardware-file", &hwfile) == 0) {
		hardware_set_file(hwfile);
		if(hardware_read() == EXIT_FAILURE) {
			goto clear;
		}
	}

	/* Start threads library that keeps track of all threads used */
	pthread_create(&pth, NULL, &threads_start, (void *)NULL);	

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->init() == EXIT_FAILURE) {
			logprintf(LOG_ERR, "could not initialize %s hardware mode", tmp_confhw->hardware->id);
			goto clear;
		}
		threads_register(tmp_confhw->hardware->id, &receive_code, (void *)tmp_confhw->hardware);
		tmp_confhw = tmp_confhw->next;
	}

	while(main_loop) {
		sleep(1);
	}

	main_gc();
	return (EXIT_SUCCESS);
clear:
	main_gc();
	return (EXIT_FAILURE);
}
