/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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
	#define	IP_MAXPACKET	65535
#else
	#include <unistd.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <net/route.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <ifaddrs.h>
	#include <net/if.h>
	#include <netinet/ip.h>
	#include <netinet/tcp.h>
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
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <poll.h>

#include "mem.h"
#include "log.h"
#include "eventpool.h"

#include "../../libuv/uv.h"
#include "../storage/storage.h"

#define ARP_REQUEST 	1
#define ARP_REPLY 		2
#define ETHER_HDRLEN 	14         // IPv4 header length
#define ARP_PKTLEN 		28         // ARP packet length
#define ETH_ALEN 			6

typedef struct aap {
	char ip[INET_ADDRSTRLEN];
	char mac[18];
	unsigned long lastseen;
} aap;

typedef struct data_t {
	pcap_t *handle;
	int fd;
	int run;
	int stop;
	int which;
	int interval;
	int timeout;
	uv_poll_t *poll_req;
	uv_timer_t *timer_req;

	struct uv_custom_poll_t *custom_poll_data;

	unsigned char srcmac[ETH_ALEN];
	char srcip[INET_ADDRSTRLEN];

	struct {
		struct {
			char ip[INET_ADDRSTRLEN];
			int active;
		} *data;
		int nr;
		int iter;
	} search;

	struct {
		uv_mutex_t lock;
		struct {
			char ip[INET_ADDRSTRLEN];
			char mac[18];
			unsigned long lastseen;
		} *data;
		int nr;
		int iter;
	} found;
} data_t;

static struct data_t **data;
static int nrdata = 0;

typedef struct ether_hdr {
	uint8_t dstmac[ETH_ALEN];
	uint8_t srcmac[ETH_ALEN];
	uint16_t ftype;
} ether_hdr;

typedef struct arphdr {
	u_int16_t htype;    /* Hardware Type           */
	u_int16_t ptype;    /* Protocol Type           */
	u_char hlen;        /* Hardware Address Length */
	u_char plen;        /* Protocol Address Length */
	u_int16_t oper;     /* Operation Code          */
	u_char sha[6];      /* Sender hardware address */
	u_char spa[4];      /* Sender IP address       */
	u_char tha[6];      /* Target hardware address */
	u_char tpa[4];      /* Target IP address       */
} arphdr_t;

static void *reason_arp_device_free(void *param) {
	struct reason_arp_device_t *data = param;
	FREE(data);
	return NULL;
}

static void initpacket(char *dstip, char *srcip, unsigned char srcmac[6], char *packet) {
	struct ether_hdr *hdr = (struct ether_hdr *)(packet);
	struct arphdr *pkt = (struct arphdr *)(packet + sizeof(struct ether_hdr));

	memset(hdr->dstmac, 0xff, ETH_ALEN);
	memcpy(hdr->srcmac, srcmac, ETH_ALEN);

	hdr->ftype = htons(0x0806);

	pkt->htype = htons(1);
	pkt->ptype = htons(0x0800);
	pkt->hlen = 6;
	pkt->plen = 4;
	pkt->oper = htons(ARP_REQUEST);
	memcpy(pkt->sha, srcmac, ETH_ALEN);
	memset(pkt->tha, 0x00, ETH_ALEN);

  inet_pton(AF_INET, srcip, pkt->spa);
  inet_pton(AF_INET, dstip, pkt->tpa);
}

