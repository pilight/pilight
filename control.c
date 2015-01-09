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

#include "pilight.h"
#include "common.h"
#include "config.h"
#include "devices.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "json.h"
#include "ssdp.h"
#include "dso.h"
#include "protocol.h"

int main(int argc, char **argv) {
	struct options_t *options = NULL;
	struct ssdp_list_t *ssdp_list = NULL;
	struct devices_t *dev = NULL;
	struct JsonNode *json = NULL;
	char *recvBuff = NULL, *message = NULL, *output = NULL;
	char *device = NULL, *state = NULL, *values = NULL;
	char *server = NULL;
	int has_values = 0, sockfd = 0;
	unsigned short port = 0;

	log_file_disable();
	log_shell_enable();
	log_level_set(LOG_NOTICE);

	if(!(progname = malloc(16))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-control");

	/* Define all CLI arguments of this program */
	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'd', "device", OPTION_HAS_VALUE, 0,  JSON_NULL, NULL, NULL);
	options_add(&options, 's', "state", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'v', "values", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'S', "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, 'C', "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

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
				printf("\t -C --config\t\t\tconfig file\n");
				printf("\t -P --port=xxxx\t\t\tconnect to server port\n");
				printf("\t -d --device=device\t\tthe device that you want to control\n");
				printf("\t -s --state=state\t\tthe new state of the device\n");
				printf("\t -v --values=values\t\tspecific comma separated values, e.g.:\n");
				printf("\t\t\t\t\t-v dimlevel=10\n");
				goto close;
			break;
			case 'V':
				printf("%s %s\n", progname, VERSION);
				goto close;
			break;
			case 'd':
				if((device = realloc(device, strlen(optarg)+1)) == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(device, optarg);
			break;
			case 's':
				if((state = realloc(state, strlen(optarg)+1)) == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(state, optarg);
			break;
			case 'v':
				if((values = realloc(values, strlen(optarg)+1)) == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(values, optarg);
			break;
			case 'C':
				if(config_set_file(optarg) == EXIT_FAILURE) {
					return EXIT_FAILURE;
				}
			break;
			case 'S':
				if(!(server = realloc(server, strlen(optarg)+1))) {
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
				goto close;
			break;
		}
	}
	options_delete(options);

	if(device == NULL || state == NULL ||
	   strlen(device) == 0 || strlen(state) == 0) {
		printf("Usage: %s -d device -s state\n", progname);
		goto close;
	}

	if(server && port > 0) {
		if((sockfd = socket_connect(server, port)) == -1) {
			logprintf(LOG_ERR, "could not connect to pilight-daemon");
			goto close;
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
	config_init();

	if(config_read() != EXIT_SUCCESS) {
		goto close;
	}

	socket_write(sockfd, "{\"action\":\"identify\"}");
	if(socket_read(sockfd, &recvBuff) != 0
	   || strcmp(recvBuff, "{\"status\":\"success\"}") != 0) {
		goto close;
	}

	json = json_mkobject();
	json_append_member(json, "action", json_mkstring("request config"));
	output = json_stringify(json, NULL);
	socket_write(sockfd, output);
	sfree((void *)&output);
	json_delete(json);

	if(socket_read(sockfd, &recvBuff) == 0) {
		if(json_validate(recvBuff) == true) {
			json = json_decode(recvBuff);
			if(json_find_string(json, "message", &message) == 0) {
				if(strcmp(message, "config") == 0) {
					struct JsonNode *jconfig = NULL;
					if((jconfig = json_find_member(json, "config")) != NULL) {
						config_parse(jconfig);
						if(devices_get(device, &dev) == 0) {
							JsonNode *joutput = json_mkobject();
							JsonNode *jcode = json_mkobject();
							JsonNode *jvalues = json_mkobject();
							json_append_member(jcode, "device", json_mkstring(device));

							if(values != NULL) {
								char *pch = strtok(values, ",=");
								while(pch != NULL) {
									char *name = strdup(pch);
									pch = strtok(NULL, ",=");
									if(pch == NULL) {
										sfree((void *)&name);
										break;
									} else {
										char *val = strdup(pch);
										if(pch != NULL) {
											if(devices_valid_value(device, name, val) == 0) {
												if(isNumeric(val) == EXIT_SUCCESS) {
													char *ptr = strstr(pch, ".");
													int decimals = 0;
													if(ptr != NULL) {
														decimals = (int)(strlen(pch)-((size_t)(ptr-pch)+1));
													}
													json_append_member(jvalues, name, json_mknumber(atof(val), decimals));
												} else {
													json_append_member(jvalues, name, json_mkstring(val));
												}
												has_values = 1;
											} else {
												logprintf(LOG_ERR, "\"%s\" is an invalid value for device \"%s\"", name, device);
												sfree((void *)&name);
												json_delete(json);
												goto close;
											}
										} else {
											logprintf(LOG_ERR, "\"%s\" is an invalid value for device \"%s\"", name, device);
											json_delete(json);
											goto close;
										}
										pch = strtok(NULL, ",=");
										if(pch == NULL) {
											sfree((void *)&name);
											break;
										}
									}
									sfree((void *)&name);
								}
							}

							if(devices_valid_state(device, state) == 0) {
								json_append_member(jcode, "state", json_mkstring(state));
							} else {
								logprintf(LOG_ERR, "\"%s\" is an invalid state for device \"%s\"", state, device);
								json_delete(json);
								goto close;
							}

							if(has_values == 1) {
								json_append_member(jcode, "values", jvalues);
							} else {
								json_delete(jvalues);
							}
							json_append_member(joutput, "action", json_mkstring("control"));
							json_append_member(joutput, "code", jcode);
							output = json_stringify(joutput, NULL);
							socket_write(sockfd, output);
							sfree((void *)&output);
							json_delete(joutput);
							if(socket_read(sockfd, &recvBuff) != 0
							   || strcmp(recvBuff, "{\"status\":\"success\"}") != 0) {
								logprintf(LOG_ERR, "failed to control %s", device);
							}
						} else {
							logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
							json_delete(json);
							goto close;
						}
					}
				}
			}
			json_delete(json);
		}
	}
close:
	if(sockfd > 0) {
		socket_close(sockfd);
	}
	if(server) {
		sfree((void *)&server);
	}
	if(device) {
		sfree((void *)&device);
	}
	if(state) {
		sfree((void *)&state);
	}
	if(values) {
		sfree((void *)&values);
	}
	log_shell_disable();
	protocol_gc();
	socket_gc();
	options_gc();
	config_gc();
	dso_gc();
	log_gc();
	sfree((void *)&progname);

	return EXIT_SUCCESS;
}
