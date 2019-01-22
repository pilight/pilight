/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <sys/poll.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
#endif
#include <pcap.h>
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

#include "../libs/pilight/core/arp.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/json.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/network/ping.h"

#include "alltests.h"

#define ARP_REQUEST 	1
#define ARP_REPLY 		2
#define ETHER_HDRLEN 	14         // IPv4 header length
#define ARP_PKTLEN 		28         // ARP packet length
#define ETH_ALEN 			6

typedef struct data_t {
	pcap_t *handle;
	int fd;
	uv_poll_t *poll_req;
	uv_timer_t *timer_req;

	struct uv_custom_poll_t *custom_poll_data;

	unsigned char srcmac[ETH_ALEN];
	char netmask[INET_ADDRSTRLEN];
	char srcip[INET_ADDRSTRLEN];
} data_t;

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

static struct data_t **data;
static int nrdata = 0;
static int check = 0;
static int check1[4] = {0};
static int start = 0;
static char min[17];
static uv_timer_t *timer_req = NULL;

static CuTest *gtc = NULL;

static void stop(uv_timer_t *req) {
	uv_stop(uv_default_loop());
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void *arp_event(int reason, void *param, void *userdata) {
	struct reason_arp_device_t *data1 = param;
	switch(reason) {
		case REASON_ARP_FOUND_DEVICE:
			if(check1[0] == 0 &&
				strcmp(data1->ip, min) == 0 && strcmp(data1->mac, "AA:BB:CC:DD:EE:FF") == 0) {
				printf("[ %-48s ]\n", "- waiting for ip change");
				fflush(stdout);

				check1[0] = 1;
				check = 1;
			}
		break;
		case REASON_ARP_LOST_DEVICE:
			if(check1[1] == 0 &&
				strcmp(data1->ip, min) == 0 && strcmp(data1->mac, "AA:BB:CC:DD:EE:FF") == 0) {
				printf("[ %-48s ]\n", "- waiting for disconnection");
				fflush(stdout);

				check1[1] = 1;
			}
			if(check1[2] == 0 &&
				strcmp(data1->ip, min) == 0 && strcmp(data1->mac, "FF:BB:CC:DD:EE:FF") == 0) {
				check1[2] = 1;
				arp_stop();
				uv_stop(uv_default_loop());
				uv_timer_stop(timer_req);
			}
		break;
		case REASON_ARP_CHANGED_DEVICE:
			if(check1[3] == 0 &&
				strcmp(data1->ip, min) == 0 && strcmp(data1->mac, "FF:BB:CC:DD:EE:FF") == 0) {
				printf("[ %-48s ]\n", "- waiting for mac change");
				fflush(stdout);

				check1[3] = 1;
			}
		break;
	}

	return NULL;
}

static void free_test_data(struct data_t *data) {
	if(data->custom_poll_data != NULL) {
		uv_custom_poll_free(data->custom_poll_data);
	}
	if(data->handle != NULL) {
		pcap_close(data->handle);
	}
	FREE(data);
}

static void callback(u_char *user, const struct pcap_pkthdr *pkt_header, const u_char *bytes) {
	struct data_t *data = (void *)user;

	arphdr_t *arpheader = NULL;
	ether_hdr *ether_hdr = NULL;
	char ip[INET_ADDRSTRLEN], mac[18], srcmac[18];

	memset(ip, 0, INET_ADDRSTRLEN);
	memset(mac, 0, 18);

	ether_hdr = (struct ether_hdr *)bytes;
	arpheader = (struct arphdr *)(bytes+14);

	/*
	 * Validate sent APR packet
	 */
	sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
		ether_hdr->dstmac[0], ether_hdr->dstmac[1], ether_hdr->dstmac[2],
		ether_hdr->dstmac[3], ether_hdr->dstmac[4], ether_hdr->dstmac[5]
	);

	if(strcmp(mac, "FF:FF:FF:FF:FF:FF") == 0) {
		sprintf(srcmac, "%02x:%02x:%02x:%02x:%02x:%02x",
			data->srcmac[0], data->srcmac[1], data->srcmac[2],
			data->srcmac[3], data->srcmac[4], data->srcmac[5]
		);

		sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
			ether_hdr->srcmac[0], ether_hdr->srcmac[1], ether_hdr->srcmac[2],
			ether_hdr->srcmac[3], ether_hdr->srcmac[4], ether_hdr->srcmac[5]
		);

		if(strcmp(mac, srcmac) == 0 && ether_hdr->ftype == htons(0x0806)) {
			if(htons(arpheader->htype) == 1 &&
			   htons(arpheader->ptype) == 0x800 &&
				 arpheader->hlen == 6 &&
				 arpheader->plen == 4 &&
				 ntohs(arpheader->oper) == ARP_REQUEST) {
				sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
					arpheader->sha[0], arpheader->sha[1], arpheader->sha[2],
					arpheader->sha[3], arpheader->sha[4], arpheader->sha[5]
				);
				sprintf(ip, "%d.%d.%d.%d",
					arpheader->spa[0], arpheader->spa[1], arpheader->spa[2], arpheader->spa[3]
				);
				if(strcmp(mac, srcmac) == 0 && strcmp(ip, data->srcip) == 0) {
					sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
						arpheader->tha[0], arpheader->tha[1], arpheader->tha[2],
						arpheader->tha[3], arpheader->tha[4], arpheader->tha[5]
					);
					sprintf(ip, "%d.%d.%d.%d",
						arpheader->tpa[0], arpheader->tpa[1], arpheader->tpa[2], arpheader->tpa[3]
					);
					if(strcmp(mac, "00:00:00:00:00:00") == 0 && strcmp(ip, data->srcip) == 0 && start == 0) {
						start = 1;
					}
				}
			}
		}
	}
}