static void restart(uv_timer_t *req) {
	struct data_t *data = req->data;
	int i = 0, x = 0;
	int now = time(NULL);

	data->run++;

	if(data->run >= 6) {
		data->which = 0;
		data->run = 0;
	} else {
		data->which = 1;
	}
	if(data->found.nr == 0) {
		data->which = 0;
	}

	for(i=0;i<data->found.nr;i++) {
		if((now-data->found.data[i].lastseen) > (data->interval*data->timeout)) {
			uv_mutex_lock(&data->found.lock);
			for(x=0;x<data->search.nr;x++) {
				if(strcmp(data->search.data[x].ip, data->found.data[i].ip) == 0) {
					/*
					 * Activate search for this device on the unknown list
					 */
					data->search.data[x].active = 1;
				}
			}

			struct reason_arp_device_t *data1 = MALLOC(sizeof(struct reason_arp_device_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(data1->ip, data->found.data[i].ip);
			strcpy(data1->mac, data->found.data[i].mac);
			eventpool_trigger(REASON_ARP_LOST_DEVICE, reason_arp_device_free, data1);

			for(x=i;x<data->found.nr-1;x++) {
				data->found.data[x] = data->found.data[x+1];
			}
			data->found.nr--;
			uv_mutex_unlock(&data->found.lock);
			i--;
			continue;
		}
	}

	if(data->stop == 0) {
		uv_custom_write(data->poll_req);
	}
}

static void write_cb(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;

	char *ip = NULL;
	int one = 1;
	int *iter = NULL;
	int *nr = NULL;
	int *active = &one;
	if(data->which == 0) {
		iter = &data->search.iter;
		nr = &data->search.nr;
		ip = data->search.data[*iter].ip;
		active = &data->search.data[*iter].active;
	}
	if(data->which == 1) {
		iter = &data->found.iter;
		nr = &data->found.nr;
		ip = data->found.data[*iter].ip;
		active = &one;
	}

	if(*active == 1) {
		unsigned char packet[60];
		memset(&packet, '\0', 60);

		initpacket(ip, data->srcip, data->srcmac, (char *)&packet);
		pcap_sendpacket(data->handle, packet, ETHER_HDRLEN + ARP_PKTLEN);
	}

	if(data->stop == 0) {
		if(++(*iter) >= *nr) {
			*iter = 0;
			uv_timer_start(data->timer_req, (void (*)(uv_timer_t *))restart, data->interval*1000, 0);
		} else {
			uv_custom_write(data->poll_req);
		}
	}
}

static void callback(u_char *user, const struct pcap_pkthdr *pkt_header, const u_char *bytes) {
	struct data_t *data = (void *)user;

	arphdr_t *arpheader = NULL;
	char ip[INET_ADDRSTRLEN], mac[18];
	int i = 0;

	memset(ip, 0, INET_ADDRSTRLEN);
	memset(mac, 0, 18);

	arpheader = (struct arphdr *)(bytes+14);

	if((ntohs(arpheader->oper) == ARP_REPLY) &&
	   (pkt_header->len == 60) &&
		 ntohs(arpheader->ptype) == 0x0800 &&
		 ntohs(arpheader->htype) == 1) {

		sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
			arpheader->sha[0], arpheader->sha[1], arpheader->sha[2],
			arpheader->sha[3], arpheader->sha[4], arpheader->sha[5]
		);

		sprintf(ip, "%d.%d.%d.%d",
			arpheader->spa[0], arpheader->spa[1], arpheader->spa[2], arpheader->spa[3]
		);
		for(i=0;i<data->found.nr;i++) {
			if(strcmp(data->found.data[i].ip, ip) == 0) {
				/*
				 * IP already known, update last seen timestamp
				 */
				if(strcmp(data->found.data[i].mac, mac) != 0) {
					struct reason_arp_device_t *data1 = MALLOC(sizeof(struct reason_arp_device_t));
					if(data1 == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					strcpy(data1->ip, ip);
					strcpy(data1->mac, mac);
					eventpool_trigger(REASON_ARP_CHANGED_DEVICE, reason_arp_device_free, data1);

					struct reason_arp_device_t *data2 = MALLOC(sizeof(struct reason_arp_device_t));
					if(data2 == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					strcpy(data2->ip, ip);
					strcpy(data2->mac, data->found.data[i].mac);
					eventpool_trigger(REASON_ARP_LOST_DEVICE, reason_arp_device_free, data2);
				}
				strcpy(data->found.data[i].mac, mac);
				data->found.data[i].lastseen = time(NULL);

				return;
			}
		}

		uv_mutex_lock(&data->found.lock);
		if((data->found.data = REALLOC(data->found.data, sizeof(*data->found.data)*(data->found.nr+1))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(data->found.data[data->found.nr].mac, mac);
		strcpy(data->found.data[data->found.nr].ip, ip);
		data->found.data[data->found.nr].lastseen = time(NULL);

		struct reason_arp_device_t *data1 = MALLOC(sizeof(struct reason_arp_device_t));
		if(data1 == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(data1->ip, ip);
		strcpy(data1->mac, mac);
		eventpool_trigger(REASON_ARP_FOUND_DEVICE, reason_arp_device_free, data1);

		data->found.nr++;
		uv_mutex_unlock(&data->found.lock);

		for(i=0;i<data->search.nr;i++) {
			if(strcmp(data->search.data[i].ip, ip) == 0) {
				/*
				 * Deactive search for this device on the unknown list
				 */
				data->search.data[i].active = 0;
			}
		}
	}
}

static void read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;

	pcap_dispatch(data->handle, 1, callback, (void *)data);

	if(data->stop == 0) {
		uv_custom_read(data->poll_req);
	}
}

static unsigned int ipdiff(struct in_addr *min, struct in_addr *max) {
	return
		(((unsigned char *)&(*max).s_addr)[0]-((unsigned char *)&(*min).s_addr)[0])+
		(((unsigned char *)&(*max).s_addr)[1]-((unsigned char *)&(*min).s_addr)[1])+
		(((unsigned char *)&(*max).s_addr)[2]-((unsigned char *)&(*min).s_addr)[2])+
		(((unsigned char *)&(*max).s_addr)[3]-((unsigned char *)&(*min).s_addr)[3]);
}

static void free_data(struct data_t *data) {
	if(data->custom_poll_data != NULL) {
		uv_custom_poll_free(data->custom_poll_data);
	}
	if(data->search.data != NULL) {
		FREE(data->search.data);
	}
	if(data->found.data != NULL) {
		FREE(data->found.data);
	}
	if(data->handle != NULL) {
		pcap_close(data->handle);
	}
	FREE(data);
}

void arp_stop(void) {
	int i = 0;
	for(i=0;i<nrdata;i++) {
		data[i]->stop = 1;
		uv_timer_stop(data[i]->timer_req);
		uv_poll_stop(data[i]->poll_req);
	}
}

int arp_gc(void) {
	int i = 0;
	for(i=0;i<nrdata;i++) {
		free_data(data[i]);
	}
	FREE(data);
	nrdata = 0;
	return 0;
}

void arp_scan(void) {
	char error[PCAP_ERRBUF_SIZE], *e = error;
	char ip[17], netmask[17], min[17], max[17];
	int count = 0, i = 0, x = 0, has_mac = 0;
	double itmp = 0.0;
	uv_interface_address_t *interfaces = NULL;
	struct in_addr in_ip, in_netmask, in_min, in_max;

	int err = uv_interface_addresses(&interfaces, &count);
	if(err != 0) {
		/* LCOV_EXCL_START */
		logprintf(LOG_ERR, "uv_interface_addresses: %s", uv_strerror(err));
		return;
		/* LCOV_EXCL_STOP */
	}

	memset(&error, 0, PCAP_ERRBUF_SIZE);

	for(i=0;i<count;i++) {
		if(interfaces[i].is_internal == 0 &&
		   interfaces[i].address.address4.sin_family == AF_INET) {

			has_mac = 0;
			for(x=0;x<6;x++) {
				if((interfaces[i].phys_addr[x] & 0xFF) != 0x00) {
					has_mac = 1;
					break;
				}
			}
			if(has_mac == 0) {
				continue; /*LCOV_EXCL_LINE*/
			}

			if((data = REALLOC(data, sizeof(struct data_t *) * (nrdata+1))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			if((data[nrdata] = MALLOC(sizeof(struct data_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			memset(data[nrdata], '\0', sizeof(struct data_t));

			uv_ip4_name(&interfaces[i].address.address4, ip, sizeof(ip));
			uv_ip4_name(&interfaces[i].netmask.netmask4, netmask, sizeof(netmask));

			uv_inet_pton(AF_INET, ip, &in_ip);
			uv_inet_pton(AF_INET, netmask, &in_netmask);
			in_min.s_addr = in_ip.s_addr & in_netmask.s_addr;
			in_max.s_addr = in_ip.s_addr | ~in_netmask.s_addr;

			uv_inet_ntop(AF_INET, &in_min, min, sizeof(min));
			uv_inet_ntop(AF_INET, &in_max, max, sizeof(max));

			memcpy(data[nrdata]->srcmac, interfaces[i].phys_addr, ETH_ALEN);
			memcpy(data[nrdata]->srcip, ip, INET_ADDRSTRLEN);

			unsigned int nrip = ipdiff(&in_min, &in_max);
			if((data[nrdata]->search.data = MALLOC(sizeof((*data[nrdata]->search.data))*(nrip+1))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			data[nrdata]->search.nr = nrip;
			data[nrdata]->search.iter = 0;

			int a = 0, b = 0, c = 0, d = 0, z = 0;
			for(a=((unsigned char *)&in_min.s_addr)[0];a<=((unsigned char *)&in_max.s_addr)[0];a++) {
				for(b=((unsigned char *)&in_min.s_addr)[1];b<=((unsigned char *)&in_max.s_addr)[1];b++) {
					for(c=((unsigned char *)&in_min.s_addr)[2];c<=((unsigned char *)&in_max.s_addr)[2];c++) {
						for(d=((unsigned char *)&in_min.s_addr)[3];d<=((unsigned char *)&in_max.s_addr)[3];d++) {
							struct in_addr in_tmp;
							char tmp[17];

							((unsigned char *)&in_tmp.s_addr)[0] = a;
							((unsigned char *)&in_tmp.s_addr)[1] = b;
							((unsigned char *)&in_tmp.s_addr)[2] = c;
							((unsigned char *)&in_tmp.s_addr)[3] = d;

							uv_inet_ntop(AF_INET, &in_tmp, tmp, sizeof(tmp));

							strcpy(data[nrdata]->search.data[z].ip, tmp);
							data[nrdata]->search.data[z].active = 1;
							z++;
						}
					}
				}
			}

			/*
			 * In order to unit test this code we need to
			 * search for an ip address that is probably not
			 * in use. So, besides 10.0.0.138 we also
			 * search for 10.0.0.1, but only if the last bit
			 * of the netmask is greater then zero.
			 */
			if(((unsigned char *)&in_netmask.s_addr)[3] > 0 && ((unsigned char *)&in_min.s_addr)[3] > 0) {
				if((data[nrdata]->search.data = REALLOC(data[nrdata]->search.data, sizeof((*data[nrdata]->search.data))*(nrip+2))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				data[nrdata]->search.nr = nrip+1;

				struct in_addr *in_tmp = &in_min;
				char tmp[17];

				((unsigned char *)&(*in_tmp).s_addr)[3] = 1;

				uv_inet_ntop(AF_INET, in_tmp, tmp, sizeof(tmp));

				strcpy(data[nrdata]->search.data[z].ip, tmp);
				data[nrdata]->search.data[z].active = 1;
				z++;
			}

			if((data[nrdata]->handle = pcap_open_live(interfaces[i].name, 64, 0, 1, e)) == NULL) {
				/* LCOV_EXCL_START */
				logprintf(LOG_ERR, "pcap_open_live: %s", e);
				free_data(data[nrdata]);
				continue;
				/* LCOV_EXCL_STOP */
			}

			if((pcap_setnonblock(data[nrdata]->handle, 1, e)) != 0) {
				/* LCOV_EXCL_START */
				logprintf(LOG_ERR, "pcap_setnonblock: %s", e);
				free_data(data[nrdata]);
				continue;
				/* LCOV_EXCL_STOP */
			}

#ifdef _WIN32
			data[nrdata]->fd = pcap_fileno(data[nrdata]->handle);
#else
			data[nrdata]->fd = pcap_get_selectable_fd(data[nrdata]->handle);
#endif

			if((data[nrdata]->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_timer_init(uv_default_loop(), data[nrdata]->timer_req);

			data[nrdata]->interval = 10;
			data[nrdata]->timeout = 3;
			if(settings_select_number(ORIGIN_WEBSERVER, "arp-interval", &itmp) == 0) { data[nrdata]->interval = (int)itmp; }
			if(settings_select_number(ORIGIN_WEBSERVER, "arp-timeout", &itmp) == 0) { data[nrdata]->timeout = (int)itmp; }

			data[nrdata]->timer_req->data = data[nrdata];

			data[nrdata]->poll_req = NULL;
			if((data[nrdata]->poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_mutex_init(&data[nrdata]->found.lock);

			uv_custom_poll_init(&data[nrdata]->custom_poll_data, data[nrdata]->poll_req, (void *)data[nrdata]);
			data[nrdata]->custom_poll_data->is_ssl = 0;
			data[nrdata]->custom_poll_data->is_udp = 0;
			data[nrdata]->custom_poll_data->custom_recv = 1;
			data[nrdata]->custom_poll_data->write_cb = write_cb;
			data[nrdata]->custom_poll_data->read_cb = read_cb;
			data[nrdata]->custom_poll_data->close_cb = NULL;

			int r = uv_poll_init_socket(uv_default_loop(), data[nrdata]->poll_req, data[nrdata]->fd);
			if(r != 0) {
				/* LCOV_EXCL_START */
				logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
				free_data(data[nrdata]);
				continue;
				/* LCOV_EXCL_STOP */
			}

			uv_custom_write(data[nrdata]->poll_req);
			uv_custom_read(data[nrdata]->poll_req);

			nrdata++;
		}
	}

	uv_free_interface_addresses(interfaces, count);
}
