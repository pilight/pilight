/*
	Copyright (C) 2013 CurlyMo

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
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

#include "settings.h"
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

	log_file_disable();
	log_shell_enable();
	log_level_set(LOG_NOTICE);

	progname = strdup("pilight-control");

	struct options_t *options = NULL;

	int sockfd = 0;
    char *recvBuff = NULL;
	char *message = NULL;
	char *pch = NULL;
	steps_t steps = WELCOME;

	char device[50];
	char location[50];
	char state[10] = {'\0'};
	char values[255] = {'\0'};
	struct conf_locations_t *slocation = NULL;
	struct conf_devices_t *sdevice = NULL;
	int has_values = 0;
	
	char *server = strdup("127.0.0.1");
	unsigned short port = PORT;
	
	JsonNode *json = json_mkobject();
	JsonNode *jconfig = json_mkobject();
	JsonNode *jcode = json_mkobject();
	JsonNode *jvalues = json_mkobject();

	/* Define all CLI arguments of this program */
	options_add(&options, 'H', "help", no_value, 0, NULL);
	options_add(&options, 'V', "version", no_value, 0, NULL);
	options_add(&options, 'l', "location", has_value, 0, NULL);
	options_add(&options, 'd', "device", has_value, 0,  NULL);
	options_add(&options, 's', "state", has_value, 0,  NULL);
	options_add(&options, 'v', "values", has_value, 0,  NULL);
	options_add(&options, 'S', "server", has_value, 0, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", has_value, 0, "[0-9]{1,4}");

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &optarg);
		if(c == -1)
			break;
		switch(c) {
			case 'H':
				printf("\t -H --help\t\t\tdisplay this message\n");
				printf("\t -V --version\t\t\tdisplay version\n");
				printf("\t -S --server=%s\t\tconnect to server address\n", server);
				printf("\t -P --port=%d\t\t\tconnect to server port\n", port);
				printf("\t -l --location=location\t\tthe location in which the device resides\n");
				printf("\t -d --device=device\t\tthe device that you want to control\n");
				printf("\t -s --state=state\t\tthe new state of the device\n");
				printf("\t -v --values=values\t\tspecific comma separated values, e.g.:\n");
				printf("\t\t\t\t\t-v dimlevel=10\n");
				exit(EXIT_SUCCESS);
			break;
			case 'V':
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
			case 'v':
				strcpy(values, optarg);
			break;			
			case 'S':
				free(server);
				server = strdup(optarg);
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

	if((sockfd = socket_connect(server, port)) == -1) {
		logprintf(LOG_ERR, "could not connect to pilight-daemon");
		exit(EXIT_FAILURE);
	}

	/* Initialize peripheral modules */
	hw_init();

	while(1) {
		if(steps > WELCOME) {
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
		}
		switch(steps) {
			case WELCOME:
				socket_write(sockfd, "{\"message\":\"client controller\"}");
				steps=IDENTIFY;
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
				if((jconfig = json_find_member(json, "config")) != NULL) {
					config_parse(jconfig);
					if(config_get_location(location, &slocation) == 0) {
						if(config_get_device(location, device, &sdevice) == 0) {
							json_delete(json);

							json = json_mkobject();
							jcode = json_mkobject();
							jvalues = json_mkobject();

							json_append_member(jcode, "location", json_mkstring(location));
							json_append_member(jcode, "device", json_mkstring(device));

							pch = strtok(values, ",=");
							while(pch != NULL) {
								char *name = strdup(pch);
								pch = strtok(NULL, ",=");
								if(pch == NULL) {
									break;
								} else {
									char *val = strdup(pch);
									if(pch != NULL) {
										if(config_valid_value(location, device, name, val) == 0) {
											json_append_member(jvalues, name, json_mkstring(val));
											has_values = 1;
										} else {
											logprintf(LOG_ERR, "\"%s\" is an invalid value for device \"%s\"", name, device);
											goto close;
										}
									} else {
										logprintf(LOG_ERR, "\"%s\" is an invalid value for device \"%s\"", name, device);
										goto close;
									}
									pch = strtok(NULL, ",=");
									if(pch == NULL) {
										break;
									}
								}
							}							
							
							if(strlen(state) > 0) {
								if(config_valid_state(location, device, state) == 0) {
									json_append_member(jcode, "state", json_mkstring(state));
								} else {
									logprintf(LOG_ERR, "\"%s\" is an invalid state for device \"%s\"", state, device);
									goto close;
								}
							}
							
							if(has_values == 1) {
								json_append_member(jcode, "values", jvalues);
							}
							
							json_append_member(json, "message", json_mkstring("send"));
							json_append_member(json, "code", jcode);

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
			break;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
close:
	json_delete(json);
	socket_close(sockfd);
free(server);
return EXIT_SUCCESS;
}
