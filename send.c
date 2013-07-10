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
#include <syslog.h>

#include "config.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "json.h"

#include "hardware.h"

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	SEND
} steps_t;

int main(int argc, char **argv) {

	disable_file_log();
	enable_shell_log();
	set_loglevel(LOG_NOTICE);

	progname = malloc((10*sizeof(char))+1);
	progname = strdup("433-send");

	options = malloc(255*sizeof(struct options_t));

	int sockfd = 0;
    char *recvBuff = NULL;
    char *message;
	steps_t steps = WELCOME;

	/* Hold the name of the protocol */
	char protobuffer[25] = "\0";
	int i;
	/* Does this protocol exists */
	int match = 0;

	/* Do we need to print the help */
	int help = 0;
	/* Do we need to print the version */
	int version = 0;
	/* Do we need to print the protocol help */
	int protohelp = 0;

	char server[16] = "127.0.0.1";
	unsigned short port = PORT;
	
	/* Hold the final protocol struct */
	protocol_t *protocol = NULL;

	JsonNode *json = json_mkobject();
	JsonNode *code = json_mkobject();

	/* Define all CLI arguments of this program */
	addOption(&options, 'h', "help", no_value, 0, NULL);
	addOption(&options, 'v', "version", no_value, 0, NULL);
	addOption(&options, 'p', "protocol", has_value, 0, NULL);
	addOption(&options, 'r', "repeat", has_value, 0, "[0-9]");
	addOption(&options, 'S', "server", has_value, 0, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	addOption(&options, 'P', "port", has_value, 0, "[0-9]{1,4}");

	/* Initialize peripheral modules */
	hw_init();

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
			case 'S':
				strcpy(server, optarg);
			break;
			case 'P':
				port = (unsigned short)atoi(optarg);
			break;
			default:;
		}
	}

	/* Check if a protocol was given */
	if(strlen(protobuffer) > 0 && strcmp(protobuffer,"-v") != 0) {
		if(strlen(protobuffer) > 0 && version) {
			printf("-p and -v cannot be combined\n");
		} else {
			for(i=0; i<protocols.nr; ++i) {
				protocol = protocols.listeners[i];
				/* Check if the protocol exists */
				if(providesDevice(&protocol, protobuffer) == 0 && match == 0 && protocol->createCode != NULL) {
					match=1;
					/* Check if the protocol requires specific CLI arguments
					   and merge them with the main CLI arguments */
					if(protocol->options != NULL && help == 0) {
						mergeOptions(&options, &protocol->options);
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
		if(protohelp == 1 && match == 1 && protocol->printHelp != NULL)
			printf("Usage: %s -p %s [options]\n", progname, protobuffer);
		else
			printf("Usage: %s -p protocol [options]\n", progname);
		if(help == 1) {
			printf("\t -h --help\t\t\tdisplay this message\n");
			printf("\t -v --version\t\t\tdisplay version\n");
			printf("\t -S --server=%s\t\tconnect to server address\n", server);
			printf("\t -P --port=%d\t\t\tconnect to server port\n", port);
			printf("\t -p --protocol=protocol\t\tthe protocol that you want to control\n");
			printf("\t -r --repeat=repeat\t\tnumber of times the command is send\n");
		}
		if(protohelp == 1 && match == 1 && protocol->printHelp != NULL) {
			printf("\n\t[%s]\n", protobuffer);
			protocol->printHelp();
		} else {
			printf("\nThe supported protocols are:\n");
			for(i=0; i<protocols.nr; ++i) {
				protocol = protocols.listeners[i];
				if(protocol->createCode != NULL) {
					while(protocol->devices != NULL) {
						printf("\t %s\t\t\t",protocol->devices->id);
						if(strlen(protocol->devices->id)<5)
							printf("\t");
						printf("%s\n", protocol->devices->desc);
						protocol->devices = protocol->devices->next;
					}
				}
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

	int itmp;
	/* Check if we got sufficient arguments from this protocol */
	while(options != NULL && strlen(options->name) > 0) {
		/* Only send the CLI arguments that belong to this protocol, the protocol name
		   and those that are called by the user */
		if((getOptionIdByName(&protocol->options, options->name, &itmp) == 0 || strcmp(options->name, "protocol") == 0 || strcmp(options->name, "repeat") == 0)
		   && strlen(options->value) > 0) {
			json_append_member(code, options->name, json_mkstring(options->value));
		}
		options = options->next;
	}

	if(protocol->createCode(code) == 0) {

		if((sockfd = connect_to_server(strdup(server), port)) == -1) {
			logprintf(LOG_ERR, "could not connect to 433-daemon");
			goto close;
		}

		while(1) {
			/* Clear the receive buffer again and read the welcome message */
			if((recvBuff = socket_read(sockfd)) != NULL) {
				json = json_decode(recvBuff);
				json_find_string(json, "message", &message);
			} else {
				goto close;
			}
			usleep(100);
			switch(steps) {
				case WELCOME:
					if(strcmp(message, "accept connection") == 0) {
						socket_write(sockfd, "{\"message\":\"client sender\"}");
						steps=IDENTIFY;
					}
				case IDENTIFY:
					if(strcmp(message, "accept client") == 0) {
						steps=SEND;
					}
					if(strcmp(message, "reject client") == 0) {
						steps=REJECT;
					}
				case SEND:
					json_delete(json);
					json = json_mkobject();
					json_append_member(json, "message", json_mkstring("send"));
					json_append_member(json, "code", code);
					socket_write(sockfd, json_stringify(json, NULL));
					goto close;
				break;
				case REJECT:
				default:
					goto close;
				break;
			}
		}
	}
close:
	json_delete(json);
	socket_close(sockfd);
return EXIT_SUCCESS;
}
