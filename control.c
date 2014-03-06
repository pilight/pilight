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
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

#include "pilight.h"
#include "common.h"
#include "settings.h"
#include "config.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "json.h"
#include "ssdp.h"

#include "protocol.h"

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

	if((progname = malloc(16)) == NULL) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-control");

	struct options_t *options = NULL;
	struct ssdp_list_t *ssdp_list = NULL;

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

	char *server = NULL;
	unsigned short port = 0;

	JsonNode *json = NULL;
	JsonNode *jconfig = NULL;
	JsonNode *jcode = NULL;
	JsonNode *jvalues = NULL;

	/* Define all CLI arguments of this program */
	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'l', "location", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'd', "device", OPTION_HAS_VALUE, 0,  JSON_NULL, NULL, NULL);
	options_add(&options, 's', "state", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'v', "values", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'S', "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &optarg);
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
				printf("\t -l --location=location\t\tthe location in which the device resides\n");
				printf("\t -d --device=device\t\tthe device that you want to control\n");
				printf("\t -s --state=state\t\tthe new state of the device\n");
				printf("\t -v --values=values\t\tspecific comma separated values, e.g.:\n");
				printf("\t\t\t\t\t-v dimlevel=10\n");
				exit(EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s %s\n", progname, VERSION);
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
				server = realloc(server, strlen(optarg)+1);
				if(!server) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(server, optarg);
			break;
			case 'P':
				port = (unsigned short)atoi(optarg);
			break;
			default:
				printf("Usage: %s -l location -d device -s state\n", progname);
				exit(EXIT_SUCCESS);
			break;
		}
	}
	options_delete(options);

	if(strlen(location) == 0 || strlen(device) == 0 || strlen(state) == 0) {
		printf("Usage: %s -l location -d device -s state\n", progname);
		exit(EXIT_SUCCESS);
	}

	if(server && port > 0) {
		if((sockfd = socket_connect(server, port)) == -1) {
			logprintf(LOG_ERR, "could not connect to pilight-daemon");
			exit(EXIT_FAILURE);
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

	protocol_init();

	while(1) {
		if(steps > WELCOME) {
			/* Clear the receive buffer again and read the welcome message */
			if((recvBuff = socket_read(sockfd))) {
				json = json_decode(recvBuff);
				json_find_string(json, "message", &message);
				sfree((void *)&recvBuff);
			} else {
				goto close;
			}
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
				json_delete(json);
			break;
			case CONFIG:
				if((jconfig = json_find_member(json, "config")) != NULL) {
					config_parse(jconfig);
					if(config_get_location(location, &slocation) == 0) {
						if(config_get_device(location, device, &sdevice) == 0) {
							JsonNode *joutput = json_mkobject();
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
											if(isNumeric(val) == EXIT_SUCCESS) {
												json_append_member(jvalues, name, json_mknumber(atoi(val)));
											} else {
												json_append_member(jvalues, name, json_mkstring(val));
											}
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

							if(config_valid_state(location, device, state) == 0) {
								json_append_member(jcode, "state", json_mkstring(state));
							} else {
								logprintf(LOG_ERR, "\"%s\" is an invalid state for device \"%s\"", state, device);
								goto close;
							}

							if(has_values == 1) {
								json_append_member(jcode, "values", jvalues);
							} else {
								json_delete(jvalues);
							}

							json_append_member(joutput, "message", json_mkstring("send"));
							json_append_member(joutput, "code", jcode);
							char *output = json_stringify(joutput, NULL);
							socket_write(sockfd, output);
							sfree((void *)&output);
							json_delete(joutput);
						} else {
							logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
							goto close;
						}
					} else {
						logprintf(LOG_ERR, "the location \"%s\" does not exist", location);
						goto close;
					}
				}
				json_delete(json);
				goto close;
			break;
			case REJECT:
			default:
				json_delete(json);
				goto close;
			break;
		}
	}
close:
	if(sockfd > 0) {
		socket_close(sockfd);
	}
	if(server) {
		sfree((void *)&server);
	}
	log_shell_disable();
	config_gc();
	protocol_gc();
	socket_gc();
	options_gc();
	log_gc();
	sfree((void *)&progname);

return EXIT_SUCCESS;
}
