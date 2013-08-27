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

#include "settings.h"
#include "log.h"
#include "options.h"
#include "wiringPi.h"
#include "libs/lirc/lirc.h"
#include "libs/lirc/ir_remote.h"
#include "libs/lirc/hardware.h"
#include "libs/lirc/hw-types.h"
#include "irq.h"

int normalize(int i) {
	double x;
	x=(double)i/PULSE_LENGTH;

	return (int)(round(x));
}

int main(int argc, char **argv) {

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	progname = malloc(15);
	strcpy(progname, "pilight-debug");

	struct options_t *options = NULL;	
	
	lirc_t data;
	char *socket = malloc(11);
	strcpy(socket, "/dev/lirc0");
	int have_device = 0;
	int use_lirc = USE_LIRC;
	int gpio_in = GPIO_IN_PIN;

	int duration = 0;
	int i = 0;
	int y = 0;

	int recording = 1;
	int bit = 0;
	int raw[255];
	int pRaw[255];
	int code[255];
	int binary[255];
	int footer = 0;
	int header = 0;
	int pulse = 0;
	int rawLength = 0;
	int binaryLength = 0;

	int loop = 1;

	options_add(&options, 'H', "help", no_value, 0, NULL);
	options_add(&options, 'V', "version", no_value, 0, NULL);
	options_add(&options, 'S', "socket", has_value, 0, "^/dev/([A-Za-z]+)([0-9]+)");
	options_add(&options, 'L', "lirc", no_value, 0, NULL);
	options_add(&options, 'G', "gpio", has_value, 0, "^[0-7]$");

	while (1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &optarg);
		if(c == -1)
			break;
		switch (c) {
			case 'H':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");		
				printf("\t -S --socket=socket\tread from given socket\n");
				printf("\t -L --lirc\t\tuse the lirc_rpi kernel module\n");
				printf("\t -G --gpio=#\t\tGPIO pin we're directly reading from\n");
				return (EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s %s\n", progname, "1.0");
				return (EXIT_SUCCESS);
			break;	
			case 'S':
				socket = realloc(socket, strlen(optarg)+1);
				strcpy(socket, optarg);
				have_device = 1;
			break;
			case 'L':
				use_lirc = 1;
			break;
			case 'G':
				gpio_in = atoi(optarg);
				use_lirc = 0;
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	options_delete(options);

	if(use_lirc == 1) {
		hw_choose_driver(NULL);
		if(strcmp(socket, "/var/lirc/lircd") == 0) {
			logprintf(LOG_ERR, "refusing to connect to lircd socket");
			return EXIT_FAILURE;
		}

		if(have_device)
			hw.device = socket;

		if(!hw.init_func()) {
			return EXIT_FAILURE;
		}
	} else {

		if(wiringPiSetup() == -1)
			return EXIT_FAILURE;

		if(wiringPiISR(gpio_in, INT_EDGE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_in) ;
			return 1 ;
		}
		(void)piHiPri(55);
	}

	printf("Please make sure the daemon is not running when using this debugger.\n\n");
	printf("Now press and hold one of the button on your remote or wait until\n");
	printf("another device such as a weather station has send new codes\n");
	printf("It is possible that the debugger needs to be restarted when it does.\n");
	printf("not show anything. This is because it's then following a wrong lead.\n");

	while(loop) {
		if(use_lirc == 1) {
			data = hw.readdata(0);
			duration = (data & PULSE_MASK);
		} else {
			duration = irq_read(gpio_in);
		}

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
		if((duration > 5000
		   && duration < 100000 && footer == 0) || ((footer-(footer*0.1)<duration) && (footer+(footer*0.1)>duration))) {
			recording = 1;

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
				/* If we are certain we are recording similar codes.
				   Save the header values and the raw code length */
				if(footer>0) {
					if(header == 0) {
						header=raw[1];
					}
					if(rawLength == 0)
						rawLength=bit;
				}
				/* Try to catch the footer, and the low and high values */
				for(i=0;i<bit;i++) {
					if((i+1)<bit && i > 2 && footer > 0) {
						if((raw[i]/PULSE_LENGTH) >= 2) {
							pulse=raw[i];
						}
					}
					if(duration > 5000 && duration < 100000) {
						footer=raw[i];
					}
				}

				/* If we have gathered all data, stop with the loop */
				if(header > 0 && footer > 0 && pulse > 0 && rawLength > 0) {
					loop = 0;
				}
			}
			bit=0;
		}

		fflush(stdout);
	};

	/* Convert the raw code into binary code */
	for(i=0;i<rawLength;i++) {
		if((unsigned int)raw[i] > (pulse-PULSE_LENGTH)) {
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
	if(normalize(header) == normalize(pulse)) {
		printf("header:\t\t0\n");
	} else {
		printf("header:\t\t%d\n",normalize(header));
	}
	printf("pulse:\t\t%d\n",normalize(pulse));
	printf("footer:\t\t%d\n",normalize(footer));
	printf("rawLength:\t%d\n",rawLength);
	printf("binaryLength:\t%d\n",binaryLength);
	printf("\n");
	printf("Raw code:\n");
	for(i=0;i<rawLength;i++) {
		printf("%d ",normalize(raw[i])*PULSE_LENGTH);
	}
	printf("\n");
	printf("Binary code:\n");
	for(i=0;i<binaryLength;i++) {
		printf("%d",binary[i]);
	}
	printf("\n");
	
	free(progname);
	free(socket);
	return (EXIT_SUCCESS);
}
