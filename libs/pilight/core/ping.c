/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/* 
 * Some portions of this code are taken from FreeBSD's ping source.
 * Those portions are subject to the BSD copyright, which is appended
 * at the end of this file. Namely, in_cksum.
 *
 * Some other portions are taken from icmpquery by
 *  David G. Andersen <angio@pobox.com>
 *                    <danderse@cs.utah.edu>
 *                    http://www.angio.net/
 *
 */

#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
	#include <iphlpapi.h>
	#include <icmpapi.h>
	#define MSG_NOSIGNAL 0
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#ifndef __sun
		#include <sys/cdefs.h>
		#include <netinet/if_ether.h>
	#endif
	#include <sys/time.h>
	#include <netinet/tcp.h>
	#include <net/route.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <netinet/in_systm.h>
	#include <netinet/ip.h>
	#include <netinet/ip_icmp.h>
	#include <unistd.h>
	#include <sys/time.h>
#endif
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "common.h"
#include "ping.h"
#include "network.h"
// #include "timerpool.h"
#include "log.h"
#include "mem.h"

#ifdef _WIN32
static char send_data[] = "pilight";
struct io_status_block {
  union {
    int status;
    void *pointer;
  };
  unsigned long information;
} io_status_block;
#endif

typedef struct data_t {
	struct ping_list_t *nodes;
	struct ping_list_t *iplist;
#ifdef _WIN32
	HANDLE file;
	char ip[INET_ADDRSTRLEN+1];
	struct {
		char *buffer;
		int size;
	} reply;
#else
	uv_timer_t *timer_req;
	uv_poll_t *poll_req;
#endif

	void (*callback)(char *, int);
} data_t;

void ping_add_host(struct ping_list_t **iplist, const char *ip) {
	struct ping_list_t *node = MALLOC(sizeof(struct ping_list_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	strncpy(node->ip, ip, INET_ADDRSTRLEN);
	node->found = 0;
	node->live = 0;
	node->called = 0;

	node->next = *iplist;
	*iplist = node;
}

static void ping_data_gc(struct data_t *data) {
	struct ping_list_t *tmp = NULL;
#ifdef _WIN32
	FREE(data->reply.buffer);
	IcmpCloseHandle(data->file);
#endif
	while(data->iplist) {
		tmp = data->iplist;
		data->iplist = data->iplist->next;
		FREE(tmp);
	}
	if(data->iplist != NULL) {
		FREE(data->iplist);
	}
	FREE(data);
}

#ifdef _WIN32
int reply_cb(void *param, struct io_status_block *io_status, unsigned long reserved) {
	struct data_t *data = param;

	data->callback(data->ip, (io_status->status == 0) ? 0 : -1);
	if(data->nodes == NULL) {
		ping_data_gc(data);
	} else {
		unsigned long ipaddr = INADDR_NONE;

		strcpy(data->ip, data->nodes->ip);
		inet_pton(AF_INET, data->nodes->ip, &ipaddr);

		data->nodes = data->nodes->next;
		int ret = IcmpSendEcho2(data->file, NULL, (FARPROC)reply_cb, data, ipaddr, send_data, sizeof(send_data), NULL, data->reply.buffer, data->reply.size, 1000);
		int error = WSAGetLastError();
		if(error != ERROR_IO_PENDING) {
			logprintf(LOG_ERR, "IcmpSendEcho2: %d", error);
			FREE(data);
			return -1;
		}
	}
	
	return 0;
}

int ping(struct ping_list_t *iplist, void (*callback)(char *, int)) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	unsigned long ipaddr = INADDR_NONE;
	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}

	memset(data, 0, sizeof(struct data_t));
	data->callback = callback;
	data->nodes = iplist;
	data->iplist = iplist;
	data->file = IcmpCreateFile();

	if(data->file == INVALID_HANDLE_VALUE) {
		logprintf(LOG_ERR, "IcmpCreateFile: %d\n", WSAGetLastError());
		FREE(data);
		return -1;
	}

	data->reply.size = sizeof(ICMP_ECHO_REPLY) + sizeof(send_data) + 8;
	data->reply.buffer = MALLOC(data->reply.size);
	if(data->reply.buffer == NULL) {
		OUT_OF_MEMORY
	}

	strcpy(data->ip, data->nodes->ip);
	inet_pton(AF_INET, data->nodes->ip, &ipaddr);
	data->nodes = data->nodes->next;

	int ret = IcmpSendEcho2(data->file, NULL, (FARPROC)reply_cb, data, ipaddr, send_data, sizeof(send_data), NULL, data->reply.buffer, data->reply.size, 1000);
	int error = WSAGetLastError();
	if(error != ERROR_IO_PENDING) {
		logprintf(LOG_ERR, "IcmpSendEcho2: %d",error);
		FREE(data);
		return -1;
	}
	return 0;
}
#else
/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 *      From FreeBSD's ping.c
 */
