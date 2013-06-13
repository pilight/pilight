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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "lirc.h"
#include "lirc/lircd.h"
#include "lirc/hardware.h"
#include "lirc/hw-types.h"
#include "lirc/transmit.h"
#include "lirc/hw_default.h"

#include "protocol.h"
#include "options.h"
#include "config.h"
#include "protocols/kaku_switch.h"
#include "protocols/kaku_dimmer.h"
#include "protocols/kaku_old.h"
#include "protocols/elro.h"
#include "protocols/raw.h"

extern struct hardware hw;

void logprintf(int prio, char *format_str, ...) { }

void logperror(int prio, const char *s) { }

int main(int argc, char **argv) {
	progname = malloc((10*sizeof(char))+1);
	progname = "433-send";
	int sockfd = 0, n = 0, connected = 0;
    char recvBuff[BUFFER_SIZE];
    char message[BUFFER_SIZE];

	struct sockaddr_in serv_addr;

	/* Hold the name of the protocol */
	char protobuffer[255];
	int i;
	/* Does this protocol exists */
	int match = 0;

	/* Do we need to print the help */
	int help = 0;
	/* Do we need to print the version */
	int version = 0;
	/* Do we need to print the protocol help */
	int protohelp = 0;

	/* Hold the final protocol struct */
	protocol_t *device = NULL;

	/* The short CLI arguments the main program has */
	char optstr[255] = {'h','v','p',':','r',':','\0'};
	/* The long CLI arguments the program has */
	struct option mainOptions[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"protocol", required_argument, NULL, 'p'},
		{"repeat", required_argument, NULL, 'r'},
		{0,0,0,0}
	};
	/* Make sure the long options are a static array to prevent unexpected behavior */
	struct option *long_options=setOptions(mainOptions);
	/* Number of main options */
	int long_opt_nr = (sizeof(mainOptions)/sizeof(mainOptions[0])-1);
	/* Number of option from the protocol */
	int proto_opt_nr;

	/* Temporary pointer to the protocol options */
	struct option *backup_options;

	/* Structs holding all CLI arguments and their values */
	struct options_t *options = malloc(25*sizeof(struct options_t));
	struct options_t *node = malloc(sizeof(struct options_t));

	/* The long CLI argument holder */
	char longarg[10];
	/* The abbreviated CLI argument holder */
	char shortarg[2];

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
			for(i=0; i<protocols.nr; ++i) {
				device = protocols.listeners[i];
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
							} else if(long_options[long_opt_nr].has_arg == 2) {
								optstr[y++]=':';
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
			printf("\t -r --repeat=repeat\t\tnumber of times the command is send\n");
		}
		if(protohelp == 1 && match == 1 && device->printHelp != NULL) {
			printf("\n\t[%s]\n",device->id);
			device->printHelp();
		} else {
			printf("\nThe supported protocols are:\n");
			for(i=0; i<protocols.nr; ++i) {
				device = protocols.listeners[i];
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
		if(strchr(optstr,c)) {
			node = malloc(sizeof(struct options_t));
			node->id = c;
			node->value = optarg;
			node->next = options;
			options = node;
		} else {
			exit(EXIT_FAILURE);
		}
	}

	memset(recvBuff, '0',sizeof(recvBuff));

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Error : Could not create socket \n");
        return 1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       perror("connect");
       exit(EXIT_FAILURE);
    }

	while(1) {
		bzero(recvBuff,BUFFER_SIZE);
		if((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) < 1) {
			//perror("read");
			goto close;
		}

		if(n > 0) {
			recvBuff[n]='\0';
			if(connected == 0) {
				if(strcmp(recvBuff,"ACCEPT CONNECTION\n") == 0) {
					strcpy(message,"CLIENT SENDER\n");
					if((n = write(sockfd, message, strlen(message))) < 0) {
						perror("write");
						goto close;
					}
				}
				if(strcmp(recvBuff,"ACCEPT CLIENT\n") == 0) {
					connected=1;
				}
				if(strcmp(recvBuff,"REJECT CLIENT\n") == 0) {
					goto close;
				}
			}
		}
		if(connected) {
			memset(message,'0',BUFFER_SIZE);
			int x=0;
			while(node->id != 0) {
				x+=sprintf(message+x,"%d ",node->id);
				if(node->value != NULL) {
					x+=sprintf(message+x,"%s",node->value);
				} else {
					x+=sprintf(message+x,"1");
				}
				node = node->next;
				if(node->id != 0)
					x+=sprintf(message+x," ");
				else
					x+=sprintf(message+x,"\n");					
			}
			if((n = write(sockfd, message, BUFFER_SIZE-1)) < 0) {
				perror("write");
				goto close;
			}			
			memset(message,'0',BUFFER_SIZE);
			strcpy(message,"SEND\n");
			if((n = write(sockfd, message, BUFFER_SIZE-1)) < 0) {
				perror("write");
				goto close;
			}
		goto close;
		}
	}
close:
	shutdown(sockfd, SHUT_WR);
	close(sockfd);
return 0;
}
