/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "libs/pilight/core/threadpool.h"
#include "libs/pilight/core/timerpool.h"
#include "libs/pilight/core/eventpool.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/gc.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/json.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/dso.h"

#include "libs/pilight/protocols/protocol.h"

#define CONNECT			1
#define VALIDATE		2
#define SEND				3
#define SUCCESS			4

typedef struct pname_t {
	char *name;
	char *desc;
	struct pname_t *next;
} pname_t;

typedef struct ssdp_list_t {
	char server[INET_ADDRSTRLEN+1];
	unsigned short port;
	char name[17];
	struct ssdp_list_t *next;
} ssdp_list_t;

static struct pname_t *pname = NULL;

static struct ssdp_list_t *ssdp_list = NULL;
static int ssdp_list_size = 0;

static int status = CONNECT;
static unsigned short connected = 0;
static unsigned short connecting = 0;
static unsigned short found = 0;
static char *instance = NULL;
static struct JsonNode *code = NULL;
static char *uuid = NULL;

int main_gc(void) {
	if(code != NULL) {
		json_delete(code);
		code = NULL;
	}
	log_shell_disable();

	protocol_gc();
	log_gc();

	timer_thread_gc();
	eventpool_gc();

	log_shell_disable();
	log_gc();
	FREE(progname);

	return 0;
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	switch(event) {
		case EV_CONNECT_SUCCESS: {
			switch(status) {
				case CONNECT: {
					connected = 1;
					socket_write(node->fd, "{\"action\":\"identify\"}");
					eventpool_fd_enable_write(node);
					status = VALIDATE;
				} break;
			}
		} break;
		case EV_WRITE: {
			switch(status) {
				case VALIDATE:
					eventpool_fd_enable_read(node);
				break;
				case SEND: {
					struct JsonNode *json = json_mkobject();
					json_append_member(json, "action", json_mkstring("send"));
					if(uuid != NULL) {
						json_append_member(code, "uuid", json_mkstring(uuid));
					}
					json_append_member(json, "code", code);

					char *output = json_stringify(json, NULL);
					socket_write(node->fd, output);
					json_free(output);
					json_delete(json);
					code = NULL;

					status = SUCCESS;
					eventpool_fd_enable_read(node);
				} break;
			}
		} break;
		case EV_READ: {
			switch(status) {
				case VALIDATE: {
					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						if(strcmp(node->buffer, "{\"status\":\"success\"}") == 0) {
							status = SEND;
							eventpool_fd_enable_write(node);
						}
						FREE(node->buffer);
						node->len = 0;
					}
					FREE(node->buffer);
				} break;
				case SUCCESS: {
					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						if(strcmp(node->buffer, "{\"status\":\"success\"}") != 0) {
							logprintf(LOG_ERR, "failed to send codes");
						}
						FREE(node->buffer);
						node->len = 0;
					}
					return -1;
				} break;
			}
		} break;
		case EV_DISCONNECTED: {
			kill(getpid(), SIGINT);
		} break;
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

static void sort_list(void) {
	struct pname_t *a = NULL;
	struct pname_t *b = NULL;
	struct pname_t *c = NULL;
	struct pname_t *e = NULL;
	struct pname_t *tmp = NULL;

	while(pname && e != pname->next) {
		c = a = pname;
		b = a->next;
		while(a != e) {
			if(strcmp(a->name, b->name) > 0) {
				if(a == pname) {
					tmp = b->next;
					b->next = a;
					a->next = tmp;
					pname = b;
					c = b;
				} else {
					tmp = b->next;
					b->next = a;
					a->next = tmp;
					c->next = b;
					c = b;
				}
			} else {
				c = a;
				a = a->next;
			}
			b = a->next;
			if(b == e)
				e = a;
		}
	}
}

