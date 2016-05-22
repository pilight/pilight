/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include "libs/pilight/core/eventpool.h"
#include "libs/pilight/core/timerpool.h"
#include "libs/pilight/core/threadpool.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/gc.h"
#include "libs/pilight/protocols/protocol.h"

static unsigned short stats = 0;
static unsigned short connecting = 0;
static unsigned short connected = 0;
static unsigned short found = 0;
static char *instance = NULL;
char **filters = NULL;
unsigned int m = 0;

typedef struct ssdp_list_t {
	char server[INET_ADDRSTRLEN+1];
	unsigned short port;
	char name[17];
	struct ssdp_list_t *next;
} ssdp_list_t;

static struct ssdp_list_t *ssdp_list = NULL;
static int ssdp_list_size = 0;
static unsigned short filteropt = 0;

int main_gc(void) {
	char *filter = NULL;

	if(filter != NULL) {
		FREE(filter);
		filter = NULL;
	}
	if(filters != NULL) {
		array_free(&filters, m);
	}

	struct ssdp_list_t *tmp = NULL;
	while(ssdp_list) {
		tmp = ssdp_list;
		ssdp_list = ssdp_list->next;
		FREE(tmp);
	}

	protocol_gc();
	timer_thread_gc();
	eventpool_gc();

	log_shell_disable();
	log_gc();
	FREE(progname);

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
			struct JsonNode *jclient = json_mkobject();
			struct JsonNode *joptions = json_mkobject();
			json_append_member(jclient, "action", json_mkstring("identify"));
			json_append_member(joptions, "receiver", json_mknumber(1, 0));
			json_append_member(joptions, "stats", json_mknumber(stats, 0));
			json_append_member(jclient, "options", joptions);
			char *out = json_stringify(jclient, NULL);
			socket_write(node->fd, out);
			json_delete(jclient);
			json_free(out);
			eventpool_fd_enable_write(node);
		} break;
		case EV_WRITE: {
			eventpool_fd_enable_read(node);
		}	break;
		case EV_READ: {
			int x = socket_recv(node->fd, &node->buffer, &node->len);
			if(x == -1) {
				return -1;
			} else if(x == 0) {
				eventpool_fd_enable_read(node);
				return 0;
			} else {
				if(strcmp(node->buffer, "{\"status\":\"success\"}") != 0) {
					char **array = NULL;
					char *protocol = NULL;
					unsigned int n = explode(node->buffer, "\n", &array), i = 0;
					for(i=0;i<n;i++) {
						if(json_validate(array[i]) == true) {
							struct JsonNode *jcontent = json_decode(array[i]);
							struct JsonNode *jtype = json_find_member(jcontent, "type");
							if(jtype != NULL) {
								json_remove_from_parent(jtype);
								json_delete(jtype);
							}
							if(filteropt == 1) {
								int filtered = 0, j = 0;
								if(json_find_string(jcontent, "protocol", &protocol) == 0) {
									for(j=0;j<m;j++) {
										if(strcmp(filters[j], protocol) == 0) {
											filtered = 1;
											break;
										}
									}
								}
								if(filtered == 0) {
									char *content = json_stringify(jcontent, "\t");
									printf("%s\n", content);
									json_free(content);
								}
							} else {
								char *content = json_stringify(jcontent, "\t");
								printf("%s\n", content);
								json_free(content);
							}
							json_delete(jcontent);
						}
					}
					array_free(&array, n);
				}
				FREE(node->buffer);
				node->len = 0;
				eventpool_fd_enable_read(node);
			}
		} break;
		case EV_DISCONNECTED: {
			kill(getpid(), SIGINT);
		} break;
	}
	return 0;
}

static void *ssdp_reseek(void *param) {
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
	struct timeval tv;
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
					OUT_OF_MEMORY
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
				connecting = 1;
				socket_connect1(data->ip, data->port, client_callback);
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

	log_shell_enable();
	log_file_disable();

	log_level_set(LOG_NOTICE);

	pilight.process = PROCESS_CLIENT;

	if((progname = MALLOC(16)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(progname, "pilight-receive");
	struct options_t *options = NULL;
	struct timeval tv;

	char *args = NULL;
	char *server = NULL;
	char *filter = NULL;
	unsigned short port = 0;

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'S', "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, 'I', "instance", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 's', "stats", OPTION_NO_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, 'F', "filter", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

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
				printf("\t -I --instance=name\t\tconnect to pilight instance\n");
				printf("\t -s --stats\t\t\tshow CPU and RAM statistics\n");
				printf("\t -F --filter=protocol\t\tfilter out protocol(s)\n");
				exit(EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s v%s\n", progname, PILIGHT_VERSION);
				exit(EXIT_SUCCESS);
			break;
			case 'S':
				if((server = MALLOC(strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(server, args);
			break;
			case 'I':
				if((instance = MALLOC(strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(instance, args);
			break;
			case 'P':
				port = (unsigned short)atoi(args);
			break;
			case 's':
				stats = 1;
			break;
			case 'F':
				if((filter = REALLOC(filter, strlen(args)+1)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				strcpy(filter, args);
				filteropt = 1;
			break;
			default:
				printf("Usage: %s \n", progname);
				exit(EXIT_SUCCESS);
			break;
		}
	}
	options_delete(options);
	options_gc();

	if(filteropt == 1) {
		struct protocol_t *protocol = NULL;
		m = explode(filter, ",", &filters);
		int match = 0, j = 0;

		protocol_init();

		for(j=0;j<m;j++) {
			match = 0;
			struct protocols_t *pnode = protocols;
			if(filters[j] != NULL && strlen(filters[j]) > 0) {
				while(pnode) {
					protocol = pnode->listener;
					if(protocol_device_exists(protocol, filters[j]) == 0 && match == 0) {
						match = 1;
						break;
					}
					pnode = pnode->next;
				}
				if(match == 0) {
					logprintf(LOG_ERR, "invalid protocol: %s", filters[j]);
					goto close;
				}
			}
		}
	}

	threadpool_init(1, 1, 10);
	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	timer_thread_start();

	tv.tv_sec = 1;

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

	eventpool_fd_add("stdin", fileno(stdin), user_input, NULL, NULL);
	eventpool_process(NULL);

close:
	if(server != NULL) {
		FREE(server);
	}
	if(instance != NULL) {
		FREE(instance);
	}

	main_gc();

	return EXIT_SUCCESS;
}
