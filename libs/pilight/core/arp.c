/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#ifdef _WIN32
	#define WPCAP
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <windows.h>
	#include <psapi.h>
	#include <tlhelp32.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
	#define MSG_NOSIGNAL 0
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/if_ether.h>
	#include <netinet/tcp.h>
	#include <net/route.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <ifaddrs.h>
	#include <net/if.h>
	#include <netinet/ip.h>
	#include <netinet/tcp.h>
	#include <netinet/if_ether.h>
	#ifdef __FreeBSD__
		#include <net/if_dl.h>
		#include <net/if_types.h>
	#else
		#include <sys/ioctl.h>
	#endif
	#include <regex.h>
	#include <netdb.h>
#endif
#include <pcap.h>
#include <poll.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include "eventpool.h"
#include "network.h"
#include "mem.h"
#include "log.h"
#include "arp.h"

#define ETHER_HDRLEN 	14         // IPv4 header length
#define ARP_PKTLEN 		28         // ARP packet length
#define ETH_ALEN 			6
#define BUFFER 				32

typedef struct data_t {
	pcap_t *handle;
	int buffer;
	int nrips;
	int called;
	int tries;
	unsigned long stop;
	struct arp_list_t *iplist;
	struct arp_list_t *nodes;
	unsigned char srcmac[ETH_ALEN];
	char srcip[INET_ADDRSTRLEN];
	char dstmac[19];
	void (*callback)(char *, char *);
	struct eventpool_fd_t *server;
} data_t;

typedef struct ether_hdr {
	uint8_t dstmac[ETH_ALEN];
	uint8_t srcmac[ETH_ALEN];
	uint16_t ftype;
} ether_hdr;

typedef struct arp_pkt {
	uint16_t hwtype;
	uint16_t ptype;
	uint8_t hwsize;
	uint8_t psize;
	uint16_t opcode;
	unsigned char srcmac[ETH_ALEN];
	unsigned char srcip[4];
	unsigned char dstmac[ETH_ALEN];
	unsigned char dstip[4];
} arp_pkt;

