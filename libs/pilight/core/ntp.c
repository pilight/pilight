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
#include <signal.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
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
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#include "network.h"
#include "socket.h"
#include "pilight.h"
#include "../config/settings.h"
#include "common.h"
#include "log.h"

typedef struct l_fp {
	union {
		unsigned int Xl_ui;
		int Xl_i;
	} Ul_i;
	union {
		unsigned int Xl_uf;
		int Xl_f;
	} Ul_f;
} l_fp;

typedef struct pkt {
	int	li_vn_mode;
	int rootdelay;
	int rootdispersion;
	int refid;
	struct l_fp ref;
	struct l_fp org;
	struct l_fp rec;
	/* Make sure the pkg is 48 bits */
	double tmp;
} pkt;

static int diff = 0;
static int ntploop = 1;
static int running = 0;
static int synced = -1;

int ntp_gc(void) {
	ntploop = 0;
	while(running > 0) {
		usleep(10);
	}
	logprintf(LOG_DEBUG, "garbage collected ntp library");

	return 0;
}

void *ntpthread(void *param) {
	struct sockaddr_in servaddr;
	struct pkt msg;
	char ip[INET_ADDRSTRLEN+1], **ntpservers = NULL, name[25];
	unsigned int nrservers = 0;
	int sockfd = 0, firstrun = 1, interval = 86400;
	int counter = 0, ntptime = -1,  x = 0, timeout = 3;

	memset(&msg, '\0', sizeof(struct pkt));
	memset(&servaddr, '\0', sizeof(struct sockaddr_in));

	while(1) {
		sprintf(name, "ntpserver%d", nrservers);
		if((ntpservers = REALLOC(ntpservers, sizeof(char *)*(nrservers+1))) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		if((x = settings_find_string(name, &ntpservers[nrservers])) == 0) {
			nrservers++;
		} else {
			break;
		}
	}

	if(nrservers == 0) {
		FREE(ntpservers);
		logprintf(LOG_NOTICE, "no ntp-servers defined in the config settings");
		return 0;
	}

#ifndef _WIN32
	struct timeval tv;
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
#else
	WSADATA wsa;

	timeout *= 1000;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		return (void *)NULL;
	}
#endif

	running = 1;

	while(ntploop) {
		if((counter == interval || (ntptime == -1 && ((counter % 10) == 0)) || (firstrun == 1))) {
			firstrun = 0;
			for(x=0;x<nrservers;x++) {
				if(ntploop == 0) {
					break;
				}

				char *p = ip;
				if(host2ip(ntpservers[x], p) == -1) {
					continue;
				}

				if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
					logprintf(LOG_ERR, "error in ntp server socket connection @%s", ntpservers[x]);
				} else {
					memset(&servaddr, '\0', sizeof(servaddr));
					servaddr.sin_family = AF_INET;
					servaddr.sin_port = htons(123);

					inet_pton(AF_INET, ip, &servaddr.sin_addr);

					switch(socket_timeout_connect(sockfd, (struct sockaddr *)&servaddr, timeout)) {
						case -1:
							logprintf(LOG_NOTICE, "could not connect to ntp server @%s", ntpservers[x]);
							continue;
						break;
						case -2:
							logprintf(LOG_NOTICE, "ntp server connection timeout @%s", ntpservers[x]);
							continue;
						break;
						case -3:
							logprintf(LOG_NOTICE, "error in ntp server socket connection @%s", ntpservers[x]);
							continue;
						break;
						default:
						break;
					}

#ifdef _WIN32
					setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(struct timeval));
#else
					setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
#endif
					msg.li_vn_mode=227;
					if(sendto(sockfd, (char *)&msg, 48, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < -1) {
						logprintf(LOG_NOTICE, "error in ntp sending");
					} else {
						if(recvfrom(sockfd, (void *)&msg, 48, 0, NULL, NULL) < -1) {
							logprintf(LOG_NOTICE, "error in ntp receiving");
						} else {
							if(msg.refid > 0) {
								(msg.rec).Ul_i.Xl_ui = ntohl((msg.rec).Ul_i.Xl_ui);
								(msg.rec).Ul_f.Xl_f = (int)ntohl((unsigned int)(msg.rec).Ul_f.Xl_f);

								unsigned int adj = 2208988800u;
								ntptime = (time_t)(msg.rec.Ul_i.Xl_ui - adj);
								diff = (int)(time(NULL) - ntptime);
								logprintf(LOG_INFO, "time offset found of %d seconds", diff);
								counter = 0;
								synced = 0;
								break;
							} else {
								logprintf(LOG_INFO, "could not sync with ntp server: %s", ntpservers[x]);
							}
						}
					}
				}
				if(sockfd > 0) {
					close(sockfd);
				}
			}
		}
		sleep(1);
		counter++;
	}

	if(ntpservers != NULL) {
		FREE(ntpservers);
	}
	running = 0;

	return (void *)NULL;
}

int getntpdiff(void) {
	return diff;
}

int isntpsynced(void) {
	return synced;
}