int main(int argc, char **argv) {
	atomicinit();
	gc_attach(main_gc);
	gc_catch();

	log_file_disable();
	log_shell_enable();
	log_level_set(LOG_NOTICE);

	pilight.process = PROCESS_CLIENT;

	if((progname = MALLOC(13)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(progname, "pilight-send");

	struct options_t *options = NULL;
	struct timeval tv;

	int sockfd = 0;
	int raw[MAXPULSESTREAMLENGTH-1];
	char *args = NULL, *recvBuff = NULL;

	char *protobuffer = NULL;
	int match = 0;

	int help = 0;
	int version = 0;
	int protohelp = 0;

	char *server = NULL;
	unsigned short port = 0;

	struct protocol_t *protocol = NULL;

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'p', "protocol", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'S', "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, 'I', "instance", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'U', "uuid", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[a-zA-Z0-9]{4}-[a-zA-Z0-9]{2}-[a-zA-Z0-9]{2}-[a-zA-Z0-9]{2}-[a-zA-Z0-9]{6}");

	/* Get the protocol to be used */
	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 0, &args);
		if(c == -1)
			break;
		if(c == -2)
			c = 'H';
		switch(c) {
			case 'p':
				if(strlen(args) == 0) {
					logprintf(LOG_INFO, "options '-p' and '--protocol' require an argument");
					exit(EXIT_FAILURE);
				} else {
					if((protobuffer = REALLOC(protobuffer, strlen(args)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(protobuffer, args);
				}
			break;
			case 'V':
				version = 1;
			break;
			case 'H':
				help = 1;
			break;
			case 'I':
				if((instance = MALLOC(strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(instance, args);
			break;
			case 'S':
				if((server = REALLOC(server, strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(server, args);
			break;
			case 'P':
				port = (unsigned short)atoi(args);
			break;
			case 'U':
				if((uuid = REALLOC(uuid, strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(uuid, args);
			break;
			default:;
		}
	}

	/* Initialize protocols */
	protocol_init();

	/* Check if a protocol was given */
	if(protobuffer != NULL && strlen(protobuffer) > 0 && strcmp(protobuffer, "-V") != 0) {
		if(strlen(protobuffer) > 0 && version) {
			printf("-p and -V cannot be combined\n");
		} else {
			struct protocols_t *pnode = protocols;
			/* Retrieve the used protocol */
			while(pnode) {
				/* Check if the protocol exists */
				protocol = pnode->listener;
				if(protocol_device_exists(protocol, protobuffer) == 0 && match == 0 && protocol->createCode != NULL) {
					match=1;
					/* Check if the protocol requires specific CLI arguments
					   and merge them with the main CLI arguments */
					if(protocol->options && help == 0) {
						options_merge(&options, &protocol->options);
					} else if(help == 1) {
						protohelp=1;
					}
					break;
				}
				pnode = pnode->next;
			}
			/* If no protocols matches the requested protocol */
			if(match == 0) {
				logprintf(LOG_ERR, "this protocol is not supported or doesn't support sending");
				goto close;
			}
		}
	}

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 2, &args);

		if(c == -1)
			break;
		if(c == -2) {
			if(match == 1) {
				protohelp = 1;
			} else {
				help = 1;
			}
		break;
		}
	}

	/* Display help or version information */
	if(version == 1) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto close;
	} else if(help == 1 || protohelp == 1 || match == 0) {
		if(protohelp == 1 && match == 1 && protocol->printHelp)
			printf("Usage: %s -p %s [options]\n", progname, protobuffer);
		else
			printf("Usage: %s -p protocol [options]\n", progname);
		if(help == 1) {
			printf("\t -H --help\t\t\tdisplay this message\n");
			printf("\t -V --version\t\t\tdisplay version\n");
			printf("\t -S --server=x.x.x.x\t\tconnect to server address\n");
			printf("\t -P --port=xxxx\t\t\tconnect to server port\n");
			printf("\t -I --instance=name\t\tconnect to pilight instance\n");
			printf("\t -C --config\t\t\tconfig file\n");
			printf("\t -U --uuid=xxx-xx-xx-xx-xxxxxx\tUUID\n");
			printf("\t -p --protocol=protocol\t\tthe protocol that you want to control\n");
		}
		if(protohelp == 1 && match == 1 && protocol->printHelp) {
			printf("\n\t[%s]\n", protobuffer);
			protocol->printHelp();
		} else {
			printf("\nThe supported protocols are:\n");
			struct protocols_t *pnode = protocols;
			/* Retrieve the used protocol */
			while(pnode) {
				protocol = pnode->listener;
				if(protocol->createCode) {
					struct protocol_devices_t *tmpdev = protocol->devices;
					while(tmpdev) {
						struct pname_t *node = MALLOC(sizeof(struct pname_t));
						if(node == NULL) {
							OUT_OF_MEMORY
						}
						if((node->name = MALLOC(strlen(tmpdev->id)+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(node->name, tmpdev->id);
						if((node->desc = MALLOC(strlen(tmpdev->desc)+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(node->desc, tmpdev->desc);
						node->next = pname;
						pname = node;
						tmpdev = tmpdev->next;
					}
				}
				pnode = pnode->next;
			}
			sort_list();
			struct pname_t *ptmp = NULL;
			while(pname) {
				ptmp = pname;
				printf("\t %s\t\t",ptmp->name);
				if(strlen(ptmp->name) < 7)
					printf("\t");
				if(strlen(ptmp->name) < 15)
					printf("\t");
				printf("%s\n", ptmp->desc);
				FREE(ptmp->name);
				FREE(ptmp->desc);
				pname = pname->next;
				FREE(ptmp);
			}
			FREE(pname);
		}
		goto close;
	}

	code = json_mkobject();
	int itmp = 0;
	/* Check if we got sufficient arguments from this protocol */
	struct options_t *tmp = options;
	while(tmp) {
		if(strlen(tmp->name) > 0) {
			/* Only send the CLI arguments that belong to this protocol, the protocol name
			and those that are called by the user */
			if((options_get_id(&protocol->options, tmp->name, &itmp) == 0)
			    && tmp->vartype == JSON_STRING && tmp->string_ != NULL
				&& (strlen(tmp->string_) > 0)) {
				if(isNumeric(tmp->string_) == 0) {
					json_append_member(code, tmp->name, json_mknumber(atof(tmp->string_), nrDecimals(tmp->string_)));
				} else {
					json_append_member(code, tmp->name, json_mkstring(tmp->string_));
				}
			}
			if(strcmp(tmp->name, "protocol") == 0 && strlen(tmp->string_) > 0) {
				JsonNode *jprotocol = json_mkarray();
				json_append_element(jprotocol, json_mkstring(tmp->string_));
				json_append_member(code, "protocol", jprotocol);
			}
		}
		tmp = tmp->next;
	}

	options_delete(options);
	options_gc();

	threadpool_init(1, 1, 10);
	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found);

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	timer_thread_start();

	tv.tv_sec = 1;
	memset(raw, 0, MAXPULSESTREAMLENGTH-1);
	protocol->raw = raw;
	char message[255];
	if(protocol->createCode(code, message) == 0) {
		if(server != NULL && port > 0) {
			socket_connect1(server, port, client_callback);
			threadpool_add_scheduled_work("socket timeout", timeout, tv, NULL);
		} else {
			ssdp_seek();
			threadpool_add_scheduled_work("ssdp seek", ssdp_reseek, tv, NULL);
			if(instance == NULL) {
				printf("[%2s] %15s:%-5s %-16s\n", "#", "server", "port", "name");
				printf("To which server do you want to send?:\r");
				fflush(stdout);
			} else {
				threadpool_add_scheduled_work("ssdp seek", ssdp_not_found, tv, NULL);
			}
		}

		eventpool_fd_add("stdin", fileno(stdin), user_input, NULL, NULL);
		eventpool_process(NULL);

		if(server != NULL) {
			FREE(server);
		}
	}

close:
	if(instance != NULL) {
		FREE(instance);
	}
	if(sockfd > 0) {
		socket_close(sockfd);
	}
	if(recvBuff != NULL) {
		FREE(recvBuff);
	}
	if(server != NULL) {
		FREE(server);
	}
	if(protobuffer != NULL) {
		FREE(protobuffer);
	}
	if(uuid != NULL) {
		FREE(uuid);
	}

	struct ssdp_list_t *tmp1 = NULL;
	while(ssdp_list) {
		tmp1 = ssdp_list;
		ssdp_list = ssdp_list->next;
		FREE(tmp1);
	}

	main_gc();

	return EXIT_SUCCESS;
}