static int in_cksum(int *addr, int len) {
	register int nleft = len;
	register int *w = addr;
	register int sum = 0;
	int answer = 0;

	/*
	* Our algorithm is simple, using a 32 bit accumulator (sum), we add
	* sequential 16 bit words to it, and at the end, fold back all the
	* carry bits from the top 16 bits into the lower 16 bits.
	*/
	while(nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if(nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return answer;
}

static void ping_timeout(uv_timer_t *handle) {
	struct data_t *data = handle->data;
	struct ping_list_t *tmp = data->iplist;
	struct timeval tv;
	unsigned long now = 0;
	int match = 0, match1 = 0;

	gettimeofday(&tv, NULL);
	now = (1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec);

	while(tmp) {
		match1++;
		if((now-tmp->time) > 1000000 && tmp->live == 1) {
			tmp->live = 0;
			if(tmp->found == 0) {
				if(tmp->called == 0) {
					tmp->called = 1;
					if(data->callback != NULL) {
						data->callback(tmp->ip, (tmp->found == 1) ? 0 : -1);
					}
				}
			}
			match++;
		} else if(tmp->called == 1) {
			match++;
		}
		tmp = tmp->next;
	}

	if(match1 == match) {
		tmp = data->iplist;
		while(tmp) {
			if(tmp->found == 0) {
				if(data->callback != NULL && tmp->called == 0) {
					tmp->called = 1;
					data->callback(tmp->ip, (tmp->found == 1) ? 0 : -1);
				}
				logprintf(LOG_DEBUG, "ping did not find the network device %s", tmp->ip);
			}
			tmp = tmp->next;
		}

		uv_custom_close(data->poll_req);
		uv_custom_write(data->poll_req);
		uv_timer_stop(data->timer_req);
	}
	
	return;
}

static void close_cb(uv_handle_t *handle) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	FREE(handle);
}

static void custom_close_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;

	ping_data_gc(data);
	uv_custom_poll_free(custom_poll_data);

	if(!uv_is_closing((uv_handle_t *)req)) {
		uv_poll_stop(req);
		uv_close((uv_handle_t *)req, close_cb);
	}
}

#ifdef __FreeBSD__
static void write_cb(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;
	static u_char outpackhdr[IP_MAXPACKET], *outpack;
	struct icmp *icp = NULL;	
	static int datalen = 12;
	static int send_len = 0;
	struct ip *ip = NULL;
	u_char packet[IP_MAXPACKET] __aligned(4);
	int icmp_len = 0, cc = 0, i = 0, r = 0;

	uv_os_fd_t fd = 0;
	r = uv_fileno((uv_handle_t *)req, &fd);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return;
	}	

	outpack = outpackhdr + sizeof(struct ip);
	
	icmp_len = sizeof(struct ip) + ICMP_MINLEN;
	send_len = icmp_len + datalen;

	ip = (struct ip *)outpackhdr;

	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_tos = 0;
	ip->ip_id = htons(4321);
	ip->ip_off = htons(0);
	ip->ip_ttl = 255;
	ip->ip_p = IPPROTO_ICMP;
	ip->ip_src.s_addr = INADDR_ANY;

	icp = (struct icmp *)packet;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = htons(0);
	icp->icmp_id = htons(4321);

	cc = ICMP_MINLEN + datalen;

	icp->icmp_cksum = in_cksum((int *)icp, cc);

	struct sockaddr_in dst;
	memset(&dst, 0, sizeof(struct sockaddr));

	dst.sin_family = AF_INET;
	inet_pton(AF_INET, data->nodes->ip, &dst.sin_addr);
	dst.sin_port = htons(0);	
	
	if((i = sendto(fd, packet, cc, 0, (struct sockaddr *)&dst, sizeof(dst))) < 0) {
		logprintf(LOG_ERR, "sendto: %s", strerror(errno));
	}
	data->nodes->live = 1;
	data->nodes = data->nodes->next;

	uv_custom_read(req);
	if(data->nodes != NULL) {
		uv_custom_write(req);
	}
}
#else
static int initpacket(char *buf) {
	struct ip *ip = (struct ip *)buf;
	struct icmp *icmp = (struct icmp *)(ip + 1);
	int icmplen = 20;

	ip->ip_src.s_addr = INADDR_ANY;
	ip->ip_v = 4;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_tos = 0;
	ip->ip_id = htons(4321);
	ip->ip_ttl = 255;
	ip->ip_p = IPPROTO_ICMP;
	ip->ip_sum = 0;
	ip->ip_off = 0;

	icmp->icmp_seq = 0;
	icmp->icmp_cksum = 0;
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;

	gettimeofday((struct timeval *)(icmp+8), NULL);
	memset(icmp+12, '\0',	8);
	ip->ip_len = sizeof(struct ip) + icmplen;

	return icmplen;
}

