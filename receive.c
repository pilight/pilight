/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include "pilight.h"
#include "common.h"
#include "settings.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "ssdp.h"

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	RECEIVE
} steps_t;

int main(int argc, char **argv) {

	log_shell_enable();
	log_file_disable();

	log_level_set(LOG_NOTICE);

	progname = malloc(16);
	if(!progname) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-receive");
	struct options_t *options = NULL;
	struct ssdp_list_t *ssdp_list = NULL;

	JsonNode *json = NULL;

	char *server = NULL;
	unsigned short port = 0;

    int sockfd = 0;
    char *recvBuff = NULL;
	char *message = NULL;
	char *args = NULL;
	steps_t steps = WELCOME;

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL);
	options_add(&options, 'S', "server", OPTION_HAS_VALUE, 0, JSON_NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", OPTION_HAS_VALUE, 0, JSON_NULL, "[0-9]{1,4}");

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &args);
		if(c == -1)
			break;
		if(c == -2)
			c = 'H';
		switch(c) {
			case 'H':
				printf("\t -H --help\t\t\tdisplay this message\n");
				printf("\t -V --version\t\t\tdisplay version\n");
				printf("\t -S --server=x.x.x.x\t\tconnect to server address\n");
				printf("\t -P --port=xxxx\t\t\tconnect to server port\n");
				exit(EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s %s\n", progname, VERSION);
				exit(EXIT_SUCCESS);
			break;
			case 'S':
				server = realloc(server, strlen(args)+1);
				memset(server, '\0', strlen(args)+1);
				if(!server) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(server, args);
			break;
			case 'P':
				port = (unsigned short)atoi(args);
			break;
			default:
				printf("Usage: %s -l location -d device\n", progname);
				exit(EXIT_SUCCESS);
			break;
		}
	}
	options_delete(options);

	if(server && port > 0) {
		if((sockfd = socket_connect(server, port)) == -1) {
			logprintf(LOG_ERR, "could not connect to pilight-daemon");
			return EXIT_FAILURE;
		}
	} else if(ssdp_seek(&ssdp_list) == -1) {
		logprintf(LOG_ERR, "no pilight ssdp connections found");
		goto close;
	} else {
		if((sockfd = socket_connect(ssdp_list->ip, ssdp_list->port)) == -1) {
			logprintf(LOG_ERR, "could not connect to pilight-daemon");
			goto close;
		}
	}
	if(ssdp_list) {
		ssdp_free(ssdp_list);
	}
	if(server) {
		sfree((void *)&server);
	}

	while(1) {
		if(steps > WELCOME) {
			/* Clear the receive buffer again and read the welcome message */
			if((recvBuff = socket_read(sockfd)) == NULL) {
				goto close;
			}
		}
		switch(steps) {
			case WELCOME:
				socket_write(sockfd, "{\"message\":\"client receiver\"}");
				steps=IDENTIFY;
			break;
			case IDENTIFY:
				//extract the message
				json = json_decode(recvBuff);
				json_find_string(json, "message", &message);
				if(strcmp(message, "accept client") == 0) {
					steps=RECEIVE;
				} else if(strcmp(message, "reject client") == 0) {
					steps=REJECT;
				}
				//cleanup
				json_delete(json);
				sfree((void *)&recvBuff);
				json = NULL;
				message = NULL;
				recvBuff = NULL;
			break;
			case RECEIVE: {
				char *line = strtok(recvBuff, "\n");
				//for each line
				while(line) {
					json = json_decode(line);
					char *output = json_stringify(json, "\t");
					printf("%s\n", output);
					sfree((void *)&output);
					json_delete(json);
					line = strtok(NULL,"\n");
				}
				sfree((void *)&recvBuff);
				sfree((void *)&line);
				recvBuff = NULL;
			} break;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
close:
	if(sockfd > 0) {
		socket_close(sockfd);
	}
	if(recvBuff) {
		sfree((void *)&recvBuff);
	}
	options_gc();
	log_shell_disable();
	log_gc();
	sfree((void *)&progname);
return EXIT_SUCCESS;
}
