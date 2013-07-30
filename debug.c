/*
	Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
	Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
	Copyright (C) 2013 CurlyMo

	This file is part of the QPido.

    QPido is free software: you can redistribute it and/or modify it 
	under the terms of the GNU General Public License as published by 
	the Free Software Foundation, either version 3 of the License, or 
	(at your option) any later version.

    QPido is distributed in the hope that it will be useful, but 
	WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
	General Public License for more details.

    You should have received a copy of the GNU General Public License 
	along with QPido. If not, see <http://www.gnu.org/licenses/>

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

#include "settings.h"
#include "log.h"
#include "options.h"

#include "lirc.h"
#include "lirc/ir_remote.h"
#include "lirc/hardware.h"
#include "lirc/hw-types.h"

int normalize(int i) {
	double x;
	x=(double)i/PULSE_LENGTH;

	return (int)(round(x));
}

int main(int argc, char **argv) {

	enable_shell_log();
	disable_file_log();
	set_loglevel(LOG_NOTICE);

	progname = malloc((11*sizeof(char))+1);
	progname = strdup("qpido-debug");

	lirc_t data;
	char *socket = strdup(DEFAULT_LIRC_SOCKET);
	int have_device = 0;

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

	hw_choose_driver(NULL);

	addOption(&options, 'H', "help", no_value, 0, NULL);
	addOption(&options, 'V', "version", no_value, 0, NULL);
	addOption(&options, 'S', "socket", has_value, 0, "^/dev/([A-Za-z]+)([0-9]+)");

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
				printf("\t -S --socket=socket\tread from given socket\n");
				return (EXIT_SUCCESS);
			break;
			case 'v':
				printf("%s %s\n", progname, "1.0");
				return (EXIT_SUCCESS);
			break;
			case 's':
				socket = optarg;
				have_device = 1;
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}

	if(strcmp(socket, "/var/lirc/lircd") == 0) {
		logprintf(LOG_ERR, "refusing to connect to lircd socket");
		return EXIT_FAILURE;
	}

	if(have_device)
		hw.device = socket;

	if(!hw.init_func()) {
		return EXIT_FAILURE;
	}

/*
End of the original (but stripped) code of mode2
*/

	while (1) {
		data = hw.readdata(0);
		duration = (data & PULSE_MASK);

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
					break;
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
	binaryLength = i/4;

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
