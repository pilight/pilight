/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <sys/un.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif
#include <stdint.h>
#include <math.h>

#include "../../core/eventpool.h"
#include "../../core/threadpool.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/socket.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "lirc.h"

#ifndef _WIN32
static char socket_path[BUFFER_SIZE];
static int connected = 0;
static int initialized = 0;
static char *name = NULL;

static void *thread(void *param);

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static int callback(struct eventpool_fd_t *node, int state) {
	struct timeval tv;

	switch(state) {
		case EV_SOCKET_SUCCESS: {
			int ret = 0;
			struct sockaddr_un servaddr;

			memset(&servaddr, '\0', sizeof(servaddr));
			servaddr.sun_family = AF_UNIX;
			strcpy(servaddr.sun_path, socket_path);

			if((ret = connect(node->fd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr))) < 0) {
				if(errno == EINPROGRESS || errno == EISCONN) {
					node->stage = EVENTPOOL_STAGE_CONNECTING;
					eventpool_fd_enable_write(node);
					return 0;
				} else {
					return -1;
				}
			} else if(ret == 0) {
				node->stage = EVENTPOOL_STAGE_CONNECTING;
				eventpool_fd_enable_write(node);
				return 0;
			} else {
				return -1;
			}
			return -1;
		} break;
		case EV_CONNECT_SUCCESS: {
			eventpool_fd_enable_read(node);
			tv.tv_sec = 86400;
			tv.tv_usec = 0;
			threadpool_add_scheduled_work(name, thread, tv, NULL);
			connected = 1;
		} break;
		case EV_READ: {
			size_t bytes = 0;
			char recvBuff[BUFFER_SIZE];
			memset(&recvBuff, '\0', BUFFER_SIZE);
			bytes = (int)recv(node->fd, recvBuff, BUFFER_SIZE, 0);
			if(bytes <= 0) {
				return -1;
			} else {
				char **nlarray = NULL;
				unsigned int e = explode(recvBuff, "\n", &nlarray), t = 0;
				for(t=0;t<e;t++) {
					int x = 0, nrspace = 0;
					for(x=0;x<strlen(nlarray[t]);x++) {
						if(nlarray[t][x] == ' ') {
							nrspace++;
						}
					}
					if(nrspace >= 3) {
						char **array = NULL;
						unsigned int q = explode(nlarray[t], " ", &array);
						if(q == 4) {
							char *code1 = array[0];
							char *rep = array[1];
							char *btn = array[2];
							char *remote = array[3];
							char *y = NULL;
							if((y = strstr(remote, "\n")) != NULL) {
								size_t pos = (size_t)(y-remote);
								remote[pos] = '\0';
							}
							int r = strtol(rep, NULL, 16);

							struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
							if(data == NULL) {
								OUT_OF_MEMORY
							}
							snprintf(data->message, 1024, "{\"code\":\"%s\",\"repeat\":%d,\"button\":\"%s\",\"remote\":\"%s\"}", code1, r, btn, remote);
							strncpy(data->origin, "receiver", 255);
							data->protocol = lirc->id;
							if(strlen(pilight_uuid) > 0) {
								data->uuid = pilight_uuid;
							} else {
								data->uuid = NULL;
							}
							data->repeat = 1;
							eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
						}
						array_free(&array, q);
					}
				}
				array_free(&nlarray, t);
				memset(recvBuff, '\0', BUFFER_SIZE);
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_CONNECT_FAILED:
		case EV_DISCONNECTED:
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			threadpool_add_scheduled_work(name, thread, tv, (void *)NULL);
			eventpool_fd_remove(node);
			connected = 0;
			return -1;
		break;
	}
	return 0;
}

static void *thread(void *param) {
	if(connected == 0) {
		connected = 1;
		if(path_exists(socket_path) == EXIT_SUCCESS) {
			eventpool_socket_add("lirc", NULL, -1, AF_UNIX, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, callback, NULL, NULL);
		}
	}

	return NULL;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;

	struct timeval tv;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jchild = NULL;
	int match = 0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}
	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(lirc->id, jchild->string_) == 0) {
				match = 1;
				break;
		}
			jchild = jchild->next;
		}
	}
	if(match == 0) {
		return NULL;
	}

	if(initialized == 0) {
		initialized = 1;
	} else {
		return NULL;
	}

	if((name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(name, jdevice->key);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, NULL);

	return NULL;
}

static void gc(void) {
	FREE(name);
}

#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void lircInit(void) {
	protocol_register(&lirc);
	protocol_set_id(lirc, "lirc");
	protocol_device_add(lirc, "lirc", "Lirc API");
	lirc->devtype = LIRC;
	lirc->hwtype = API;

	options_add(&lirc->options, 'c', "code", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&lirc->options, 'a', "repeat", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&lirc->options, 'b', "button", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&lirc->options, 'r', "remote", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);

#ifndef _WIN32
	// lirc->initDev=&initDev;
	lirc->gc=&gc;

	memset(socket_path, '\0', BUFFER_SIZE);
	strcpy(socket_path, "/dev/lircd");
	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "lirc";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	lircInit();
}
#endif
