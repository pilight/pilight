/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	#include <unistd.h>
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
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/socket.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "lirc.h"

#ifndef _WIN32
static uv_timer_t *timer_req = NULL;

static char socket_path[BUFFER_SIZE];
static int initialized = 0;
static char *name = NULL;

static void start(void);

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;

	FREE(data);
	return NULL;
}

static void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
	if(nread > 0) {
		char **nlarray = NULL;
		unsigned int e = explode(buf->base, "\n", &nlarray), t = 0;

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
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
	}

	if(nread < 0) {
		if(nread != UV_EOF) {
			logprintf(LOG_ERR, "read_cb: %s\n", uv_strerror(nread));
		} else {
			uv_read_stop((uv_stream_t *)client);

			timer_req = MALLOC(sizeof(uv_timer_t));
			if(timer_req == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			int r = 0;
			if((r = uv_timer_init(uv_default_loop(), timer_req)) != 0) {
				logprintf(LOG_ERR, "uv_timer_init: %s", uv_strerror(r));
				FREE(timer_req);
				free(buf->base);
				return;
			}

			if((r = uv_timer_start(timer_req, (void (*)(uv_timer_t *))start, 1000, 0)) != 0) {
				logprintf(LOG_ERR, "uv_timer_start: %s", uv_strerror(r));
				FREE(timer_req);
				free(buf->base);
				return;
			}
		}
	}

	free(buf->base);
	return;
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	if((buf->base = malloc(suggested_size)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	buf->len = suggested_size;
	memset(buf->base, '\0', buf->len);
}

static void connect_cb(uv_connect_t *req, int status) {
	if(status < 0) {
		logprintf(LOG_ERR, "connect_cb: %s", uv_strerror(status));
	} else {
		uv_read_start((uv_stream_t *)req->handle, alloc_cb, read_cb);
	}

	FREE(req);
}

static void start(void) {
	int err = 0;
	uv_connect_t *connect_req = MALLOC(sizeof(uv_connect_t));
	if(connect_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_pipe_t *pipe_req = MALLOC(sizeof(uv_pipe_t));
	if(pipe_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	if((err = uv_pipe_init(uv_default_loop(), pipe_req, 1)) != 0) {
		logprintf(LOG_ERR, "uv_pipe_init: %s", uv_strerror(err));
		FREE(pipe_req);
		FREE(connect_req);
		return;
	}
	uv_pipe_connect(connect_req, pipe_req, socket_path, connect_cb);
}

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jchild = NULL;
	int match = 0;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
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
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(name, jdevice->key);

	start();
	
	return NULL;
}

static void gc(void) {
	if(name != NULL) {
		FREE(name);
	}
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

	options_add(&lirc->options, "c", "code", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&lirc->options, "a", "repeat", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&lirc->options, "b", "button", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&lirc->options, "r", "remote", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);

#ifndef _WIN32
	lirc->gc=&gc;

	memset(socket_path, '\0', BUFFER_SIZE);

#ifdef PILIGHT_UNITTEST
	char *dev = getenv("PILIGHT_LIRC_DEV");
	if(dev == NULL) {
		strcpy(socket_path, "/dev/lircd");
	} else {
		strcpy(socket_path, dev);
	}
#else
	strcpy(socket_path, "/dev/lircd");
#endif
	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
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
