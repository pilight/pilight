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
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include "drivers/lirc.h"
#include "daemons/ir_remote.h"
#include "daemons/hardware.h"
#include "daemons/hw-types.h"

#include "protocol.h"
#include "protocols/kaku_switch.h"
#include "protocols/kaku_dimmer.h"
#include "protocols/elro.h"

/*
Start of the original (but stripped) code of mode2
*/

char *progname = "receive";

void logprintf(int prio, char *format_str, ...) {
	va_list ap;

	fprintf(stderr, "%s: ", progname);
	va_start(ap, format_str);
	if (prio == LOG_WARNING)
		fprintf(stderr, "WARNING: ");
	vfprintf(stderr, format_str, ap);
	fputc('\n', stderr);
	fflush(stderr);
	va_end(ap);
}

void logperror(int prio, const char *s) {
	if (s != NULL) {
		logprintf(prio, "%s: %s", s, strerror(errno));
	} else {
		logprintf(prio, "%s", strerror(errno));
	}
} 

int waitfordata(unsigned long maxusec) {
	fd_set fds;
	int ret;
	struct timeval tv;

	while (1) {
		FD_ZERO(&fds);
		FD_SET(hw.fd, &fds);
		do {
			do {
				if (maxusec > 0) {
					tv.tv_sec = maxusec / 1000000;
					tv.tv_usec = maxusec % 1000000;
					ret = select(hw.fd + 1, &fds, NULL, NULL, &tv);
					if (ret == 0)
						return (0);
				} else {
					ret = select(hw.fd + 1, &fds, NULL, NULL, NULL);
				}
			}
			while (ret == -1 && errno == EINTR);
			if (ret == -1) {
				logprintf(LOG_ERR, "select() failed\n");
				logperror(LOG_ERR, NULL);
				continue;
			}
		}
		while (ret == -1);

		if (FD_ISSET(hw.fd, &fds)) {
			/* we will read later */
			return (1);
		}
	}
}

int main(int argc, char **argv) {
	lirc_t data;
	char *socket = "/dev/lircd";
	int have_device = 0;
	
	int duration = 0;
	protocol *device;

	int i = 0;
	int x = 0;
	int y = 0;
	
	hw_choose_driver(NULL);
	while (1) {
		int c;
		static struct option long_options[] = {
			{"help", no_argument, NULL, 'h'},
			{"version", no_argument, NULL, 'v'},
			{"device", required_argument, NULL, 'd'},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv, "hvd:", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				printf("Usage: %s [options]\n", progname);
				printf("\t -h --help\t\tdisplay usage summary\n");
				printf("\t -v --version\t\tdisplay version\n");
				printf("\t -d --device=device\tread from given device\n");
				return (EXIT_SUCCESS);
			break;
			case 'v':
				printf("%s %s\n", progname, "1.0");
				return (EXIT_SUCCESS);
			break;
			case 'd':
				socket = optarg;
				have_device = 1;
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	if (optind < argc) {
		fprintf(stderr, "%s: too many arguments\n", progname);
		return EXIT_FAILURE;
	}
	
	if (strcmp(socket, "/var/lirc/lircd") == 0) {
		fprintf(stderr, "%s: refusing to connect to lircd socket\n", progname);
		return EXIT_FAILURE;
	}

	if (have_device)
		hw.device = socket;
		
	if (!hw.init_func())
		return EXIT_FAILURE;
	
/*
End of the original (but stripped) code of mode2
*/	
	
	/* Initialize peripheral modules */
	kakuSwInit();
	kakuDimInit();
	elroInit();	
	
	while (1) {
		data = hw.readdata(0);		
		duration = (data & PULSE_MASK);				
		
		for(i=0; i<protos.nr; ++i) {
			device = protos.listeners[i];
			
			/* Try to catch the header of the code */
			if(duration > (device->header[0]-(device->header[0]*device->multiplier[0])) 
			   && duration < (device->header[0]+(device->header[0]*device->multiplier[0])) 
			   && device->bit == 0) {				
				device->raw[device->bit++] = duration;
			} else if(duration > (device->header[1]-(device->header[1]*device->multiplier[0])) 
			   && duration < (device->header[1]+(device->header[1]*device->multiplier[0])) 
			   && device->bit == 1) {
				device->raw[device->bit++] = duration;
				device->recording = 1;
			}
			
			/* If we are recording, keep recording until the footer has been matched */
			if(device->recording == 1) {
				if(device->bit < 255) {
					device->raw[device->bit++] = duration;
				} else {
					device->bit = 0;
					device->recording = 0;
				}
			}
			
			/* Try to catch the footer of the code */
			if(duration > (device->footer-(device->footer*device->multiplier[1])) 
			   && duration < (device->footer+(device->footer*device->multiplier[1]))) {

				/* Check if the code matches the raw length */
				if(device->bit == device->rawLength) {
					device->parseRaw();
					
					/* Convert the raw codes to one's and zero's */
					for(x=0;x<device->bit;x++) {
					
						/* Check if the current code matches the previous one */
						if(device->pCode[x]!=device->code[x])
							y=0;
						device->pCode[x]=device->code[x];
						if(device->raw[x] > device->crossing) {	
							device->code[x]=1;
						} else {
							device->code[x]=0;
						}
					}
				
					y++;
					
					/* Continue if we have recognized enough repeated codes */
					if(y >= device->repeats) {
						device->parseCode();
						
						/* Convert the one's and zero's into binary */
						for(x=2; x<device->bit; x+=4) {
							if(device->code[x+2] == 1) {
								device->binary[x/4]=1;
							} else {
								device->binary[x/4]=0;
							}
						}
						
						/* Check if the binary matches the binary length */
						if((x/4) == device->binaryLength)
							device->parseBinary();
					}
				}
				device->recording = 0;
				device->bit = 0;
			}
		}
		
		fflush(stdout);
	};
	return (EXIT_SUCCESS);
}