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
#include <fcntl.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#endif


#include "libs/libuv/uv.h"
#include "libs/pilight/core/eventpool.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/json.h"
#include "libs/pilight/core/ssdp.h"

#include "libs/pilight/protocols/protocol.h"
#include "libs/pilight/storage/storage.h"
#include "libs/pilight/events/events.h"

#define STATUS					0
#define REQUESTCONFIG		1
#define RECEIVECONFIG		2
#define VALIDATE				3

static uv_tty_t *tty_req = NULL;
static uv_signal_t *signal_req = NULL;
static int steps = STATUS;

static struct ssdp_list_t *ssdp_list = NULL;
static int ssdp_list_size = 0;
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

static void signal_cb(uv_signal_t *, int);
static uv_timer_t *ssdp_reseek_req = NULL;

static int main_gc(void) {
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

	socket_gc();
	protocol_gc();
	storage_gc();
	ssdp_gc();

	eventpool_gc();

	log_shell_disable();
	log_gc();

	if(progname != NULL) {
		FREE(progname);
		progname = NULL;
	}

	return 0;
}

static void timeout_cb(uv_timer_t *param) {
	if(connected == 0) {
		logprintf(LOG_ERR, "could not connect to the pilight instance");
		signal_cb(NULL, SIGINT);
	}
}

static void ssdp_not_found(uv_timer_t *param) {
	if(found == 0) {
		logprintf(LOG_ERR, "could not find pilight instance: %s", instance);
		signal_cb(NULL, SIGINT);
	}
}

static void alloc_cb(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	if((buf->base = malloc(len)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(buf->base, 0, len);
}

static void on_read(int fd, char *buf, ssize_t len, char **buf1, ssize_t *len1) {
	if(strcmp(buf, "1") != 0) {
		if(socket_recv(buf, len, buf1, len1) > 0) {
			switch(steps) {
				case STATUS:
					if(strncmp(*buf1, "{\"status\":\"success\"}", 20) == 0) {
						steps = RECEIVECONFIG;

						socket_write(fd, "{\"action\":\"request config\"}");
					}
				break;
				case RECEIVECONFIG: {
					char *message = NULL;
					int has_values = 0;
					if(json_validate(*buf1) == true) {
						struct JsonNode *json = json_decode(*buf1);
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
									struct JsonNode *jdevices = NULL;
									if((jdevices = json_find_member(jconfig, "devices")) != NULL) {
										devices_import(jdevices);
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
														OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
															OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
																	OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
															goto close;
															return;
														}
													}
													FREE(name);
												}
												array_free(&array, n);
											}

											struct JsonNode *jstate = json_find_member(tmp, "state");
											if(jstate != NULL && jstate->tag == JSON_STRING) {
												if((jstate->string_ = REALLOC(jstate->string_, strlen(state)+1)) == NULL) {
													OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
												goto close;
												return;
											}

											if(has_values == 1) {
												json_append_member(jcode, "values", jvalues);
											} else {
												json_delete(jvalues);
											}
											json_append_member(joutput, "action", json_mkstring("control"));

											char *out = json_stringify(joutput, NULL);
											socket_write(fd, out);
											json_free(out);
											json_delete(joutput);

											steps = VALIDATE;
										} else {
											logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
											json_delete(json);
											goto close;
											return;
										}
									}
								}
							}
						}
						json_delete(json);
					}
				} break;
				case VALIDATE: {
					if(strncmp(*buf1, "{\"status\":\"success\"}", 20) != 0) {
						logprintf(LOG_ERR, "failed to send command");
					}
					goto close;
				} break;
			}

			FREE(*buf1);
			*len1 = 0;
		}
	}

	return;

close:
	FREE(*buf1);
	*len1 = 0;
	kill(getpid(), SIGINT);
}

static void *socket_disconnected(int reason, void *param, void *userdata) {
	struct reason_socket_disconnected_t *data = param;

	socket_close(data->fd);
	signal_cb(NULL, SIGINT);

	return NULL;
}

static void *socket_connected(int reason, void *param, void *userdata) {
	struct reason_socket_connected_t *data = param;

	connected = 1;

	struct JsonNode *jclient = json_mkobject();
	json_append_member(jclient, "action", json_mkstring("identify"));

	char *out = json_stringify(jclient, NULL);

	socket_write(data->fd, out);

	json_delete(jclient);
	json_free(out);
	return NULL;
}

static void connect_to_server(char *server, int port) {
	socket_connect(server, port, on_read);

	uv_timer_t *socket_timeout_req = NULL;
	
	if((socket_timeout_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), socket_timeout_req);
	uv_timer_start(socket_timeout_req, timeout_cb, 1000, 0);
}

