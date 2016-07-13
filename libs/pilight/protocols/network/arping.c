/*
	Copyright (C) 2014 - 2016 CurlyMo

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
#include <math.h>
#ifdef _WIN32
	#define WPCAP
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
#else
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

#include "../../core/threadpool.h"
#include "../../core/eventpool.h"
#include "../../core/pilight.h"
#include "../../core/arp.h"
#include "../../core/network.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "arping.h"

#define CONNECTED				1
#define DISCONNECTED 		0
#define INTERVAL				5

static int iprange = 0;
static int nrips = 256;

typedef struct data_t {
	char *name;
	char *dstmac;
	char srcmac[ETH_ALEN];
	int srcip[4];
	char dstip[INET_ADDRSTRLEN+1];
	char **devs;
	char ip[INET_ADDRSTRLEN+1];

	int polling;
	int nrdevs;
	int interval;
	int state;
	// int lastrange;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void callback(char *a, char *b) {
	struct data_t *settings = data;
	while(settings) {
		if(strcmp(settings->dstmac, a) == 0) {
			break;
		}
		settings = settings->next;
	}

	settings->polling = 0;

	if(b != NULL && strcmp(b, "0.0.0.0") != 0) {
		if(strlen(settings->dstip) > 0 && strcmp(settings->dstip, b) != 0) {
			memset(settings->dstip, '\0', INET_ADDRSTRLEN+1);
			logprintf(LOG_NOTICE, "arping network device %s changed ip from %s to %s", settings->dstmac, settings->dstip, b);
		}
		strcpy(settings->dstip, b);

		logprintf(LOG_DEBUG, "arping found network device %s at %s", settings->dstmac, b);
		if(settings->state == DISCONNECTED) {
			settings->state = CONNECTED;
			struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY
			}
			snprintf(data->message, 1024, "{\"mac\":\"%s\",\"ip\":\"%s\",\"state\":\"connected\"}", a, b);
			strcpy(data->origin, "receiver");
			data->protocol = arping->id;
			if(strlen(pilight_uuid) > 0) {
				data->uuid = pilight_uuid;
			} else {
				data->uuid = NULL;
			}
			data->repeat = 1;
			eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

			// iprange -= nrips;
			// settings->lastrange = iprange;
		}
	} else {
		logprintf(LOG_DEBUG, "arping did not find network device %s", settings->dstmac);
		if(settings->state == CONNECTED) {
			settings->state = DISCONNECTED;

			struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY
			}
			snprintf(data->message, 1024, "{\"mac\":\"%s\",\"ip\":\"0.0.0.0\",\"state\":\"disconnected\"}", a);
			strcpy(data->origin, "receiver");
			data->protocol = arping->id;
			if(strlen(pilight_uuid) > 0) {
				data->uuid = pilight_uuid;
			} else {
				data->uuid = NULL;
			}
			data->repeat = 1;
			eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

			memset(&settings->dstip, '\0', INET_ADDRSTRLEN+1);
		}
	}
	iprange = 0;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;
	struct arp_list_t *iplist = NULL;
	struct timeval tv;
	int i = 0, tries = 0;

	tv.tv_sec = settings->interval;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);

	if(settings->polling == 1) {
		logprintf(LOG_DEBUG, "arping is still searching for network device %s", settings->dstmac);
		return NULL;
	}

	// strcpy(settings->dstip, "10.0.0.141");
	if(strlen(settings->dstip) == 0) {
		logprintf(LOG_DEBUG, "arping is starting search for network device %s in iprange %d.%d.%d.%d to %d.%d.%d.%d",
				settings->dstmac,
				settings->srcip[0], settings->srcip[1], settings->srcip[2], iprange,
				settings->srcip[0], settings->srcip[1], settings->srcip[2], iprange+(nrips-1));
		for(i=iprange;i<iprange+nrips;i++) {
			memset(settings->ip, '\0', INET_ADDRSTRLEN+1);
			snprintf(settings->ip, sizeof(settings->ip), "%d.%d.%d.%d",
				settings->srcip[0], settings->srcip[1], settings->srcip[2], i);
			arp_add_host(&iplist, settings->ip);
			tries = 5;
		}
		iprange += nrips;
		if(iprange > 255) {
			iprange = 0;
		}
	} else {
		logprintf(LOG_DEBUG, "arping is starting search for network device %s at %s", settings->dstmac, settings->dstip);
		arp_add_host(&iplist, settings->dstip);
		tries = 20;
	}

	settings->polling = 1;

	// srcip = search ip
	// srcmac = source mac

	arp_resolv(iplist, settings->devs[0], settings->srcmac, settings->dstmac, tries, callback);

	return (void *)NULL;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	char *tmp = NULL, *p = NULL;
	double itmp = 0;
	int match = 0, i = 0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if(!(arping->masterOnly == 0 || pilight.runmode == STANDALONE)) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
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
		OUT_OF_MEMORY
	}
	memset(node, '\0', sizeof(struct data_t));

	node->interval = INTERVAL;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "mac", &tmp) == 0) {
				if((node->dstmac = MALLOC(strlen(tmp)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				memset(node->dstmac, '\0', strlen(tmp)+1);
				for(i=0;i<=strlen(node->dstmac);i++) {
					if(isNumeric(&tmp[i]) != 0) {
						node->dstmac[i] = (char)tolower(tmp[i]);
					} else {
						node->dstmac[i] = tmp[i];
					}
				}
				break;
			}
			jchild = jchild->next;
		}
	}


	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		node->interval = (int)round(itmp);

	if((node->nrdevs = inetdevs(&node->devs)) == 0) {
		logprintf(LOG_ERR, "could not determine default network interface");
		array_free(&node->devs, node->nrdevs);
		FREE(node->dstmac);
		FREE(node);
		return NULL;
	}

	p = node->ip;
	memset(&node->ip, '\0', INET_ADDRSTRLEN+1);
	if(dev2ip(node->devs[0], &p, AF_INET) != 0) {
		logprintf(LOG_ERR, "could not determine host ip address");

		array_free(&node->devs, node->nrdevs);
		FREE(node->dstmac);
		FREE(node);
		return NULL;
	}

	p = node->srcmac;
	memset(&node->srcmac, '\0', ETH_ALEN);
	if(dev2mac(node->devs[0], &p) != 0 || (node->srcmac[0] == 0 && node->srcmac[1] == 0 &&
		node->srcmac[2] == 0 && node->srcmac[3] == 0 &&
		node->srcmac[4] == 0 && node->srcmac[5] == 0)) {
		logprintf(LOG_ERR, "could not obtain MAC address for interface %s", node->devs[0]);

		array_free(&node->devs, node->nrdevs);
		FREE(node->dstmac);
		FREE(node);
		return NULL;
	}

	if(sscanf(node->ip, "%d.%d.%d.%d", &node->srcip[0], &node->srcip[1], &node->srcip[2], &node->srcip[3]) != 4) {
		logprintf(LOG_ERR, "could not extract ip address");

		array_free(&node->devs, node->nrdevs);
		FREE(node->dstmac);
		FREE(node);
		return NULL;
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, jdevice->key);

	node->next = data;
	data = node;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->dstmac);
		FREE(tmp->name);
		array_free(&tmp->devs, tmp->nrdevs);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}

static int checkValues(JsonNode *code) {
	double interval = INTERVAL;

	json_find_number(code, "poll-interval", &interval);

	if((int)round(interval) < INTERVAL) {
		logprintf(LOG_ERR, "arping poll-interval cannot be lower than %d", INTERVAL);
		return 1;
	}

	return 0;
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

	options_add(&arping->options, 'c', "connected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arping->options, 'd', "disconnected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arping->options, 'm', "mac", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}$");
	options_add(&arping->options, 'i', "ip", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	options_add(&arping->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	// arping->initDev=&initDev;
	arping->gc=&gc;
	arping->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
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
