/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "libs/pilight/core/threadpool.h"
#include "libs/pilight/core/eventpool.h"
#include "libs/pilight/core/timerpool.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/json.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/dso.h"
#include "libs/pilight/core/gc.h"

#include "libs/pilight/protocols/protocol.h"
#include "libs/pilight/storage/storage.h"
#include "libs/pilight/events/events.h"

#define IDENTIFY				0
#define STATUS					1
#define REQUESTCONFIG		2
#define RECEIVECONFIG		3
#define VALIDATE				4

static unsigned short connected = 0;
static unsigned short connecting = 0;
static unsigned short found = 0;
static char *instance = NULL;

static char *state = NULL, *values = NULL, *device = NULL;

typedef struct ssdp_list_t {
	char server[INET_ADDRSTRLEN+1];
	unsigned short port;
	char name[17];
	struct ssdp_list_t *next;
} ssdp_list_t;

static struct ssdp_list_t *ssdp_list = NULL;
static int ssdp_list_size = 0;

int main_gc(void) {
	log_shell_disable();

	struct ssdp_list_t *tmp1 = NULL;
	while(ssdp_list) {
		tmp1 = ssdp_list;
		ssdp_list = ssdp_list->next;
		FREE(tmp1);
	}

	if(state != NULL) {
		FREE(state);
		state = NULL;
	}

	if(device != NULL) {
		FREE(device);
		device = NULL;
	}

	if(values != NULL) {
		FREE(values);
		values = NULL;
	}

	if(instance != NULL) {
		FREE(instance);
		instance = NULL;
	}

	protocol_gc();
	storage_gc();

	timer_thread_gc();
	eventpool_gc();

	log_shell_disable();
	log_gc();

	if(progname != NULL) {
		FREE(progname);
		progname = NULL;
	}

	return 0;
}

void *timeout(void *param) {
	if(connected == 0) {
		signal(SIGALRM, SIG_IGN);
		logprintf(LOG_ERR, "could not connect to the pilight instance");

		kill(getpid(), SIGINT);
	}
	return NULL;
}

