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
#include <stdlib.h>
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
	REQUEST,
	CONFIG
} steps_t;

int main(int argc, char **argv) {

	disable_file_log();
	enable_shell_log();
	set_loglevel(LOG_NOTICE);

	progname = malloc((10*sizeof(char))+1);
	progname = strdup("433-control");

	options = malloc(255*sizeof(struct options_t));

	int sockfd = 0;
    char *recvBuff = NULL;
	char *message;
	steps_t steps = WELCOME;

	char device[50];
	char location[50];
	char state[10];
	struct conf_locations_t *slocation = NULL;
	struct conf_devices_t *sdevice = NULL;
	
	char server[16] = "127.0.0.1";
	unsigned short port = PORT;
	
	JsonNode *json = json_mkobject();
	JsonNode *config = json_mkobject();
	JsonNode *code = json_mkobject();

	/* Define all CLI arguments of this program */
	addOption(&options, 'h', "help", no_value, 0, NULL);
	addOption(&options, 'v', "version", no_value, 0, NULL);
	addOption(&options, 'l', "location", has_value, 0, NULL);
	addOption(&options, 'd', "device", has_value, 0,  NULL);
	addOption(&options, 's', "state", has_value, 0,  NULL);
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
				printf("\t -h --help\t\t\tdisplay this message\n");
				printf("\t -v --version\t\t\tdisplay version\n");
				printf("\t -S --server=%s\t\tconnect to server address\n", server);
				printf("\t -P --port=%d\t\t\tconnect to server port\n", port);
				printf("\t -l --location=location\t\tthe location in which the device resides\n");
				printf("\t -d --device=device\t\tthe device that you want to control\n");
				printf("\t -s --state=state\t\tthe new state of the device\n");
				exit(EXIT_SUCCESS);
			break;
			case 'v':
				printf("%s %s\n", progname, "1.0");
				exit(EXIT_SUCCESS);
			break;
			case 'l':
				strcpy(location, optarg);
			break;
			case 'd':
				strcpy(device, optarg);
			break;
			case 's':
				strcpy(state, optarg);
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

	if(strlen(location) == 0 || strlen(device) == 0) {
		printf("Usage: %s -l location -d device\n", progname);
		exit(EXIT_SUCCESS);
	}

	if((sockfd = connect_to_server(strdup(server), port)) == -1) {
		logprintf(LOG_ERR, "could not connect to 433-daemon");
		exit(EXIT_FAILURE);
	}

	/* Initialize peripheral modules */
	hw_init();

	while(1) {
		/* Clear the receive buffer again and read the welcome message */
		if(steps == REQUEST) {
			if((recvBuff = socket_read_big(sockfd)) != NULL) {
				json = json_decode(recvBuff);
				json_find_string(json, "message", &message);
			} else {
				goto close;
			}
		} else {
			if((recvBuff = socket_read(sockfd)) != NULL) {
				json = json_decode(recvBuff);
				json_find_string(json, "message", &message);
			} else {
				goto close;
			}
		}
		usleep(100);
		switch(steps) {
			case WELCOME:
				if(strcmp(message, "accept connection") == 0) {
					socket_write(sockfd, "{\"message\":\"client controller\"}");
					steps=IDENTIFY;
				}
			break;
			case IDENTIFY:
				if(strcmp(message, "accept client") == 0) {
					steps=REQUEST;
				}
				if(strcmp(message, "reject client") == 0) {
					steps=REJECT;
				}
			case REQUEST:
				socket_write(sockfd, "{\"message\":\"request config\"}");
				steps=CONFIG;
			break;
			case CONFIG:
				if((config = json_find_member(json, "config")) != NULL) {
					config_parse(config);
					if(config_get_location(location, &slocation) == 0) {
						if(config_get_device(location, device, &sdevice) == 0) {
							json_delete(json);

							json = json_mkobject();
							code = json_mkobject();

							json_append_member(code, "location", json_mkstring(location));
							json_append_member(code, "device", json_mkstring(device));
							if(strlen(state) > 0) {
								if(config_valid_state(location, device, state) == 0) {
									json_append_member(code, "state", json_mkstring(state));
								} else {
									logprintf(LOG_ERR, "\"%s\" is an invalid state for device \"%s\"", state, device);
									goto close;
								}
							}
							
							json_append_member(json, "message", json_mkstring("send"));
							json_append_member(json, "code", code);

							socket_write(sockfd, json_stringify(json, NULL));
						} else {
							logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
							goto close;
						}
					} else {
						logprintf(LOG_ERR, "the location \"%s\" does not exist", location);
						goto close;
					}
				}
				goto close;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
close:
	json_delete(json);
	socket_close(sockfd);
return EXIT_SUCCESS;
}
