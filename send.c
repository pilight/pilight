/*	
	Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
	Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
	Copyright (C) 2013 CurlyMo
	
	This file is part of the Raspberry Pi 433.92Mhz transceiver.

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

#define _GNU_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "daemons/lircd.h"
#include "daemons/hardware.h"
#include "daemons/hw-types.h"
#include "daemons/transmit.h"
#include "daemons/hw_default.h"

#include "protocol.h"
#include "protocols/kaku_switch.h"
#include "protocols/kaku_dimmer.h"
#include "protocols/kaku_old.h"
#include "protocols/elro.h"

extern struct hardware hw;

char *progname = "send";

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

int waitfordata(long maxusec) { 
	return 1;
}

int main(int argc, char **argv) {
	char *socket = NULL;
	struct ir_ncode code;
	
	int i;
	int match = 0;
	int success = 0;
	
	int dimlevel = -1;
	int unit = -1;
	int id = -1;
	int state = -1;
	int all = -1;
	int repeat = 5;
	protocol *device = NULL;
	
	hw_choose_driver(NULL);
	
	/* Initialize peripheral modules */
	kakuSwInit();
	kakuDimInit();
	kakuOldInit();
	elroInit();		
	
	while (1) {
		int c;
		static struct option long_options[] = {
			{"help", no_argument, NULL, 'h'},
			{"version", no_argument, NULL, 'v'},
			{"socket", required_argument, NULL, 'd'},
			{"protocol", required_argument, NULL, 'p'},
			{"on", no_argument, NULL, 't'},
			{"off", no_argument, NULL, 'f'},
			{"unit", required_argument, NULL, 'u'},
			{"id", required_argument, NULL, 'i'},
			{"dimlevel", required_argument, NULL, 'd'},
			{"repeat", required_argument, NULL, 'r'},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv, "hvs:tfu:i:ad:r:p:", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			printf("Usage: %s [options]\n", progname);
			printf("\t -h --help\t\t\tdisplay this message\n");
			printf("\t -v --version\t\t\tdisplay version\n");
			printf("\t -s --socket=socket\t\tread from given socket\n");
			printf("\t -p --protocol=protocol\t\twhich device are you trying to control\n");
			printf("\t -t --on\t\t\tsend an on signal\n");
			printf("\t -t --off\t\t\tsend an off signal\n");
			printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
			printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
			printf("\t -a --all\t\t\tsend command to all devices with this id\n");
			printf("\t -d --dimlevel\t\t\tsend a specific dimlevel\n");
			printf("\t -r --repeat=repeat\t\tnumber of times the command is send\n");
			return (EXIT_SUCCESS);
		case 'v':
			printf("%s %s\n", progname, "1.0");
			return (EXIT_SUCCESS);
		case 's':
			socket = optarg;
			break;
		case 'p':
			for(i=0; i<protos.nr; ++i) {
				device = protos.listeners[i];
				if(strcmp(device->id,optarg) == 0) {
					match=1;
					break;
				}
			}
			if(!match) {
				printf("This protocol is not supported\n");
				printf("The supported protocols are:\n");
				for(i=0; i<protos.nr; ++i) {
					device = protos.listeners[i];
					printf("\t- %s:\t",device->id);
					if(strlen(device->id) < 8)
						printf("\t");
					printf("%s\n", device->desc);
				}
				return (EXIT_FAILURE);
			}
			break;
		case 't':
			if(state > -1 || dimlevel > -1) {
				printf("You can't combine -t, -f and -d\n");
				return (EXIT_FAILURE);
			} else {
				state = 1;
			}
		break;
		case 'f':
			if(state > -1 || dimlevel > -1) {
				printf("You can't combine -t, -f and -d\n");
				return (EXIT_FAILURE);
			} else {
				state = 0;
			}
		break;
		case 'u':
			if(all > -1) {
				printf("You can't combine -a and -u\n");
				return (EXIT_FAILURE);
			} else {
				unit = atoi(optarg);
			}
		break;
		case 'i':
			id = atoi(optarg);
		break;
		case 'a':
			if(unit > -1) {
				printf("You can't combine -a and -u\n");
				return (EXIT_FAILURE);
			} else {
				all = 1;
			}
		break;
		case 'd':
			if(state > -1 || dimlevel > -1) {
				printf("You can't combine -t, -f and -d\n");
				return (EXIT_FAILURE);
			} else {
				dimlevel = atoi(optarg);
			}
		break;
		case 'r':
			repeat = atoi(optarg);
		break;
		default:
			printf("Usage: %s [options]\n", progname);
			return (EXIT_FAILURE);
		}
	}
	if((id == -1 && (unit == -1 || all == -1)) 
	   || (unit == -1 && all == -1)
	   || (optind != argc) || (argc < 3)) {
		fprintf(stderr, "%s: invalid argument count\n", progname);
		return (EXIT_FAILURE);
	}

	if (socket != NULL) {
		hw.device = socket;
	}
	if (strcmp(hw.name, "null") == 0) {
		fprintf(stderr, "%s: there's no hardware I can use and no peers are specified\n", progname);
		return (EXIT_FAILURE);
	}

	if (!hw.init_func())
		return EXIT_FAILURE;
		
	if (hw.send_mode == 0)
		return EXIT_FAILURE;
	
	if(match) {
		device->createCode(id, unit, state, all, dimlevel);
		code.length = (device->rawLength+1);
		code.signals = device->raw;

		for(i=0;i<repeat;i++) {	
			if((success = send_ir_ncode(&code)) != 1)
				return EXIT_FAILURE;
			usleep(25000);
		}
	}
	
	return (EXIT_SUCCESS);
}