static void write_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;
	char buf[1500];
	struct ip *ip = (struct ip *)buf;
	struct icmp *icmp = (struct icmp *)(ip + 1);
	int icmplen = 0, r = 0, x = 0;

	memset(&buf, '\0', 1500);

	icmplen = initpacket(buf);

	uv_os_fd_t fd = 0;
	r = uv_fileno((uv_handle_t *)req, &fd);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return;
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	data->nodes->time = (1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec);

	struct sockaddr_in dst;
	memset(&dst, 0, sizeof(struct sockaddr));
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum((int *)icmp, icmplen);

	dst.sin_family = AF_INET;
	dst.sin_addr.s_addr = inet_addr(data->nodes->ip);
	dst.sin_port = htons(0);

	ip->ip_dst.s_addr = inet_addr(data->nodes->ip);

	if((x = sendto(fd, buf, ip->ip_len, 0, (struct sockaddr *)&dst, sizeof(dst))) < 0) {
		logprintf(LOG_ERR, "sendto: %s", strerror(errno));
	}

	data->nodes->live = 1;
	data->nodes = data->nodes->next;

	uv_custom_read(req);
	if(data->nodes != NULL) {
		uv_custom_write(req);
	}
}
#endif

static void read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;
	unsigned char buf2[1500];
	struct ip *ip = (struct ip *)buf2;
	struct icmp *icmp = (struct icmp *)(ip + 1);
	char buf1[INET_ADDRSTRLEN+1];

	if(*nread == -1) {
		/*
		 * We are disconnected
		 */
		return;
	}

	if(*nread == 40) {
		memcpy(&buf2, buf, 40);

		/*
		 * In case of inconsistancies between ip_src
		 * and ip_src->s_addr, synchronize ip_src
		 * to ip_src->s_addr
		 */
		int i = 0;
		for(i=0;i<4;i++) {
			if(((unsigned char *)&ip->ip_src)[i] != ((const unsigned char *)(&((struct sockaddr_in *)&ip->ip_src)->sin_addr))[i]) {
				((unsigned char *)(&((struct sockaddr_in *)&ip->ip_src)->sin_addr))[i] = ((unsigned char *)&ip->ip_src)[i];
			}
		}
		icmp = (struct icmp *)(buf2 + (ip->ip_hl << 2));

		memset(&buf1, '\0', INET_ADDRSTRLEN+1);

		uv_ip4_name((struct sockaddr_in *)&ip->ip_src, buf1, INET_ADDRSTRLEN+1);
		if(icmp->icmp_type == ICMP_ECHOREPLY || icmp->icmp_type == ICMP_ECHO) {
			struct ping_list_t *tmp = data->iplist;
			while(tmp) {
				if(strcmp(tmp->ip, buf1) == 0 && tmp->found == 0) {
					logprintf(LOG_DEBUG, "ping found network device %s", tmp->ip);
					if(tmp->called == 0) {
						tmp->called = 1;
						if(data->callback != NULL) {
							data->callback(tmp->ip, 0);
						}
					}
					tmp->found = 1;
					break;
				}
				tmp = tmp->next;
			}
		}
	}
	*nread = 0;
	uv_custom_read(req);
}

int ping(struct ping_list_t *iplist, void (*callback)(char *, int)) {
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct data_t *data = NULL;
	int sockfd = 0;
	unsigned long on = 1;

	if(iplist == NULL) {
		return -1;
	}

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
		logprintf(LOG_ERR, "socket: %s", strerror(errno));
		return -1;
	}

#ifdef _WIN32
	ioctlsocket(sockfd, FIONBIO, &on);
#else
	long arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg | O_NONBLOCK);
#endif

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on)) < 0) {
		logprintf(LOG_ERR, "setsockopt: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

#ifdef __FreeBSD__
	int off = 0;
	if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, (const char *)&off, sizeof(off)) < 0) {
#else
	if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, (const char *)&on, sizeof(on)) < 0) {
#endif
		logprintf(LOG_ERR, "setsockopt: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	if((data = MALLOC(sizeof(struct data_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(data, 0, sizeof(struct data_t));
	data->callback = callback;
	data->nodes = iplist;
	data->iplist = iplist;

	if((data->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY
	}
	data->timer_req->data = data;

	if((data->poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY
	}

	uv_custom_poll_init(&custom_poll_data, data->poll_req, (void *)data);

	custom_poll_data->write_cb = write_cb;
	custom_poll_data->read_cb = read_cb;
	custom_poll_data->close_cb = custom_close_cb;

	int r = uv_poll_init_socket(uv_default_loop(), data->poll_req, sockfd);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		goto free;
	}

	r = uv_timer_init(uv_default_loop(), data->timer_req);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_timer_init: %s", uv_strerror(r));
		goto free;
	}

	r = uv_timer_start(data->timer_req, (void (*)(uv_timer_t *))ping_timeout, 1000, 1000);		
	if(r != 0) {
		logprintf(LOG_ERR, "uv_timer_start: %s", uv_strerror(r));
		goto free;
	}

	uv_custom_write(data->poll_req);
	uv_custom_read(data->poll_req);

	return 0;

free:
	FREE(data->poll_req);
	FREE(data->timer_req);
	ping_data_gc(data);
	uv_custom_poll_free(custom_poll_data);

	return -1;
}
#endif