void *ssdp_not_found(void *param) {
	if(found == 0) {
		signal(SIGALRM, SIG_IGN);
		logprintf(LOG_ERR, "could not find pilight instance: %s", instance);

		kill(getpid(), SIGINT);
	}
	return NULL;
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	switch(event) {
		case EV_CONNECT_SUCCESS: {
			connected = 1;
			switch(node->steps) {
				case IDENTIFY:
					socket_write(node->fd, "{\"action\":\"identify\"}");
					node->steps = STATUS;
				break;
			}
			eventpool_fd_enable_write(node);
		} break;
		case EV_WRITE: {
			switch(node->steps) {
				case REQUESTCONFIG: {
					socket_write(node->fd, "{\"action\":\"request config\"}");
					node->steps = RECEIVECONFIG;
				} break;
			}
			eventpool_fd_enable_read(node);
		}
		break;
		case EV_READ: {
			switch(node->steps) {
				case STATUS: {
					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						if(strcmp(node->buffer, "{\"status\":\"success\"}") == 0) {
							node->steps = REQUESTCONFIG;
							eventpool_fd_enable_write(node);
						}
						FREE(node->buffer);
						node->len = 0;
					}
				} break;
				case RECEIVECONFIG: {
					char *message = NULL;
					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						int has_values = 0;
						if(json_validate(node->buffer) == true) {
							struct JsonNode *json = json_decode(node->buffer);
							FREE(node->buffer);
							node->len = 0;
							if(json_find_string(json, "message", &message) == 0) {
								if(strcmp(message, "config") == 0) {
									struct JsonNode *jconfig = NULL;
									if((jconfig = json_find_member(json, "config")) != NULL) {
										int match = 1;
										struct JsonNode *tmp = NULL;
										while(match) {
											struct JsonNode *jchilds = json_first_child(jconfig);
											match = 0;
											while(jchilds) {
												if(strcmp(jchilds->key, "devices") != 0) {
													json_remove_from_parent(jchilds);
													tmp = jchilds;
													match = 1;
												}
												jchilds = jchilds->next;
												if(tmp != NULL) {
													json_delete(tmp);
												}
												tmp = NULL;
											}
										}
										storage_gc();
										storage_import(jconfig);
										if(devices_select(ORIGIN_CONTROLLER, device, &tmp) == 0) {
											struct JsonNode *joutput = json_mkobject();
											struct JsonNode *jcode = json_mkobject();
											struct JsonNode *jvalues = json_mkobject();
											json_append_member(joutput, "code", jcode);
											json_append_member(jcode, "device", json_mkstring(device));

											if(values != NULL) {
												char **array = NULL;
												unsigned int n = 0, q = 0;
												if(strstr(values, ",") != NULL) {
													n = explode(values, ",=", &array);
												} else {
													n = explode(values, "=", &array);
												}
												for(q=0;q<n;q+=2) {
													char *name = MALLOC(strlen(array[q])+1);
													if(name == NULL) {
														OUT_OF_MEMORY
													}
													strcpy(name, array[q]);
													if(q+1 == n) {
														array_free(&array, n);
														logprintf(LOG_ERR, "\"%s\" is missing a value for device \"%s\"", name, device);
														FREE(name);
														break;
													} else {
														char *val = MALLOC(strlen(array[q+1])+1);
														if(val == NULL) {
															OUT_OF_MEMORY
														}
														strcpy(val, array[q+1]);
														struct JsonNode *jvalue = NULL;
														if((jvalue = json_find_member(tmp, name)) == NULL) {
															if(isNumeric(val) == 0) {
																json_append_member(tmp, name, json_mknumber(atof(val), nrDecimals(val)));
															} else {
																json_append_member(tmp, name, json_mkstring(val));
															}
														} else {
															if(jvalue->tag == JSON_NUMBER) {
																jvalue->number_ = atof(val);
																jvalue->decimals_ = nrDecimals(val);
															} else {
																if((jvalue->string_ = REALLOC(jvalue->string_, strlen(val)+1)) == NULL) {
																	OUT_OF_MEMORY
																}
																strcpy(jvalue->string_, val);
															}
														}
														if(devices_validate_settings(tmp, -1) == 0) {
															if(isNumeric(val) == EXIT_SUCCESS) {
																json_append_member(jvalues, name, json_mknumber(atof(val), nrDecimals(val)));
															} else {
																json_append_member(jvalues, name, json_mkstring(val));
															}
															has_values = 1;
														} else {
															logprintf(LOG_ERR, "\"%s\" is an invalid value for device \"%s\"", name, device);
															array_free(&array, n);
															FREE(name);
															json_delete(json);
															json_delete(joutput);
															json_delete(jvalues);
															return -1;
														}
													}
													FREE(name);
												}
												array_free(&array, n);
											}

											struct JsonNode *jstate = json_find_member(tmp, "state");
											if(jstate != NULL && jstate->tag == JSON_STRING) {
												if((jstate->string_ = REALLOC(jstate->string_, strlen(state)+1)) == NULL) {
													OUT_OF_MEMORY
												}
												strcpy(jstate->string_, state);
											}
											if(devices_validate_state(tmp, -1) == 0) {
												json_append_member(jcode, "state", json_mkstring(state));
											} else {
												logprintf(LOG_ERR, "\"%s\" is an invalid state for device \"%s\"", state, device);
												json_delete(json);
												json_delete(joutput);
												json_delete(jvalues);
												return -1;
											}

											if(has_values == 1) {
												json_append_member(jcode, "values", jvalues);
											} else {
												json_delete(jvalues);
											}
											json_append_member(joutput, "action", json_mkstring("control"));
											char *output = json_stringify(joutput, NULL);
											socket_write(node->fd, output);
											json_free(output);
											json_delete(joutput);
											node->steps = VALIDATE;
										} else {
											logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
											json_delete(json);
											return -1;
										}
									}
								}
							}
							json_delete(json);
						} else {
							FREE(node->buffer);
							node->len = 0;
						}
					}
				} break;
				case VALIDATE: {
					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						if(strcmp(node->buffer, "{\"status\":\"success\"}") != 0) {
							logprintf(LOG_ERR, "failed to send command");
						}
						FREE(node->buffer);
						node->len = 0;
						return -1;
					}
				} break;
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_DISCONNECTED: {
			kill(getpid(), SIGINT);
		} break;
	}
	return 0;
}

static void *ssdp_reseek(void *node) {
	if(found == 0 && connecting == 0) {
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		threadpool_add_scheduled_work("ssdp seek", ssdp_reseek, tv, NULL);
		ssdp_seek();
	}
	return NULL;
}

static int user_input(struct eventpool_fd_t *node, int event) {
	struct timeval tv;
	switch(event) {
		case EV_CONNECT_SUCCESS: {
#ifdef _WIN32
			unsigned long on = 1;
			ioctlsocket(node->fd, FIONBIO, &on);
#else
			long arg = fcntl(node->fd, F_GETFL, NULL);
			fcntl(node->fd, F_SETFL, arg | O_NONBLOCK);
#endif
			eventpool_fd_enable_read(node);
		} break;
		case EV_READ: {
			char buf[BUFFER_SIZE];
			memset(buf, '\0', BUFFER_SIZE);
			int c = 0;
			if((c = read(node->fd, buf, BUFFER_SIZE)) > 0) {
				buf[c-1] = '\0';
				if(isNumeric(buf) == 0) {
					struct ssdp_list_t *tmp = ssdp_list;
					int i = 0;
					while(tmp) {
						if((ssdp_list_size-i) == atoi(buf)) {
							socket_connect1(tmp->server, tmp->port, client_callback);
							tv.tv_sec = 1;
							tv.tv_usec = 0;
							threadpool_add_scheduled_work("socket timeout", timeout, tv, NULL);
							connecting = 1;
							return -1;
						}
						i++;
						tmp = tmp->next;
					}
				}
			}
			eventpool_fd_enable_read(node);
		}
	}
	return 0;
}

