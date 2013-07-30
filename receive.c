/*
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include "settings.h"
#include "log.h"
#include "options.h"
#include "socket.h"

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	RECEIVE
} steps_t;

int main(int argc, char **argv) {
	enable_shell_log();
	disable_file_log();

	set_loglevel(LOG_NOTICE);

	progname = malloc((10*sizeof(char))+1);
	progname = strdup("433-receive");

	JsonNode *json = json_mkobject();

	char server[16] = "127.0.0.1";
	unsigned short port = PORT;	
	
    int sockfd = 0;
    char *recvBuff;
	char *message = NULL;
	steps_t steps = WELCOME;

	addOption(&options, 'H', "help", no_value, 0, NULL);
	addOption(&options, 'V', "version", no_value, 0, NULL);	
	addOption(&options, 'S', "server", has_value, 0, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	addOption(&options, 'P', "port", has_value, 0, "[0-9]{1,4}");	
	
	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = getOptions(&options, argc, argv, 1);
		if(c == -1)
			break;
		switch(c) {
			case 'h':
				printf("\t -H --help\t\t\tdisplay this message\n");
				printf("\t -V --version\t\t\tdisplay version\n");				
				printf("\t -S --server=%s\t\tconnect to server address\n", server);
				printf("\t -P --port=%d\t\t\tconnect to server port\n", port);				
				exit(EXIT_SUCCESS);
			break;
			case 'v':
				printf("%s %s\n", progname, "1.0");
				exit(EXIT_SUCCESS);
			break;
			case 'S':
				strcpy(server, optarg);
			break;
			case 'P':
				port = (unsigned short)atoi(optarg);
			break;			
			default:
				printf("Usage: %s -l location -d device\n", progname);
				exit(EXIT_SUCCESS);
			break;
		}
	}	
	
    if((sockfd = connect_to_server(strdup(server), port)) == -1) {
		logprintf(LOG_ERR, "could not connect to 433-daemon");
		return EXIT_FAILURE;
	}

	while(1) {
		/* Clear the receive buffer again and read the welcome message */
		if((recvBuff = socket_read(sockfd)) != NULL) {

			json = json_decode(recvBuff);
			json_find_string(json, "message", &message);
		} else {
			goto close;
		}

		switch(steps) {
			case WELCOME:
				if(strcmp(message, "accept connection") == 0) {
					socket_write(sockfd, "{\"message\":\"client receiver\"}");
					steps=IDENTIFY;
				}
			break;
			case IDENTIFY:
				if(strcmp(message, "accept client") == 0) {
					steps=RECEIVE;
				}
				if(strcmp(message, "reject client") == 0) {
					steps=REJECT;
				}
			break;
			case RECEIVE:
				printf("%s\n", json_stringify(json, "\t"));
			break;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
close:
	socket_close(sockfd);
return EXIT_SUCCESS;
}
