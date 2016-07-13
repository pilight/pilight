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
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif
#include <pthread.h>
#include <stdint.h>
#include <math.h>

#include "../../core/threadpool.h"
#include "../../core/eventpool.h"
#include "../../core/pilight.h"
#include "../../core/network.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/socket.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "xbmc.h"

typedef struct data_t {
	char *name;
	char *server;
	int port;
	int sockfd;

	int reset;
	int connected;
	unsigned long timerid;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *thread(void *param);

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void createMessage(char *server, int port, char *action, char *media) {
	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	snprintf(data->message, 1024, "{\"action\":\"%s\",\"media\":\"%s\",\"server\":\"%s\",\"port\":%d}", action, media, server, port);
	strncpy(data->origin, "receiver", 255);
	data->protocol = xbmc->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
}

static int callback(struct eventpool_fd_t *node, int event) {
	struct data_t *settings = node->userdata;

	char recvBuff[BUFFER_SIZE], action[10], media[15];
	char *m = NULL, *t = NULL;
	char shut[] = "shutdown";
	char home[] = "home";
	char none[] = "none";
	size_t bytes = 0;

	memset(&recvBuff, '\0', BUFFER_SIZE);
	memset(&action, '\0', 10);
	memset(&media, '\0', 15);

	switch(event) {
		case EV_CONNECT_SUCCESS: {
			settings->connected = 1;
			settings->reset = 1;
			createMessage(settings->server, settings->port, home, none);
			eventpool_fd_enable_read(node);
		} break;
		case EV_READ: {
			bytes = (int)recv(node->fd, recvBuff, BUFFER_SIZE, 0);
			if(bytes <= 0) {
				return -1;
			} else {
				if(json_validate(recvBuff) == true) {
					printf("%s\n", recvBuff);
					struct JsonNode *joutput = json_decode(recvBuff);
					struct JsonNode *params = NULL;
					struct JsonNode *data = NULL;
					struct JsonNode *item = NULL;

					if(json_find_string(joutput, "method", &m) == 0) {
						if(strcmp(m, "GUI.OnScreensaverActivated") == 0) {
							strcpy(media, "screensaver");
							strcpy(action, "active");
						} else if(strcmp(m, "GUI.OnScreensaverDeactivated") == 0) {
							strcpy(media, "screensaver");
							strcpy(action, "inactive");
						} else {
							if((params = json_find_member(joutput, "params")) != NULL) {
								if((data = json_find_member(params, "data")) != NULL) {
									if((item = json_find_member(data, "item")) != NULL) {
										if(json_find_string(item, "type", &t) == 0) {
											strcpy(media, t);
											if(strcmp(m, "Player.OnPlay") == 0) {
												strcpy(action, "play");
											} else if(strcmp(m, "Player.OnStop") == 0) {
												strcpy(action, home);
												strcpy(media, none);
											} else if(strcmp(m, "Player.OnPause") == 0) {
												strcpy(action, "pause");
											}
										}
									}
								}
							}
						}
						if(strlen(media) > 0 && strlen(action) > 0) {
							createMessage(settings->server, settings->port, action, media);
						}
					}
					json_delete(joutput);
				}
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_CONNECT_FAILED:
		case EV_DISCONNECTED:
			if(settings->reset == 1) {
				createMessage(settings->server, settings->port, shut, none);
				settings->reset = 0;
			}
			eventpool_fd_remove(node);
			settings->connected = 0;
			return -1;
		break;
	}
	return 0;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;

	if(settings->connected == 0) {
		eventpool_socket_add("xbmc", settings->server, settings->port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, callback, NULL, settings);
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);
	}

	return NULL;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	int match = 0, has_server = 0, has_port = 0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(xbmc->id, jchild->string_) == 0) {
				match = 1;
				break;
			}
			jchild = jchild->next;
		}
	}

	if(match == 0) {
		return NULL;
	}

	if((node = MALLOC(sizeof(struct data_t)))== NULL) {
		OUT_OF_MEMORY
	}
	memset(node, '\0', sizeof(struct data_t));

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);

			while(jchild1) {
				if(strcmp(jchild1->key, "server") == 0) {
					if((node->server = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(node->server, jchild1->string_);
					has_server = 1;
				}
				if(strcmp(jchild1->key, "port") == 0) {
					node->port = (int)round(jchild1->number_);
					has_port = 1;
				}
				jchild1 = jchild1->next;
			}
			if(has_server == 1 && has_port == 1) {
				node->sockfd = -1;
				node->next = data;
				data = node;
			} else {
				if(has_server == 1) {
					FREE(node->server);
				}
				FREE(node);
				node = NULL;
			}
			jchild = jchild->next;
		}
	}

	if(node == NULL) {
		return NULL;
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, jdevice->key);

	node->reset = 1;
	node->connected = 0;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, node);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;

	while(data) {
		tmp = data;
		FREE(tmp->name);
		FREE(tmp->server);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}

static int checkValues(JsonNode *code) {
	char *action = NULL;
	char *media = NULL;

	if(json_find_string(code, "action", &action) == 0 &&
	   json_find_string(code, "media", &media) == 0) {
		if(strcmp(media, "none") == 0) {
			if(!(strcmp(action, "shutdown") == 0 || strcmp(action, "home") == 0)) {
				return 1;
			} else {
				return 0;
			}
		} else if(strcmp(media, "episode") == 0
		   || strcmp(media, "movie") == 0
			 || strcmp(media, "movies") == 0
		   || strcmp(media, "song") == 0) {
			if(!(strcmp(action, "play") == 0 || strcmp(action, "pause") == 0)) {
				return 1;
			} else {
				return 0;
			}
		} else if(strcmp(media, "screensaver") == 0) {
			if(!(strcmp(action, "active") == 0 || strcmp(action, "inactive") == 0)) {
				return 1;
			} else {
				return 0;
			}
		} else {
			return 1;
		}
	} else {
		return 1;
	}
	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void xbmcInit(void) {
	protocol_register(&xbmc);
	protocol_set_id(xbmc, "xbmc");
	protocol_device_add(xbmc, "xbmc", "XBMC API");
	protocol_device_add(xbmc, "kodi", "Kodi API");
	xbmc->devtype = XBMC;
	xbmc->hwtype = API;
	xbmc->multipleId = 0;

	options_add(&xbmc->options, 'a', "action", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&xbmc->options, 'm', "media", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&xbmc->options, 's', "server", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&xbmc->options, 'p', "port", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);

	options_add(&xbmc->options, 0, "show-media", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&xbmc->options, 0, "show-action", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	xbmc->gc=&gc;
	xbmc->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "xbmc";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	xbmcInit();
}
#endif
