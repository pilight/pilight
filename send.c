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

#include "lirc.h"
#include "daemons/lircd.h"
#include "daemons/hardware.h"
#include "daemons/hw-types.h"
#include "daemons/transmit.h"
#include "daemons/hw_default.h"

#include "protocol.h"
#include "options.h"
#include "protocols/kaku_switch.h"
#include "protocols/kaku_dimmer.h"
#include "protocols/kaku_old.h"
#include "protocols/elro.h"
#include "protocols/raw.h"

extern struct hardware hw;

void logprintf(int prio, char *format_str, ...) { }

void logperror(int prio, const char *s) { } 

int main(int argc, char **argv) {
	char *progname = "send";
	char *socket = "/dev/lirc0";
	int have_device = 0;
	/* The code that is send to the hardware wrapper */
	struct ir_ncode code;	
	
	/* Hold the name of the protocol */
	char protobuffer[255];
	int i;
	/* Does this protocol exists */
	int match = 0;
	/* Did the sending succeed */
	int success = 0;		
	/* How many times does to keep need to be resend */
	int repeat = 5;
	
	/* Do we need to print the help */
	int help = 0;
	/* Do we need to print the version */
	int version = 0;
	/* Do we need to print the protocol help */
	int protohelp=0;	
	
	/* Hold the final protocol struct */
	protocol *device = NULL;
	
	/* The short CLI arguments the main program has */
	char optstr[255]={'h','v','s',':','p',':','r',':','\0'};
	/* The long CLI arguments the program has */
	struct option mainOptions[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"protocol", required_argument, NULL, 'p'},
		{"repeat", required_argument, NULL, 'r'},
		{0,0,0,0}
	};
	/* Make sure the long options are a static array to prevent unexpected behavior */
	struct option *long_options=setOptions(mainOptions);
	/* Number of main options */
	int long_opt_nr = (sizeof(long_options)/sizeof(long_options[0])-1);
	/* Number of option from the protocol */
	int proto_opt_nr;
	
	/* Temporary pointer to the protocol options */
	struct option *backup_options;	
	
	/* Structs holding all CLI arguments and their values */
	struct options *options = NULL;
	struct options *node = NULL;
	
	/* The long CLI argument holder */
	char longarg[10];
	/* The abbreviated CLI argument holder */
	char shortarg[2];	
	
	/* Initialize the default HW driver */
	hw_choose_driver(NULL);
	
	/* Initialize peripheral modules */
	kakuSwInit();
	kakuDimInit();
	kakuOldInit();
	elroInit();			
	rawInit();			
	
	/* Clear the argument holders */
	memset(longarg,'\0',11);
	memset(shortarg,'\0',3);
	
	/* Check if the basic CLI arguments are given before the protocol arguments are appended */
	for(i=0;i<argc;i++) {
		memcpy(longarg, &argv[i][0], 10);
		memcpy(shortarg, &argv[i][0], 2);

		if(strcmp(shortarg,"-h") == 0 || strcmp(longarg,"--help") == 0)
			help=1;
		if(strcmp(shortarg,"-p") == 0 || strcmp(longarg,"--protocol") == 0) {
			if(strcmp(longarg,"--protocol") == 0)
				memcpy(protobuffer,&argv[i][11],strlen(argv[i]));
			else if(i<argc-1)
				memcpy(protobuffer,argv[i+1],strlen(argv[i+1]));
			continue;
		}
		if(strcmp(shortarg,"-v") == 0 || strcmp(longarg,"--version") == 0)
			version=1;
	}
	
	/* Check if a protocol was given */
	if(strlen(protobuffer) > 0 && strcmp(protobuffer,"-v") != 0) {
		if(strlen(protobuffer) > 0 && version) {
			printf("-p and -v cannot be combined\n");
		} else {
			for(i=0; i<protos.nr; ++i) {
				device = protos.listeners[i];
				/* Check if the protocol exists */
				if(strcmp(device->id,protobuffer) == 0 && match == 0) {
					match=1;
					/* Check if the protocol requires specific CLI arguments */
					if(device->options != NULL && help == 0) {

						/* Copy all CLI options from the specific protocol */
						backup_options=device->options;
						size_t y=strlen(optstr);
						proto_opt_nr = long_opt_nr;
						while(backup_options->name != NULL) {
							long_options[long_opt_nr].name=backup_options->name;
							long_options[long_opt_nr].flag=backup_options->flag;
							long_options[long_opt_nr].has_arg=backup_options->has_arg;
							long_options[long_opt_nr].val=backup_options->val;

							optstr[y++]=long_options[long_opt_nr].val;
							if(long_options[long_opt_nr].has_arg == 1) {
								optstr[y++]=':';
							}
							backup_options++;
							proto_opt_nr++;
						}
						optstr[y++]='\0';
					} else if(help == 1) {
						protohelp=1;
					}
					break;
				}
			}
			/* If no protocols matches the requested protocol */
			if(!match) {
				fprintf(stderr, "%s: this protocol is not supported\n", progname);
			}
		}
	}
	
	/* Display help or version information */
	if(version == 1) {
		printf("%s %s\n", progname, "1.0");
		return (EXIT_SUCCESS);	
	} else if(help == 1 || protohelp == 1 || match == 0) {
		if(protohelp == 1 && match == 1 && device->printHelp != NULL)
			printf("Usage: %s -p %s [options]\n", progname, device->id);
		else
			printf("Usage: %s -p protocol [options]\n", progname);
		if(help == 1) {
			printf("\t -h --help\t\t\tdisplay this message\n");
			printf("\t -v --version\t\t\tdisplay version\n");
			printf("\t -p --protocol=protocol\t\tthe device that you want to control\n");
			printf("\t -s --socket=socket\t\tread from given socket\n");
			printf("\t -r --repeat=repeat\t\tnumber of times the command is send\n");
		}
		if(protohelp == 1 && match == 1 && device->printHelp != NULL) {
			printf("\n\t[%s]\n",device->id);
			device->printHelp();
		} else {
			printf("\nThe supported protocols are:\n");
			for(i=0; i<protos.nr; ++i) {
				device = protos.listeners[i];
				printf("\t %s\t\t\t",device->id);
				if(strlen(device->id)<5)
					printf("\t");
				printf("%s\n", device->desc);
			}
		}
		return (EXIT_SUCCESS);		
	}
	
	/* Store all CLI arguments for later usage */
	while (1) {
		int c;
		c = getopt_long(argc, argv, optstr, long_options, NULL);
		if(c == -1)
			break;
		if(c == 's') {
			socket = optarg;
			have_device=1;
		}
		if(c == 'r') {
			repeat = atoi(optarg);
		}
		if(strchr(optstr,c)) {
			node = malloc(sizeof(struct options));
			node->id = c;
			node->value = optarg;
			node->next = options;
			options = node;
		}
	}
	
	if(strcmp(socket, "/var/lirc/lircd") == 0) {
		fprintf(stderr, "%s: refusing to connect to lircd socket\n", progname);
		return EXIT_FAILURE;
	}

	if(have_device)
		hw.device = socket;

	if(!hw.init_func()) {
		fprintf(stderr, "%s: could not open %s\n", progname, hw.device);
		fprintf(stderr, "%s: default_init(): Device or resource busy\n", progname);
		return EXIT_FAILURE;
	}
		
	if(hw.send_mode == 0)
		return EXIT_FAILURE;
	
	/* Send the final code */
	if(match) {
		device->createCode(node);
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
