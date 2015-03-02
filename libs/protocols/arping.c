/*
	Copyright (C) 2014 CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
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
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <ctype.h>
#include <pcap.h>
#ifdef _WIN32
	#include "pthread.h"
	#include "implement.h"
#else
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pthread.h>
#endif

#include "pilight.h"
#include "arp.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "json.h"
#include "gc.h"
#include "arping.h"

static unsigned short arping_loop = 1;
static unsigned short arping_threads = 0;

static pthread_mutex_t arpinglock;
static pthread_mutexattr_t arpingattr;

#define CONNECTED				1
#define DISCONNECTED 		0
#define INTERVAL				5

static void *arpingParse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct in_addr if_network;
	struct in_addr if_netmask;
	char *srcmac = NULL, tmpip[17], dstip[17], buf[17];
	char ip[17], *p = ip, *if_name = NULL;
	double itmp = 0.0;
	int state = 0, nrloops = 0, interval = INTERVAL, i = 0, srcip[4];

	arping_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "mac", &srcmac) == 0) {
				break;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);

	memset(dstip, '\0', 17);
	memset(tmpip, '\0', 17);

	for(i=0;i<strlen(srcmac);i++) {
		srcmac[i] = (char)tolower(srcmac[i]);
	}

	if((if_name = pcap_lookupdev(NULL)) == NULL) {
		logprintf(LOG_ERR, "could not determine default network interface");
		return NULL;
	}

	if(pcap_lookupnet(if_name, &if_network.s_addr, &if_netmask.s_addr, NULL) < 0) {
		logprintf(LOG_ERR, "could not determine host ip address");
		return NULL;
	}


	memset(&buf, '\0', 17);
	inet_ntop(AF_INET, (void *)&if_network, buf, 17);
	if(sscanf(buf, "%d.%d.%d.%d", &srcip[0], &srcip[1], &srcip[2], &srcip[3]) != 4) {
		logprintf(LOG_ERR, "could not extract ip address");
	}

	while(arping_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&arpinglock);
			if(strlen(dstip) == 0) {
				for(i=0;i<255;i++) {
					memset(ip, '\0', 17);
					snprintf(ip, sizeof(ip), "%d.%d.%d.%d", srcip[0], srcip[1], srcip[2], i);
					arp_add_host(ip);
				}
			} else {
				arp_add_host(dstip);
			}
			if(arp_resolv(if_name, srcmac, &p) == 0) {
				if(strlen(dstip) == 0) {
					strcpy(dstip, ip);
				}
				if(strcmp(dstip, ip) != 0) {
					memset(dstip, '\0', 17);
					logprintf(LOG_NOTICE, "ip address changed from %s to %s", dstip, ip);
					strcpy(dstip, ip);
				}
				if(state == DISCONNECTED) {
					state = CONNECTED;
					arping->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "mac", json_mkstring(srcmac));
					json_append_member(code, "ip", json_mkstring(ip));
					json_append_member(code, "state", json_mkstring("connected"));

					json_append_member(arping->message, "message", code);
					json_append_member(arping->message, "origin", json_mkstring("receiver"));
					json_append_member(arping->message, "protocol", json_mkstring(arping->id));

					if(pilight.broadcast != NULL) {
						pilight.broadcast(arping->id, arping->message);
					}
					json_delete(arping->message);
					arping->message = NULL;
				}
			} else if(state == CONNECTED) {
				state = DISCONNECTED;

				arping->message = json_mkobject();
				JsonNode *code = json_mkobject();
				json_append_member(code, "mac", json_mkstring(srcmac));
				json_append_member(code, "ip", json_mkstring("0.0.0.0"));
				json_append_member(code, "state", json_mkstring("disconnected"));

				json_append_member(arping->message, "message", code);
				json_append_member(arping->message, "origin", json_mkstring("receiver"));
				json_append_member(arping->message, "protocol", json_mkstring(arping->id));

				if(pilight.broadcast != NULL) {
					pilight.broadcast(arping->id, arping->message);
				}
				json_delete(arping->message);
				arping->message = NULL;
			}
			pthread_mutex_unlock(&arpinglock);
		}
	}
	pthread_mutex_unlock(&arpinglock);

	arping_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *arpingInitDev(JsonNode *jdevice) {
	arping_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	json_free(output);

	struct protocol_threads_t *node = protocol_thread_init(arping, json);
	return threads_register("arping", &arpingParse, (void *)node, 0);
}

static void arpingThreadGC(void) {
	arping_loop = 0;
	protocol_thread_stop(arping);
	while(arping_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(arping);
}

static int arpingCheckValues(JsonNode *code) {
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
	pthread_mutexattr_init(&arpingattr);
	pthread_mutexattr_settype(&arpingattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&arpinglock, &arpingattr);

	protocol_register(&arping);
	protocol_set_id(arping, "arping");
	protocol_device_add(arping, "arping", "Ping network devices");
	arping->devtype = PING;
	arping->hwtype = API;
	arping->multipleId = 0;

	options_add(&arping->options, 'c', "connected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arping->options, 'd', "disconnected", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arping->options, 'm', "mac", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}$");
	options_add(&arping->options, 'i', "ip", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	options_add(&arping->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	arping->initDev=&arpingInitDev;
	arping->threadGC=&arpingThreadGC;
	arping->checkValues=&arpingCheckValues;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "arping";
	module->version = "1.2";
	module->reqversion = "5.0";
	module->reqcommit = "187";
}

void init(void) {
	arpingInit();
}
#endif