static void read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;

	pcap_dispatch(data->handle, 1, callback, (void *)data);

	uv_custom_read(data->poll_req);
}

static void write_cb(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;

	/*
	 * Respond with an arp request on the first ip
	 * across all network devices.
	 */
	if(check <= nrdata /*&& start == 1*/) {
		check++;
		struct in_addr in_ip, in_netmask, in_min;

		memset(&min, 0, 17);

		unsigned char packet[60];
		memset(&packet, '\0', 60);

		struct ether_hdr *hdr = (struct ether_hdr *)(packet);
		struct arphdr *pkt = (struct arphdr *)(packet + sizeof(struct ether_hdr));

		memcpy(hdr->dstmac, data->srcmac, ETH_ALEN);
		if(check1[0] == 0) {
			hdr->srcmac[0] = 0xaa;
		} else {
			hdr->srcmac[0] = 0xff;
		}
		hdr->srcmac[1] = 0xbb;
		hdr->srcmac[2] = 0xcc;
		hdr->srcmac[3] = 0xdd;
		hdr->srcmac[4] = 0xee;
		hdr->srcmac[5] = 0xff;
		hdr->ftype = htons(0x0806);

		pkt->htype = htons(1);
		pkt->ptype = htons(0x0800);
		pkt->hlen = 6;
		pkt->plen = 4;
		pkt->oper = htons(ARP_REPLY);
		memcpy(pkt->sha, hdr->srcmac, ETH_ALEN);
		memcpy(pkt->tha, hdr->dstmac, ETH_ALEN);

		uv_inet_pton(AF_INET, data->srcip, &in_ip);
		uv_inet_pton(AF_INET, data->netmask, &in_netmask);
		in_min.s_addr = (in_ip.s_addr & in_netmask.s_addr);

		/*
		 * Instead of 10.0.0.0 reply to 10.0.0.1
		 */
		struct in_addr *tmp = &in_min;
		((unsigned char *)&(*tmp).s_addr)[3] = 1;

		uv_inet_ntop(AF_INET, &in_min, min, sizeof(min));

		inet_pton(AF_INET, min, pkt->spa);
		inet_pton(AF_INET, data->srcip, pkt->tpa);

		pcap_sendpacket(data->handle, packet, 60);
	}
	uv_custom_write(data->poll_req);
}