void arp_add_host(struct arp_list_t **iplist, const char *ip) {
	struct arp_list_t *node = MALLOC(sizeof(struct arp_list_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	memset(node, '\0', sizeof(struct arp_list_t));
	strncpy(node->dstip, ip, INET_ADDRSTRLEN);
	node->tries = 0;
	node->time = 0;
	node->next = NULL;
	node->timeout = 250;

	node->next = *iplist;
	*iplist = node;
}

static void initpacket(char *dstip, char *srcip, unsigned char srcmac[6], char *packet) {
	struct ether_hdr hdr;
	struct arp_pkt pkt;
	unsigned char target_mac1[ETH_ALEN];
	unsigned char target_mac2[ETH_ALEN];

	memset(&hdr, 0, sizeof(struct ether_hdr));
	memset(&pkt, 0, sizeof(struct arp_pkt));

	memset(&target_mac1, 0xFF, ETH_ALEN);
	memset(&target_mac2, 0x00, ETH_ALEN);

	memcpy(hdr.dstmac, target_mac1, ETH_ALEN);
	memcpy(hdr.srcmac, srcmac, ETH_ALEN);

	hdr.ftype = htons(0x0806);

	pkt.hwtype = htons(1);
	pkt.ptype = htons(0x0800);
	pkt.hwsize = 6;
	pkt.psize = 4;
	pkt.opcode = htons(1);
	memcpy(pkt.srcmac, srcmac, ETH_ALEN);
	memcpy(pkt.dstmac, target_mac2, ETH_ALEN);

  inet_pton(AF_INET, srcip, pkt.srcip);
  inet_pton(AF_INET, dstip, pkt.dstip);

  memcpy(&packet[0], &hdr, ETHER_HDRLEN);
  memcpy(&packet[ETHER_HDRLEN], &pkt, ARP_PKTLEN);
}

static void arp_data_gc(struct data_t *data) {
	pcap_t *pcap_handle = data->handle;
	struct arp_list_t *tmp = NULL;
	while(data->iplist) {
		tmp = data->iplist;
		data->iplist = data->iplist->next;
		FREE(tmp);
	}
	if(data->iplist != NULL) {
		FREE(data->iplist);
	}
	FREE(data);
	data = NULL;
	pcap_close(pcap_handle);
}

static void *arp_timeout(void *param) {
	struct threadpool_tasks_t *node = param;
	struct data_t *data = node->userdata;
	struct arp_list_t *tmp = data->iplist;
	struct timeval tv;
	unsigned long now = 0;
	int match = 0, match1 = 0;

	if(data == NULL) {
		return NULL;
	}

	gettimeofday(&tv, NULL);
	now = (1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec);

	while(tmp) {
		match1++;
		if(((now-tmp->time) > 1000000 && tmp->time > 0) || tmp->time == 0) {
			match++;
		}
		tmp = tmp->next;
	}

	if(match1 == match) {
		if(data->callback != NULL && data->called == 0) {
			data->called = 1;
			data->callback(data->dstmac, NULL);
		}
		data->server->stage = EVENTPOOL_STAGE_DISCONNECT;
		data->server->dowrite = 0;
		data->server->doflush = 0;
		return NULL;
	}	else if(data->stop == 0) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		threadpool_add_scheduled_work("aap", arp_timeout, tv, data);
	}
	if(data->stop == 1) {
		data->server->stage = EVENTPOOL_STAGE_DISCONNECT;
		data->server->dowrite = 0;
		data->server->doflush = 0;
		return NULL;
	}

	if(data->buffer > BUFFER || data->nrips == data->buffer) {
		eventpool_fd_enable_write(data->server);
		data->buffer = 0;
	}
	
	return NULL;
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	struct data_t *data = node->userdata;
	struct arp_list_t *tmp = NULL;
	struct timeval tv;
	pcap_t *pcap_handle = data->handle;
	unsigned long now = 0;
	int loop = 0;

	gettimeofday(&tv, NULL);
	now = (1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec);

	switch(event) {
		case EV_CONNECT_SUCCESS: {
			eventpool_fd_enable_write(node);
			eventpool_fd_enable_read(node);
		} break;
		case EV_WRITE: {
			unsigned char packet[IP_MAXPACKET];
			if(data->stop == 1) {
				return 0;
			}
			if(data->nodes == NULL) {
				data->nodes = data->iplist;
			}

			/* Sent the individual arp request in increasing intervals */
			if((data->nodes->time == 0 || (data->nodes->time-now) > data->nodes->timeout) && data->nodes->tries <= data->tries) {
				data->nodes->tries++;
				data->buffer++;
				data->nodes->timeout *= 1.5;

				memset(&packet, '\0', IP_MAXPACKET);
				initpacket(data->nodes->dstip, data->srcip, data->srcmac, (char *)&packet);
				pcap_sendpacket(pcap_handle, packet, ETHER_HDRLEN + ARP_PKTLEN);

				if(now > 0) {
					data->nodes->time = now;

					tmp = data->nodes;
					while(tmp) {
						tmp->time = now+tmp->timeout;
						tmp = tmp->next;
					}
				}
			}
			data->nodes = data->nodes->next;

			/* Only allows BUFFER bulk request at one time to save CPU resources */
			if(data->buffer <= BUFFER) {
				eventpool_fd_enable_write(node);
			}
		} break;
		case EV_READ: {
			struct pcap_pkthdr *pkt_header = NULL;
			struct ether_hdr hdr;
			struct arp_pkt pkt;
			const u_char *packet = NULL;
			char src[INET_ADDRSTRLEN+1], dst[INET_ADDRSTRLEN+1];
			char srcmac[19], dstmac[19];

			if(pcap_next_ex(pcap_handle, &pkt_header, &packet) == 1) {

				memset(&src, '\0', INET_ADDRSTRLEN+1);
				memset(&dst, '\0', INET_ADDRSTRLEN+1);

				memset(&srcmac, '\0', 19);
				memset(&dstmac, '\0', 19);

				memcpy(&hdr, packet, ETHER_HDRLEN);
				memcpy(&pkt, packet + ETHER_HDRLEN, ARP_PKTLEN);

				if(ntohs(pkt.opcode) == 2 && ntohs(pkt.ptype) == 0x0800 && ntohs(hdr.ftype) == 0x0806) {
					snprintf(src, INET_ADDRSTRLEN, "%d.%d.%d.%d", pkt.srcip[0], pkt.srcip[1], pkt.srcip[2], pkt.srcip[3]);
					snprintf(dst, INET_ADDRSTRLEN, "%d.%d.%d.%d", pkt.dstip[0], pkt.dstip[1], pkt.dstip[2], pkt.dstip[3]);
					snprintf(srcmac, 18, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
																	pkt.srcmac[0], pkt.srcmac[1],
																	pkt.srcmac[2], pkt.srcmac[3],
																	pkt.srcmac[4], pkt.srcmac[5]);
					snprintf(dstmac, 18, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
																	pkt.dstmac[0], pkt.dstmac[1],
																	pkt.dstmac[2], pkt.dstmac[3],
																	pkt.dstmac[4], pkt.dstmac[5]);

					if(pilight.debuglevel == 3) {
						fprintf(stderr, "%s %s %s %s\n", src, dst, srcmac, dstmac);
					}
					if(strcmp(srcmac, data->dstmac) == 0) {
						if(data->callback != NULL && data->called == 0) {
							data->called = 1;
							data->stop = 1;
							data->callback(data->dstmac, src);
						}
						return 0;
					}
				}
			}
			tmp = data->iplist;
			while(tmp) {
				if(tmp->tries < data->tries) {
					loop++;
				}
				tmp = tmp->next;
			}
			if(loop == 0) {
				data->stop = 1;
				return 0;
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_DISCONNECTED: {
			node->stage = EVENTPOOL_STAGE_DISCONNECTED;
			arp_data_gc(data);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

/*
 * Use 20 repeats for a single IP
 * Use 5 repeats for a ip range
 */
int arp_resolv(struct arp_list_t *iplist, char *if_name, char *srcmac, char *dstmac, int tries, void (*callback)(char *, char *)) {
	if(iplist == NULL) {
		return -1;
	}
	pcap_t *pcap_handle = NULL;
	char *if_cpy = NULL, error[PCAP_ERRBUF_SIZE];
	char *p = NULL, *e = error;
	int fd = 0;

	if((if_cpy = MALLOC(strlen(if_name)+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(if_cpy, if_name);

#ifdef _WIN32
	int match = 0;
	pcap_if_t *alldevs = NULL, *d = NULL;

	if(pcap_findalldevs(&alldevs, e) == -1){
		logprintf(LOG_ERR, "pcap_findalldevs: %s", e);
		FREE(if_cpy);
		return -1;
	}

	d = alldevs;
	match = 0;
	for(d=alldevs;d;d=d->next) {
		if(strstr(d->name, if_cpy) != NULL) {
			match = 1;
			if((if_cpy = REALLOC(if_cpy, strlen(d->name)+1)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			strcpy(if_cpy, d->name);
			break;
		}
	}
	if(alldevs != NULL) {
		pcap_freealldevs(alldevs);
	}
	if(match == 0) {
		logprintf(LOG_ERR, "could not full interface name for %s", if_name);
		FREE(if_cpy);
		return -1;
	}
#endif

	if((pcap_handle = pcap_open_live(if_cpy, 64, 0, 3, e)) == NULL) {
		logprintf(LOG_ERR, "pcap_open_live: %s", e);
		FREE(if_cpy);
		return -1;
	}

	if((pcap_setnonblock(pcap_handle, 1, e)) < 0) {
		logprintf(LOG_ERR, "pcap_setnonblock: %s", e);
		FREE(if_cpy);
		if(pcap_handle != NULL) {
			pcap_close(pcap_handle);
		}
		return -1;
	}

	struct timeval tv;
	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	memset(data, '\0', sizeof(struct data_t));
	data->handle = pcap_handle;
	data->callback = callback;
	data->buffer = 0;
	data->nrips = 0;
	data->called = 0;
	data->tries = tries;
	data->stop = 0;
	data->nodes = NULL;
	data->iplist = iplist;

	struct arp_list_t *tmp = data->iplist;
	while(tmp) {
		data->nrips++;
		tmp = tmp->next;
	}
	strcpy(data->dstmac, dstmac);
	memcpy(data->srcmac, srcmac, ETH_ALEN);
	p = data->srcip;
	dev2ip(if_name, &p, AF_INET);

	fd = pcap_get_selectable_fd(pcap_handle);

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	data->server = eventpool_fd_add("arp", fd, client_callback, NULL, data);
	threadpool_add_scheduled_work("arp", arp_timeout, tv, data);

	FREE(if_cpy);

	return 0;
}
