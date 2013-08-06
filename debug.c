/*
	Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
	Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
	Copyright (C) 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver,
	and based on mode2 as part of the package Lirc.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>

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

#ifdef USE_LIRC

#include "lirc.h"
#include "lirc/ir_remote.h"
#include "lirc/hardware.h"
#include "lirc/hw-types.h"

#else

#include <wiringPi.h>
#include "gpio.h"
#include "irq.h"

#endif

int normalize(int i) {
	double x;
	x=(double)i/PULSE_LENGTH;

	return (int)(round(x));
}

int main(int argc, char **argv) {

	enable_shell_log();
	disable_file_log();
	set_loglevel(LOG_NOTICE);

	progname = malloc((10*sizeof(char))+1);
	progname = strdup("433-debug");

#ifdef USE_LIRC
	lirc_t data;
	char *socket = strdup("/dev/lirc0");
	int have_device = 0;
#else
	int newDuration;
#endif

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

#ifdef USE_LIRC
	hw_choose_driver(NULL);
#endif

	addOption(&options, 'H', "help", no_value, 0, NULL);
	addOption(&options, 'V', "version", no_value, 0, NULL);
#ifdef USE_LIRC	
	addOption(&options, 'S', "socket", has_value, 0, "^/dev/([A-Za-z]+)([0-9]+)");
#endif

	while (1) {
		int c;
		c = getOptions(&options, argc, argv, 1);
		if(c == -1)
			break;
		switch (c) {
			case 'h':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");
#ifdef USE_LIRC				
				printf("\t -S --socket=socket\tread from given socket\n");
#endif
				return (EXIT_SUCCESS);
			break;
			case 'v':
				printf("%s %s\n", progname, "1.0");
				return (EXIT_SUCCESS);
			break;
#ifdef USE_LIRC			
			case 'S':
				socket = optarg;
				have_device = 1;
			break;
#endif
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}

#ifdef USE_LIRC
	if(strcmp(socket, "/var/lirc/lircd") == 0) {
		logprintf(LOG_ERR, "refusing to connect to lircd socket");
		return EXIT_FAILURE;
	}

	if(have_device)
		hw.device = socket;

	if(!hw.init_func()) {
		return EXIT_FAILURE;
	}
#endif

/*
End of the original (but stripped) code of mode2
*/

#ifndef USE_LIRC
	if(gpio_request(GPIO_IN_PIN) == 0) {
		/* Attach an interrupt to the requested pin */
		irq_attach(GPIO_IN_PIN, CHANGE);
	} else {
		return EXIT_FAILURE;		
	}
	
	printf("Please make sure the daemon is not running when using this debugger.\n\n");
	printf("Now press and hold one of the button on your remote or wait until\n");
	printf("another device such as a weather station has send new codes\n");
	printf("It is possible that the debugger needs to be restarted when it does.\n");
	printf("not show anything. This is because it's then following a wrong lead.\n");

#endif

	while(loop) {
#ifdef USE_LIRC
		data = hw.readdata(0);
		duration = (data & PULSE_MASK);
#else

		unsigned short a = 0;

		if((newDuration = irq_read()) == 1) {
			continue;
		}

		for(a=0;a<2;a++) {
			if(a==0) {
				duration = PULSE_LENGTH;
			} else {
				duration = newDuration;
				if(duration < 750) {
					duration = PULSE_LENGTH;
				}
			}
#endif

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
#ifndef USE_LIRC
		}
#endif
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
	printf("header:\t\t%d\n",normalize(header));
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
	return (EXIT_SUCCESS);
}
