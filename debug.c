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
#include "common.h"
#include "settings.h"
#include "hardware.h"
#include "log.h"
#include "options.h"
#include "threads.h"
#include "wiringPi.h"
#include "irq.h"
#include "gc.h"

int pulselen = 0;
unsigned short main_loop = 1;
pthread_t pth;

int normalize(int i) {
	double x;
	x=(double)i/pulselen;

	return (int)(round(x));
}

int main_gc(void) {
	log_shell_disable();
	main_loop = 0;

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
	int i = 0;
	int y = 0;

	int recording = 1;
	int bit = 0;
	int raw[255] = {0};
	int pRaw[255] = {0};
	int code[255] = {0};
	int binary[255] = {0};
	int footer = 0;
	int pulse = 0;
	int rawLength = 0;
	int binaryLength = 0;

	struct hardware_t *hardware = (hardware_t *)param;
	if(hardware->init) {
		hardware->init();
	}

	while(main_loop && hardware->receive) {
		duration = hardware->receive();

		/* If we are recording, keep recording until the next footer has been matched */
		if(recording == 1) {
			if(bit < 255) {
				raw[bit++] = duration;
			} else {
				bit = 0;
				recording = 0;
			}
		}

		/* First try to catch code that seems to be a footer.
		   If a real footer has been recognized, start using that as the new footer */
		if((duration > 4440 && footer == 0) || ((footer-(footer*0.1)<duration) && (footer+(footer*0.1)>duration))) {
			recording = 1;
			pulselen = (int)duration/PULSE_DIV;
			/* Check if we are recording similar codes */
			for(i=0;i<(bit-1);i++) {
				if(!(((pRaw[i]-(pRaw[i]*0.3)) < raw[i]) && ((pRaw[i]+(pRaw[i]*0.3)) > raw[i]))) {
					y=0;
					recording=0;
				}
				pRaw[i]=raw[i];
			}
			y++;

			/* Continue if we have 2 matches */
			if(y>2) {
				/* If we are certain we are recording similar codes. Save the raw code length. */
				if(footer>0) {
					if(rawLength == 0)
						rawLength=bit;
				}
				/* Try to catch the footer, and the low and high values */
				for(i=0;i<bit;i++) {
					if((i+1)<bit && i > 2 && footer > 0) {
						if((raw[i]/pulselen) >= 2) {
							pulse=raw[i];
						}
					}
					if(duration > 4440) {
						footer=raw[i];
					}
				}

				/* If we have gathered all data, stop with the loop */
				if(footer > 0 && pulse > 0 && rawLength > 0) {
					main_loop = 0;
				}
			}
			bit=0;
		}

		fflush(stdout);
	};

	/* Convert the raw code into binary code */
	for(i=0;i<rawLength;i++) {
		if((unsigned int)raw[i] > (pulse-pulselen)) {
			code[i]=1;
		} else {
			code[i]=0;
		}
	}
	for(i=2;i<rawLength; i+=4) {
		if(code[i+1] == 1) {
			binary[i/4]=1;
		} else {
			binary[i/4]=0;
		}
	}
	
	binaryLength = (int)((float)i/4);

	/* Print everything */
	printf("--[RESULTS]--\n");
	printf("\n");
	printf("hardware:\t%s\n",hardware->id);
	printf("pulse:\t\t%d\n",normalize(pulse));
	printf("rawlen:\t\t%d\n",rawLength);
	printf("binlen:\t\t%d\n",binaryLength);
	printf("pulselen:\t%d\n",pulselen);
	printf("\n");
	printf("Raw code:\n");
	for(i=0;i<rawLength;i++) {
		printf("%d ",normalize(raw[i])*pulselen);
	}
	printf("\n");
	printf("Binary code:\n");
	for(i=0;i<binaryLength;i++) {
		printf("%d",binary[i]);
	}
	printf("\n");
	main_gc();
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
	pid_t pid = 0;
	
	settingsfile = malloc(strlen(SETTINGS_FILE)+1);
	if(!settingsfile) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(settingsfile, SETTINGS_FILE);	
	
	progname = malloc(15);
	if(!progname) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-debug");

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'S', "settings", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

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
					if(!settingsfile) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
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

	char pilight_daemon[] = "pilight-daemon";
	char pilight_learn[] = "pilight-learn";
	char pilight_raw[] = "pilight-raw";
	if((pid = findproc(pilight_daemon, NULL)) > 0) {
		logprintf(LOG_ERR, "pilight-daemon instance found (%d)", (int)pid);
		return (EXIT_FAILURE);
	}

	if((pid = findproc(pilight_learn, NULL)) > 0) {
		logprintf(LOG_ERR, "pilight-learn instance found (%d)", (int)pid);
		return (EXIT_FAILURE);
	}

	if((pid = findproc(pilight_raw, NULL)) > 0) {
		logprintf(LOG_ERR, "pilight-raw instance found (%d)", (int)pid);
		return (EXIT_FAILURE);
	}		

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
		threads_register(tmp_confhw->hardware->id, &receive_code, (void *)tmp_confhw->hardware, 0);
		tmp_confhw = tmp_confhw->next;
	}	

	printf("Now press and hold one of the buttons on your remote or wait until\n");
	printf("another device such as a weather station has send new codes\n");
	printf("It is possible that the debugger needs to be restarted when it does\n");
	printf("not show anything. This is because it's then following a wrong lead.\n");		
	
	while(main_loop) {
		sleep(1);
	}

	return (EXIT_SUCCESS);
clear:
	main_gc();
	return (EXIT_FAILURE);
}
