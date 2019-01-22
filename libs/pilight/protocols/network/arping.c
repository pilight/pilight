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
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#ifdef _WIN32
	#define WPCAP
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
#else
	#include <unistd.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <sys/wait.h>
	#include <net/if.h>
	#include <ifaddrs.h>
	#include <ctype.h>
#endif
#include <pcap.h>

#include "../../core/eventpool.h"
#include "../../core/pilight.h"
#include "../../core/arp.h"
#include "../../core/network.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/json.h"
#include "arping.h"

#define CONNECTED				1
#define DISCONNECTED 		0

typedef struct data_t {
	char *mac;
	char *name;

	int state;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void *arp_event(int reason, void *param, void *userdata) {
	struct reason_arp_device_t *data1 = param;

	struct data_t *tmp = data;
	while(tmp) {
		if(strcmp(tmp->mac, data1->mac) == 0) {
			struct reason_code_received_t *data2 = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			if(reason == REASON_ARP_LOST_DEVICE) {
				snprintf(data2->message, 1024, "{\"ip\":\"0.0.0.0\",\"mac\":\"%s\",\"state\":\"disconnected\"}", data1->mac);
			} else {
				snprintf(data2->message, 1024, "{\"ip\":\"%s\",\"mac\":\"%s\",\"state\":\"connected\"}", data1->ip, data1->mac);
			}
			strncpy(data2->origin, "receiver", 255);
			data2->protocol = arping->id;
			if(strlen(pilight_uuid) > 0) {
				data2->uuid = pilight_uuid;
			} else {
				data2->uuid = NULL;
			}
			data2->repeat = 1;
			eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data2);
		}
		tmp = tmp->next;
	}

	return NULL;
}

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	char *tmp = NULL;
	int match = 0;

	if(param == NULL) {
		return NULL;
	}

	if(!(arping->masterOnly == 0 || pilight.runmode == STANDALONE)) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(arping->id, jchild->string_) == 0) {
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
			if(json_find_string(jchild, "mac", &tmp) == 0) {
				if((node->mac = MALLOC(strlen(tmp)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(node->mac, tmp);
				break;
			}
			jchild = jchild->next;
		}
	}

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

	node->next = data;
	data = node;

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->mac);
		FREE(tmp->name);
		data = data->next;
		FREE(tmp);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void arpingInit(void) {
	protocol_register(&arping);
	protocol_set_id(arping, "arping");
	protocol_device_add(arping, "arping", "Ping network devices");
	arping->devtype = PING;
	arping->hwtype = API;
	arping->multipleId = 0;
	arping->masterOnly = 1;

	options_add(&arping->options, "c", "connected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arping->options, "d", "disconnected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arping->options, "m", "mac", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}$");
	options_add(&arping->options, "i", "ip", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	arping->gc=&gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);

	eventpool_callback(REASON_ARP_FOUND_DEVICE, arp_event, NULL);
	eventpool_callback(REASON_ARP_CHANGED_DEVICE, arp_event, NULL);
	eventpool_callback(REASON_ARP_LOST_DEVICE, arp_event, NULL);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "arping";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	arpingInit();
}
#endif
