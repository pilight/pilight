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
#include <time.h>
#include <math.h>
#include <string.h>

#include "pilight.h"
#include "common.h"
#include "config.h"
#include "hardware.h"
#include "log.h"
#include "options.h"
#include "threads.h"
#include "wiringX.h"
#include "datetime.h"
#include "ssdp.h"
#include "socket.h"
#include "irq.h"
#include "gc.h"
#include "dso.h"

static int pulselen = 0;
static unsigned short main_loop = 1;
static unsigned short inner_loop = 1;
static pthread_t pth;

static int normalize(int i) {
	double x;
	x=(double)i/pulselen;

	return (int)(round(x));
}

int main_gc(void) {
	log_shell_disable();
	main_loop = 0;
	inner_loop = 0;

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->deinit) {
			tmp_confhw->hardware->deinit();
		}
		tmp_confhw = tmp_confhw->next;
	}

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

	sfree((void *)&progname);
	return EXIT_SUCCESS;
}

void *receive_code(void *param) {
	int duration = 0;
	int i = 0;
	unsigned int y = 0;

	int recording = 1;
	int bit = 0;
	int raw[MAXPULSESTREAMLENGTH] = {0};
	int pRaw[MAXPULSESTREAMLENGTH] = {0};
	int code[MAXPULSESTREAMLENGTH] = {0};
	int binary[MAXPULSESTREAMLENGTH/2] = {0};
	int footer = 0;
	int pulse = 0;
	int rawLength = 0;
	int binaryLength = 0;

	time_t now = 0, later = 0;

	struct hardware_t *hw = (hardware_t *)param;
	if(hw->init) {
		hw->init();
	}
	while(main_loop) {
		memset(&raw, '\0', MAXPULSESTREAMLENGTH);
		memset(&pRaw, '\0', MAXPULSESTREAMLENGTH);
		memset(&code, '\0', MAXPULSESTREAMLENGTH);
		memset(&binary, '\0', MAXPULSESTREAMLENGTH/2);
		recording = 1;
		bit = 0;
		footer = 0;
		pulse = 0;
		rawLength = 0;
		binaryLength = 0;
		inner_loop = 1;

		duration = 0;
		i = 0;
		y = 0;
		time(&now);

		while(inner_loop && hw->receive) {
			duration = hw->receive();
			time(&later);
			if(difftime(later, now) > 1) {
				inner_loop = 0;
			}
			/* If we are recording, keep recording until the next footer has been matched */
			if(recording == 1) {
				if(bit < MAXPULSESTREAMLENGTH) {
					raw[bit++] = duration;
				} else {
					bit = 0;
					recording = 0;
				}
			}

			/* First try to catch code that seems to be a footer.
			   If a real footer has been recognized, start using that as the new footer */
			if((duration > 5100 && footer == 0) || ((footer-(footer*0.3)<duration) && (footer+(footer*0.3)>duration))) {
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
				if(y>=1) {
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
						if(duration > 5100) {
							footer=raw[i];
						}
					}

					/* If we have gathered all data, stop with the loop */
					if(footer > 0 && pulse > 0 && rawLength > 0) {
						inner_loop = 0;
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
		if(rawLength > 0 && normalize(pulse) > 0 && rawLength > 25) {
			/* Print everything */
			printf("--[RESULTS]--\n");
			printf("\n");
			printf("time:\t\t%s", asctime(localtime(&now)));
			printf("hardware:\t%s\n", hw->id);
			printf("pulse:\t\t%d\n", normalize(pulse));
			printf("rawlen:\t\t%d\n", rawLength);
			printf("binlen:\t\t%d\n", binaryLength);
			printf("pulselen:\t%d\n", pulselen);
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
		}
	}

	main_loop = 0;

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
	pid_t pid = 0;

	char configtmp[] = CONFIG_FILE;
	config_set_file(configtmp);

	progname = malloc(15);
	if(!progname) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-debug");

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
				goto clear;
			break;
			case 'V':
				printf("%s %s\n", progname, VERSION);
				goto clear;
			break;
			case 'C':
				if(config_set_file(args) == EXIT_FAILURE) {
					goto clear;
				}
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				goto clear;
			break;
		}
	}
	options_delete(options);

	char *pilight_daemon = strdup("pilight-daemon");
	if(!pilight_daemon) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if((pid = findproc(pilight_daemon, NULL, 1)) > 0) {
		logprintf(LOG_ERR, "pilight-daemon instance found (%d)", (int)pid);
		sfree((void *)&pilight_daemon);
		goto clear;
	}
	sfree((void *)&pilight_daemon);

	char *pilight_raw = strdup("pilight-raw");
	if(!pilight_raw) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if((pid = findproc(pilight_raw, NULL, 1)) > 0) {
		logprintf(LOG_ERR, "pilight-raw instance found (%d)", (int)pid);
		sfree((void *)&pilight_raw);
		goto clear;
	}
	sfree((void *)&pilight_raw);

	protocol_init();
	config_init();
	if(config_read() != EXIT_SUCCESS) {
		goto clear;
	}

	/* Start threads library that keeps track of all threads used */
	threads_create(&pth, NULL, &threads_start, (void *)NULL);

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->init() == EXIT_FAILURE) {
			logprintf(LOG_ERR, "could not initialize %s hardware mode", tmp_confhw->hardware->id);
			goto clear;
		}
		threads_register(tmp_confhw->hardware->id, &receive_code, (void *)tmp_confhw->hardware, 0);
		tmp_confhw = tmp_confhw->next;
	}

	printf("Press and hold one of the buttons on your remote or wait until\n");
	printf("another device such as a weather station has send new codes\n");
	printf("The debugger will automatically reset itself after one second of\n");
	printf("failed leads. It will keep running until you explicitly stop it.\n");
	printf("This is done by pressing both the [CTRL] and C buttons on your keyboard.\n");

	while(main_loop) {
		sleep(1);
	}

clear:
	if(main_loop) {
		main_gc();
	}
	return (EXIT_FAILURE);
}