static int select_server(int server) {
	struct ssdp_list_t *tmp = ssdp_list;
	int i = 0;
	while(tmp) {
		if((ssdp_list_size-i) == server) {
			connect_to_server(tmp->server, tmp->port);
			return 0;
		}
		i++;
		tmp = tmp->next;
	}
	return -1;
}

static void *ssdp_found(int reason, void *param, void *userdata) {
	struct reason_ssdp_received_t *data = param;
	struct ssdp_list_t *node = NULL;
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
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strncpy(node->server, data->ip, INET_ADDRSTRLEN);
				node->port = data->port;
				strncpy(node->name, data->name, 17);

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
				uv_timer_stop(ssdp_reseek_req);
				connect_to_server(data->ip, data->port);
			}
		}
	}
	return NULL;
}

static void read_cb(uv_stream_t *stream, ssize_t len, const uv_buf_t *buf) {
	buf->base[len-1] = '\0';

#ifdef _WIN32
	/* Remove windows vertical tab */
	if(buf->base[len-2] == 13) {
		buf->base[len-2] = '\0';
	}
#endif

	if(isNumeric(buf->base) == 0) {
		select_server(atoi(buf->base));
	}
	free(buf->base);
}

static void signal_cb(uv_signal_t *handle, int signum) {
	if(instance == NULL && tty_req != NULL) {
		uv_read_stop((uv_stream_t *)tty_req);
		tty_req = NULL;
	}
	uv_stop(uv_default_loop());
	main_gc();
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void main_loop(int onclose) {
	if(onclose == 1) {
		signal_cb(NULL, SIGINT);
	}
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);	
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	if(onclose == 1) {
		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			usleep(10);
		}
	}
}

int main(int argc, char **argv) {
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	struct options_t *options = NULL;
	char *server = NULL, *fconfig = NULL;
	unsigned short port = 0, showhelp = 0, showversion = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	pilight.process = PROCESS_CLIENT;

	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(fconfig, CONFIG_FILE);

	log_init();
	log_file_disable();
	log_shell_enable();
	log_level_set(LOG_NOTICE);

	if((progname = MALLOC(16)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(progname, "pilight-control");

	if((signal_req = MALLOC(sizeof(uv_signal_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_signal_init(uv_default_loop(), signal_req);
	uv_signal_start(signal_req, signal_cb, SIGINT);	

	/* Define all CLI arguments of this program */
	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "d", "device", OPTION_HAS_VALUE, 0,  JSON_NULL, NULL, NULL);
	options_add(&options, "s", "state", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "v", "values", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "S", "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, "P", "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, "C", "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "I", "instance", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse1(&options, argc, argv, 1, &optarg, NULL);
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
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(device, optarg);
			break;
			case 's':
				if((state = REALLOC(state, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(state, optarg);
			break;
			case 'v':
				if((values = REALLOC(values, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(values, optarg);
			break;
			case 'C':
				if((fconfig = REALLOC(fconfig, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(fconfig, optarg);
			break;
			case 'I':
				if((instance = MALLOC(strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(instance, optarg);
			break;
			case 'S':
				if((server = REALLOC(server, strlen(optarg)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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

	// if(storage_read(fconfig, CONFIG_DEVICES) != EXIT_SUCCESS) {
		// FREE(fconfig);
		// goto close;
	// }
	FREE(fconfig);

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found, NULL);
	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected, NULL);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_disconnected, NULL);
	
	if(server != NULL && port > 0) {
		connect_to_server(server, port);
	} else {
		ssdp_seek();
		if((ssdp_reseek_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		uv_timer_init(uv_default_loop(), ssdp_reseek_req);
		uv_timer_start(ssdp_reseek_req, (void (*)(uv_timer_t *))ssdp_seek, 3000, 3000);
		if(instance == NULL) {
			printf("[%2s] %15s:%-5s %-16s\n", "#", "server", "port", "name");
			printf("To which server do you want to connect?:\r");
			fflush(stdout);

			if((tty_req = MALLOC(sizeof(uv_tty_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_tty_init(uv_default_loop(), tty_req, 0, 1);
			uv_read_start((uv_stream_t *)tty_req, alloc_cb, read_cb);
		} else {
			uv_timer_t *ssdp_not_found_req = NULL;
			if((ssdp_not_found_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_timer_init(uv_default_loop(), ssdp_not_found_req);
			uv_timer_start(ssdp_not_found_req, ssdp_not_found, 1000, 0);
		}
	}

	main_loop(0);

	FREE(progname);

close:
	main_loop(1);

	if(server != NULL) {
		FREE(server);
	}

	if(instance != NULL) {
		FREE(instance);
	}

	main_gc();

	return EXIT_SUCCESS;
}
