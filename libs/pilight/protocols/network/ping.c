/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#ifndef _WIN32
	#include <unistd.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "../../core/eventpool.h"
#include "../../core/pilight.h"
#include "../../core/ping.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/json.h"
#include "ping.h"

typedef struct data_t {
	char *name;
	char *ip;

	int state;
	int interval;
	int polling;
	uv_timer_t *timer_req;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

#define INTERVAL				3
#define CONNECTED				1
#define DISCONNECTED 		0

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void callback(char *a, int b) {
	struct data_t *settings = data;
	while(settings) {
		if(strcmp(settings->ip, a) == 0) {
			break;
		}
		settings = settings->next;
	}

	settings->polling = 0;

	if(b == 0) {
		if(settings->state == DISCONNECTED) {
			settings->state = CONNECTED;
			struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			snprintf(data->message, 1024, "{\"ip\":\"%s\",\"state\":\"connected\"}", a);
			strncpy(data->origin, "receiver", 255);
			data->protocol = pping->id;
			if(strlen(pilight_uuid) > 0) {
				data->uuid = pilight_uuid;
			} else {
				data->uuid = NULL;
			}
			data->repeat = 1;
			eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
		}
	} else if(settings->state == CONNECTED) {
		settings->state = DISCONNECTED;

		struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
		if(data == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		snprintf(data->message, 1024, "{\"ip\":\"%s\",\"state\":\"disconnected\"}", a);
		strncpy(data->origin, "receiver", 255);
		data->protocol = pping->id;
		if(strlen(pilight_uuid) > 0) {
			data->uuid = pilight_uuid;
		} else {
			data->uuid = NULL;
		}
		data->repeat = 1;
		eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
	}
}

static void *thread(void *param) {
	uv_timer_t *timer_req = param;
	struct data_t *settings = timer_req->data;

	struct ping_list_t *iplist = NULL;

	if(settings->polling == 1) {
		logprintf(LOG_DEBUG, "ping is still searching for network device %s", settings->ip);
		return NULL;
	}

	settings->polling = 1;

	logprintf(LOG_DEBUG, "ping is starting search for network device %s", settings->ip);

	ping_add_host(&iplist, settings->ip);
	ping(iplist, callback);

	return (void *)NULL;
}

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	char *tmp = NULL;
	double itmp = 0;
	int match = 0;

	if(param == NULL) {
		return NULL;
	}

	if(!(pping->masterOnly == 0 || pilight.runmode == STANDALONE)) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(pping->id, jchild->string_) == 0) {
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

	node->interval = INTERVAL;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "ip", &tmp) == 0) {
				if((node->ip = MALLOC(strlen(tmp)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(node->ip, tmp);
				break;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		node->interval = (int)round(itmp);

	if(json_find_string(jdevice, "state", &tmp) == 0) {
		if(strcmp(tmp, "connected") == 0)	{
			node->state = CONNECTED;
		}
		if(strcmp(tmp, "disconnected") == 0) {
			node->state = DISCONNECTED;
		}
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jdevice->key);

	node->timer_req = NULL;
	node->next = data;
	data = node;

	if((node->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	node->timer_req->data = node;
	uv_timer_init(uv_default_loop(), node->timer_req);
	assert(node->interval > 0);
	uv_timer_start(node->timer_req, (void (*)(uv_timer_t *))thread, node->interval*1000, node->interval*1000);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->name);
		uv_timer_stop(tmp->timer_req);
		FREE(tmp->ip);
		data = data->next;
		FREE(tmp);
	}
}

static int checkValues(struct JsonNode *code) {
	double interval = INTERVAL;

	json_find_number(code, "poll-interval", &interval);

	if((int)round(interval) < INTERVAL) {
		logprintf(LOG_ERR, "ping poll-interval cannot be lower than %d", INTERVAL);
		return 1;
	}

	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void pingInit(void) {
	protocol_register(&pping);
	protocol_set_id(pping, "ping");
	protocol_device_add(pping, "ping", "Ping network devices");
	pping->devtype = PING;
	pping->hwtype = API;
	pping->multipleId = 0;
	pping->masterOnly = 1;

	options_add(&pping->options, "c", "connected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&pping->options, "d", "disconnected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&pping->options, "i", "ip", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	options_add(&pping->options, "0", "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	pping->gc=&gc;
	pping->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ping";
	module->version = "3.1";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	pingInit();
}
#endif