static void *ssdp_found(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_ssdp_received_t *data = task->userdata;
	struct ssdp_list_t *node = NULL;
	struct timeval tv;
	int match = 0;

	if(connecting == 0 && data->ip != NULL && data->port > 0 && data->name != NULL) {
		if(instance == NULL) {
			struct ssdp_list_t *tmp = ssdp_list;
			while(tmp) {
				if(strcmp(tmp->server, data->ip) == 0 && tmp->port == data->port) {
					match = 1;
					break;
				}
				tmp = tmp->next;
			}
			if(match == 0) {
				if((node = MALLOC(sizeof(struct ssdp_list_t))) == NULL) {
					OUT_OF_MEMORY
				}
				strncpy(node->server, data->ip, INET_ADDRSTRLEN);
				node->port = data->port;
				strncpy(node->name, data->name, 16);

				ssdp_list_size++;

				printf("\r[%2d] %15s:%-5d %-16s\n", ssdp_list_size, node->server, node->port, node->name);
				printf("To which server do you want to connect?: ");
				fflush(stdout);

				node->next = ssdp_list;
				ssdp_list = node;
			}
		} else {
			if(strcmp(data->name, instance) == 0) {
				found = 1;
				connecting = 1;
				socket_connect1(data->ip, data->port, client_callback);
				connected = 1;
				tv.tv_sec = 1;
				tv.tv_usec = 0;
				threadpool_add_scheduled_work("socket timeout", timeout, tv, NULL);
			}
		}
	}
	return NULL;
}

int main(int argc, char **argv) {
	atomicinit();
	gc_attach(main_gc);
	gc_catch();

	struct options_t *options = NULL;
	struct timeval tv;
	char *server = NULL, *fconfig = NULL;
	unsigned short port = 0, showhelp = 0, showversion = 0;

	pilight.process = PROCESS_CLIENT;

	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(fconfig, CONFIG_FILE);

	log_file_disable();
	log_shell_enable();
	log_level_set(LOG_NOTICE);

	if((progname = MALLOC(16)) == NULL) {
		OUT_OF_MEMORY
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
	options_add(&options, 'I', "instance", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &optarg);
		if(c == -1)
			break;
		if(c == -2) {
			showhelp = 1;
			break;
		}
		switch(c) {
			case 'H':
				showhelp = 1;
			break;
			case 'V':
				showversion = 1;
			break;
			case 'd':
				if((device = REALLOC(device, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(device, optarg);
			break;
			case 's':
				if((state = REALLOC(state, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(state, optarg);
			break;
			case 'v':
				if((values = REALLOC(values, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(values, optarg);
			break;
			case 'C':
				if((fconfig = REALLOC(fconfig, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(fconfig, optarg);
			break;
			case 'I':
				if((instance = MALLOC(strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(instance, optarg);
			break;
			case 'S':
				if((server = REALLOC(server, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY
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
	options_gc();

	if(showversion == 1) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto close;
	}
	if(showhelp == 1) {
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
	}

	if(device == NULL || state == NULL ||
	   strlen(device) == 0 || strlen(state) == 0) {
		printf("Usage: %s -d device -s state\n", progname);
		FREE(fconfig);
		goto close;
	}

	protocol_init();
	storage_init();

	if(storage_read(fconfig, CONFIG_DEVICES) != EXIT_SUCCESS) {
		FREE(fconfig);
		goto close;
	}
	FREE(fconfig);

	threadpool_init(1, 1, 10);
	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found);

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	timer_thread_start();

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if(server != NULL && port > 0) {
		socket_connect1(server, port, client_callback);
		threadpool_add_scheduled_work("socket timeout", timeout, tv, NULL);
	} else {
		ssdp_seek();
		threadpool_add_scheduled_work("ssdp seek", ssdp_reseek, tv, NULL);
		if(instance == NULL) {
			printf("[%2s] %15s:%-5s %-16s\n", "#", "server", "port", "name");
			printf("To which server do you want to connect?:\r");
			fflush(stdout);
		} else {
			threadpool_add_scheduled_work("ssdp seek", ssdp_not_found, tv, NULL);
		}
	}

	if(server != NULL) {
		FREE(server);
		server = NULL;
	}

	eventpool_fd_add("stdin", fileno(stdin), user_input, NULL, NULL);
	eventpool_process(NULL);

close:

	main_gc();

	return EXIT_SUCCESS;
}