static void start_arp(void) {
	char error[PCAP_ERRBUF_SIZE], *e = error;
	char ip[17], netmask[17];
	int count = 0, i = 0, x = 0, has_mac = 0;
	uv_interface_address_t *interfaces = NULL;

	int err = uv_interface_addresses(&interfaces, &count);
	if(err != 0) {
		logprintf(LOG_ERR, "uv_interface_addresses: %s", uv_strerror(err));
		return;
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
				continue;
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

			memcpy(data[nrdata]->srcmac, interfaces[i].phys_addr, ETH_ALEN);
			memcpy(data[nrdata]->srcip, ip, INET_ADDRSTRLEN);
			memcpy(data[nrdata]->netmask, netmask, INET_ADDRSTRLEN);

			if((data[nrdata]->handle = pcap_open_live(interfaces[i].name, 64, 0, 1, e)) == NULL) {
				logprintf(LOG_ERR, "pcap_open_live: %s", e);
				free_test_data(data[nrdata]);
				continue;
			}

			if((pcap_setnonblock(data[nrdata]->handle, 1, e)) != 0) {
				logprintf(LOG_ERR, "pcap_setnonblock: %s", e);
				free_test_data(data[nrdata]);
				continue;
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

			data[nrdata]->timer_req->data = data[nrdata];

			data[nrdata]->poll_req = NULL;
			if((data[nrdata]->poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_custom_poll_init(&data[nrdata]->custom_poll_data, data[nrdata]->poll_req, (void *)data[nrdata]);
			data[nrdata]->custom_poll_data->is_ssl = 0;
			data[nrdata]->custom_poll_data->is_udp = 0;
			data[nrdata]->custom_poll_data->custom_recv = 1;
			data[nrdata]->custom_poll_data->write_cb = write_cb;
			data[nrdata]->custom_poll_data->read_cb = read_cb;
			data[nrdata]->custom_poll_data->close_cb = NULL;

			int r = uv_poll_init_socket(uv_default_loop(), data[nrdata]->poll_req, data[nrdata]->fd);
			if(r != 0) {
				logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
				free_test_data(data[nrdata]);
				continue;
			}

			uv_custom_write(data[nrdata]->poll_req);
			uv_custom_read(data[nrdata]->poll_req);

			nrdata++;
		}
	}

	uv_free_interface_addresses(interfaces, count);
}

static void test_arp(CuTest *tc) {
	if(geteuid() != 0) {
		printf("[ %-33s (requires root)]\n", __FUNCTION__);
		fflush(stdout);
		return;
	}
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	FILE *f = fopen("arp.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"arp-interval\":1,\"arp-timeout\":1},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	plua_init();

	test_set_plua_path(tc, __FILE__, "arp.c");

	storage_init();
	CuAssertIntEquals(tc, 0, storage_read("arp.json", CONFIG_SETTINGS));

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 4000, 0);

	printf("[ %-48s ]\n", "- waiting for connection");
	fflush(stdout);

	start_arp();
	arp_scan();

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_ARP_FOUND_DEVICE, arp_event, NULL);
	eventpool_callback(REASON_ARP_CHANGED_DEVICE, arp_event, NULL);
	eventpool_callback(REASON_ARP_LOST_DEVICE, arp_event, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	int i = 0;
	for(i=0;i<nrdata;i++) {
		free_test_data(data[i]);
	}
	FREE(data);

	eventpool_gc();
	storage_gc();
	plua_gc();
	arp_gc();

	for(i=0;i<4;i++) {
		CuAssertIntEquals(tc, 1, check1[i]);
	}
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_arp(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_arp);

	return suite;
}