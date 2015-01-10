/*
 * icmpquery.c - send and receive ICMP queries for address mask
 *               and current time.
 *
 * Version 1.0.3
 *
 * Copyright 1998, 1999, 2000  David G. Andersen <angio@pobox.com>
 *                                        <danderse@cs.utah.edu>
 *                                        http://www.angio.net/
 *
 * All rights reserved.
 * This information is subject to change without notice and does not
 * represent a commitment on the part of David G. Andersen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David G. Andersen may not
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL DAVID G. ANDERSEN BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *
 * Verified to work on:
 *    FreeBSD (2.x, 3.x)
 *    Linux 2.0.x, 2.2.0-pre1
 *    NetBSD 1.3
 *
 * Should work on Solaris and other platforms with BSD-ish stacks.
 *
 * If you compile it somewhere else, or it doesn't work somewhere,
 * please let me know.
 *
 * Compilation:  gcc icmpquery.c -o icmpquery
 *
 * One usage note:  In order to receive accurate time information,
 *                  the time on your computer must be correct; the
 *                  ICMP timestamp reply is a relative time figure.
 */


/* Some portions of this code are taken from FreeBSD's ping source.
 * Those portions are subject to the BSD copyright, which is appended
 * at the end of this file.  Namely, in_cksum.
 */

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>

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

static int initpacket(char *buf) {
	struct ip *ip = (struct ip *)buf;
	struct icmp *icmp = (struct icmp *)(ip + 1);
	int icmplen = 20;
	struct in_addr fromaddr;
	fromaddr.s_addr = 0;

	ip->ip_src = fromaddr;
	ip->ip_v = 4;
	ip->ip_hl = sizeof *ip >> 2;
	ip->ip_tos = 0;
	ip->ip_id = htons(4321);
	ip->ip_ttl = 255;
	ip->ip_p = 1;
	ip->ip_sum = 0;

	icmp->icmp_seq = 1;
	icmp->icmp_cksum = 0;
	icmp->icmp_type = ICMP_TSTAMP;
	icmp->icmp_code = 0;

	gettimeofday((struct timeval *)(icmp+8), NULL);
	memset(icmp+12, '\0',	8);
	ip->ip_len = sizeof(struct ip) + icmplen;
	return icmplen;
}

int ping(char *addr) {
	char buf[1500];
	struct ip *ip = (struct ip *)buf;
	struct icmp *icmp = (struct icmp *)(ip + 1);
	struct sockaddr_in dst;
	struct timeval tv;
	int icmplen = 0;
	int sockfd = 0, on = 1;
	long int fromlen = 0;

	if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		perror("socket");
		return -1;
	}

	if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
		perror("IP_HDRINCL");
		return -1;
	}

	tv.tv_sec = 0;
	tv.tv_usec = 5000;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0) {
		perror("SO_RCVTIMEO");
		return -1;
	}

	memset(buf, '\0', 1500);
	icmplen = initpacket(buf);
	dst.sin_family = AF_INET;

	ip->ip_dst.s_addr = inet_addr(addr);
	dst.sin_addr.s_addr = inet_addr(addr);
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum((u_short *)icmp, icmplen);
	if(sendto(sockfd, buf, ip->ip_len, 0, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
		return -1;
	}

	memset(buf, '\0', sizeof(buf));
	if((recvfrom(sockfd, buf, sizeof(buf), 0, NULL, (unsigned int *)&fromlen)) < 0) {
		return -1;
	}

	icmp = (struct icmp *)(buf + (ip->ip_hl << 2));
	if(!(icmp->icmp_type == ICMP_TSTAMPREPLY && strcmp(inet_ntoa(ip->ip_src), addr) == 0)) {
		return -1;
	}
	return 0;
}
