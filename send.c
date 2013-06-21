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
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <syslog.h>
#include "config.h"
#include "log.h"
#include "options.h"

#include "protocol.h"
#include "protocols/kaku_switch.h"
#include "protocols/kaku_dimmer.h"
#include "protocols/kaku_old.h"
#include "protocols/elro.h"
#include "protocols/raw.h"

int main(int argc, char **argv) {

	disable_file_log();
	enable_shell_log();
	set_loglevel(LOG_INFO); 

	progname = malloc((10*sizeof(char))+1);
	progname = "433-send";
	
	options = malloc(255*sizeof(struct options_t));
	
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
	
	/* Define all CLI arguments of this program */
	addOption(&options, 'h', "help", no_argument, 0, NULL);
	addOption(&options, 'v', "version", no_argument, 0, NULL);
	addOption(&options, 'p', "protocol", required_argument, 0, NULL);
	addOption(&options, 'r', "repeat", required_argument, 0, "[0-9]");

	/* Initialize peripheral modules */
	kakuSwInit();
	kakuDimInit();
	kakuOldInit();
	elroInit();
	rawInit();
	
	/* Get the protocol to be used */
	while (1) {
		int c;
		c = getOptions(&options, argc, argv, 0);
		if (c == -1)
			break;
		switch(c) {
			case 'p':
				if(strlen(optarg) == 0) {
					logprintf(LOG_ERR, "options '-p' and '--protocol' require an argument");
					exit(EXIT_FAILURE);
				} else {
					strcpy(protobuffer, optarg);
				}
			break;
			case 'v':
				version = 1;
			break;
			case 'h':
				help = 1;
			break;
		}
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
					/* Check if the protocol requires specific CLI arguments
					   and merge them with the main CLI arguments */
					if(device->options != NULL && help == 0) {
						mergeOptions(&options, &device->options);
					} else if(help == 1) {
						protohelp=1;
					}
					break;
				}
			}
			/* If no protocols matches the requested protocol */
			if(!match) {
				logprintf(LOG_ERR, "this protocol is not supported");
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

	/* Store all CLI arguments for later usage 
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = getOptions(&options, argc, argv, 1);
		if(c == -1)
			break;
	}

	/* Check if we got sufficient arguments from this protocol */
	device->createCode(options);

	/* Clear the receive buffer */
	memset(recvBuff, '\0',sizeof(recvBuff));

	/* Try to open a new socket */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        logprintf(LOG_ERR, "could not create socket");
		return EXIT_FAILURE;
    }

	/* Clear the server address */
    memset(&serv_addr, '\0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

	/* Connect to the 433-daemon */
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		logprintf(LOG_ERR, "could not connect to 433-daemon");
		return EXIT_FAILURE;
    }

	while(1) {
		/* Clear the receive buffer again and read the welcome message */
		bzero(recvBuff,BUFFER_SIZE);
		if((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) < 1) {
			logprintf(LOG_ERR, "could not read from socket");
			goto close;
		}

		/* If we have received, identify ourselves */
		if(n > 0) {
			recvBuff[n]='\0';
			if(connected == 0) {
				if(strcmp(recvBuff,"ACCEPT CONNECTION\n") == 0) {
					strcpy(message,"CLIENT SENDER\n");
					if((n = write(sockfd, message, strlen(message))) < 0) {
						logprintf(LOG_ERR, "could not write to socket");
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
		/* If we have a valid connection send the arguments for this protocol */
		if(connected) {
			memset(message,'\0',BUFFER_SIZE);
			int x=0;
			while(options != NULL && strlen(options->name) > 0) {
				/* Only send the CLI arguments that belong to this protocol, the protocol name 
				   and those that are called by the user */
				if((getOptionIdByName(&device->options, options->name) != -1 || strcmp(options->name,"protocol") == 0) 
				   && strlen(options->value) > 0) {
					x+=sprintf(message+x,"%d %s ",options->id, options->value);
				}
				options = options->next;
			}
			/* Remove the last space and replace it with a newline */
			sprintf(message+(x-1),"\n");	

			if((n = write(sockfd, message, BUFFER_SIZE-1)) < 0) {
				logprintf(LOG_ERR, "could not write to socket");
				goto close;
			}
			memset(message,'\0',BUFFER_SIZE);
			strcpy(message,"SEND\n");
			if((n = write(sockfd, message, BUFFER_SIZE-1)) < 0) {
				logprintf(LOG_ERR, "could not write to socket");
				goto close;
			}
		goto close;
		}
	}
close:
	shutdown(sockfd, SHUT_WR);
	close(sockfd);
return EXIT_SUCCESS;
}
