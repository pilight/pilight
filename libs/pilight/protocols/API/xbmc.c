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
#include <stdint.h>
#include <math.h>

#include "../../core/eventpool.h"
#include "../../core/pilight.h"
#include "../../core/network.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/socket.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "xbmc.h"

typedef struct data_t {
	char *name;
	char *server;
	int port;
	int sockfd;
	uv_timer_t *timer_req;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *timer_cb(uv_timer_t *timer_req);

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void createMessage(char *server, int port, char *action, char *media) {
	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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

static void parseMessage(struct data_t *settings, char *message) {
	char action[10], media[15];
	char *m = NULL, *t = NULL;
	char shut[] = "shutdown";
	char home[] = "home";
	char none[] = "none";

	if(message == NULL) {
		createMessage(settings->server, settings->port, shut, none);
		return;
	}

	memset(&action, '\0', 10);
	memset(&media, '\0', 15);

	struct JsonNode *joutput = json_decode(message);
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
	return;
}

static void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
	struct data_t *settings = client->data;
	char message[1024], **array = NULL;
	int n = 0, i = 0;

	if(nread > 0) {
		memset(&message, '\0', 1024);

		if(strstr(buf->base, "}{") != NULL) {
			n = explode(buf->base, "}{", &array);
			for(i=0;i<n;i++) {
				if(i == 0) {
					sprintf(message, "%s}", array[i]);
				} else if(i+1 == n) {
					sprintf(message, "{%s", array[i]);
				} else {
					sprintf(message, "{%s}", array[i]);
				}
				if(json_validate(message) == true) {
					parseMessage(settings, message);
				}
			}
			array_free(&array, n);
		} else {
			parseMessage(settings, buf->base);
		}
	}

	if(nread < 0) {
		if(nread != UV_EOF) {
			logprintf(LOG_ERR, "read_cb: %s\n", uv_strerror(nread));
		} else {
			uv_read_stop((uv_stream_t *)client);

			parseMessage(settings, NULL);

			settings->timer_req = MALLOC(sizeof(uv_timer_t));
			if(settings->timer_req == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			settings->timer_req->data = settings;

			int r = 0;
			if((r = uv_timer_init(uv_default_loop(), settings->timer_req)) != 0) {
				logprintf(LOG_ERR, "uv_timer_init: %s", uv_strerror(r));
				FREE(settings->timer_req);
				free(buf->base);
				return;
			}

			if((r = uv_timer_start(settings->timer_req, (void (*)(uv_timer_t *))timer_cb, 1000, 0)) != 0) {
				logprintf(LOG_ERR, "uv_timer_start: %s", uv_strerror(r));
				FREE(settings->timer_req);
				free(buf->base);
				return;
			}
		}
	}

	free(buf->base);
	return;
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	buf->base = malloc(suggested_size);
	buf->len = suggested_size;
	memset(buf->base, '\0', buf->len);
}

static void connect_cb(uv_connect_t *req, int status) {
	if(status < 0) {
		logprintf(LOG_ERR, "connect_cb: %s\n", uv_strerror(status));
	} else {
		req->handle->data = req->data;
		uv_read_start((uv_stream_t *)req->handle, alloc_cb, read_cb);
	}

	FREE(req);
}

static void start(struct data_t *node) {
	struct sockaddr_in addr;
	int r = 0;

	uv_tcp_t *tcp_req = MALLOC(sizeof(uv_tcp_t));
	if(tcp_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	r = uv_tcp_init(uv_default_loop(), tcp_req);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_tcp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		// goto close;
	}

	r = uv_ip4_addr(node->server, node->port, &addr);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_ip4_addr: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		// goto close;
	}

	uv_connect_t *connect_req = MALLOC(sizeof(uv_connect_t));
	if(connect_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	tcp_req->data = node;
	connect_req->data = node;

	uv_tcp_connect(connect_req, tcp_req, (struct sockaddr *)&addr, connect_cb);
}

static void *timer_cb(uv_timer_t *timer_req) {
	start(timer_req->data);
	return NULL;
}

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct data_t *node = NULL;
	int match = 0, has_server = 0, has_port = 0;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
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
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(node, '\0', sizeof(struct data_t));

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);

			while(jchild1) {
				if(strcmp(jchild1->key, "server") == 0) {
					if((node->server = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jdevice->key);

	node->timer_req = NULL;

	start(node);

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

	options_add(&xbmc->options, "a", "action", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&xbmc->options, "m", "media", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&xbmc->options, "s", "server", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&xbmc->options, "p", "port", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);

	options_add(&xbmc->options, "0", "show-media", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&xbmc->options, "0", "show-action", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	xbmc->gc=&gc;
	xbmc->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
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
