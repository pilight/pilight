/*
	Copyright (C) 2013 CurlyMo

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
#include <string.h>
#ifdef _WIN32
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
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "pilight.h"
#include "common.h"
#include "socket.h"
#include "log.h"
#include "gc.h"
#include "ssdp.h"

static int ssdp_socket = 0;
static int ssdp_loop = 1;

int ssdp_gc(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct sockaddr_in addr;
	int sockfd = 0;

	ssdp_loop = 0;
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
#endif

	/* Make a loopback socket we can close when pilight needs to be stopped
	   or else the select statement will wait forever for an activity */
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
	/* Wakeup our recvfrom statement so the ssdp_wait function can
	   actually close and the thread can end gracefully */
		addr.sin_family = AF_INET;
		addr.sin_port = htons(1900);
		addr.sin_addr.s_addr = inet_addr("239.255.255.250");
		sendto(sockfd, "1", 1, 0, (struct sockaddr *)&addr, sizeof(addr));
	}

	logprintf(LOG_DEBUG, "garbage collected ssdp library");
	return EXIT_SUCCESS;
}

int ssdp_start(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct sockaddr_in addr;
	struct ip_mreq mreq;
	int opt = 1;
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
#endif

	ssdp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(1900);

	if(setsockopt(ssdp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int)) == -1) {
		perror("setsockopt");
		return 0;
	}

	mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if(setsockopt(ssdp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
		logprintf(LOG_ERR, "cannot bind to the ssdp multicast network");
		return 0;
	}

	if(bind(ssdp_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return 0;
	}

	return 1;
}

int ssdp_seek(struct ssdp_list_t **ssdp_list) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct sockaddr_in addr;
	char message[BUFFER_SIZE] = {'\0'};
	char header[BUFFER_SIZE] = {'\0'};
	int sock, match = 0;
	int timeout = 10;
	ssize_t len = 0;
	socklen_t addrlen = sizeof(addr);
	unsigned short int nip[4], port = 0;

#ifndef _WIN32
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = timeout*10000;
#else
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		return -1;
	}
#endif

	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		logprintf(LOG_ERR, "could not create ssdp socket");
		goto end;
	}

	memset((void *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1900);
	addr.sin_addr.s_addr = inet_addr("239.255.255.250");

#ifdef _WIN32
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(struct timeval));
#else
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
#endif

	strcpy(header, "M-SEARCH * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"ST:urn:schemas-upnp-org:service:pilight:1\r\n"
					"Man:\"ssdp:discover\"\r\n"
					"MX:3\r\n\r\n");
	if((len = sendto(sock, header, BUFFER_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr))) >= 0) {
		logprintf(LOG_DEBUG, "ssdp sent search");
	}

	int x = 0;
	while(ssdp_loop) {
		memset(message, '\0', BUFFER_SIZE);
		if((x = recvfrom(sock, message, sizeof(message), 0, (struct sockaddr *)&addr, &addrlen)) < 1) {
			//perror("read");
			goto end;
		}

		if(strstr(message, "pilight") > 0) {
			char **array = NULL;
			unsigned int n = explode(message, "\r\n", &array), q = 0;
			for(q=0;q<n;q++) {
				if(match == 0 && sscanf(array[q], "Location:%hu.%hu.%hu.%hu:%hu\r\n", &nip[0], &nip[1], &nip[2], &nip[3], &port) > 0) {
					match = 1;
				}
				FREE(array[q]);
			}
			if(n > 0) {
				FREE(array);
			}
			if(match == 1) {
				struct ssdp_list_t *node = MALLOC(sizeof(struct ssdp_list_t));
				if(node == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				sprintf(node->ip, "%hu.%hu.%hu.%hu", nip[0], nip[1], nip[2], nip[3]);
				node->ip[16] = '\0';
				node->port = port;
				node->next = *ssdp_list;
				*ssdp_list = node;
			}
		}
	}
	goto end;

end:
	if(sock > 0) {
		close(sock);
	}
	struct ssdp_list_t *ptr = *ssdp_list, *next = NULL, *prev = NULL;
	if(match == 1) {
		while(ptr) {
			next = ptr->next;
			ptr->next = prev;
			prev = ptr;
			ptr = next;
		}
		if(ptr != NULL) {
			FREE(ptr);
		}
		if(next != NULL) {
			FREE(next);
		}
		*ssdp_list = prev;

		return 0;
	} else {
		return -1;
	}
}

void ssdp_free(struct ssdp_list_t *ssdp_list) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct ssdp_list_t *tmp = NULL;
	while(ssdp_list) {
		tmp = ssdp_list;
		ssdp_list = ssdp_list->next;
		FREE(tmp);
	}
	if(ssdp_list != NULL) {
		FREE(ssdp_list);
	}
}

void *ssdp_wait(void *param) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct sockaddr_in addr;
	char message[BUFFER_SIZE];
	char **header = NULL;
	char *id = NULL;
	ssize_t len = 0;
	socklen_t addrlen = sizeof(addr);
	int nrheader = 0, x = 0;
#ifndef _WIN32
	int family = 0, s = 0;
	char host[NI_MAXHOST];
	struct ifaddrs *ifaddr, *ifa;
#endif
	char *distro = distroname();
	char *hname = hostname();

	if(distro == NULL) {
		logprintf(LOG_ERR, "failed to determine the distribution");
		exit(EXIT_FAILURE);
	}

#ifdef _WIN32
	char buf[INET_ADDRSTRLEN+1];
	int i;

	PMIB_IPADDRTABLE pIPAddrTable;
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;
	IN_ADDR IPAddr;
	pIPAddrTable = MALLOC(sizeof(MIB_IPADDRTABLE));

	if(pIPAddrTable) {
		if(GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
			FREE(pIPAddrTable);
			pIPAddrTable = MALLOC(dwSize);

		}
		if(pIPAddrTable == NULL) {
			printf("Memory allocation failed for GetIpAddrTable\n");
			exit(EXIT_FAILURE);
		}
	}

	if((dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0)) != NO_ERROR) {
		logprintf(LOG_ERR, "GetIpAddrTable failed with");
		exit(EXIT_FAILURE);
	}

	for(i=0;i<(int)pIPAddrTable->dwNumEntries;i++) {
		IPAddr.S_un.S_addr = (u_long)pIPAddrTable->table[i].dwAddr;
		memset(&buf, '\0', INET_ADDRSTRLEN+1);
		inet_ntop(AF_INET, (void *)&IPAddr, buf, INET_ADDRSTRLEN+1);
		if(strcmp(buf, "127.0.0.1") != 0) {
			if(!(header = realloc(header, sizeof(char *)*((size_t)nrheader+1)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			if((header[nrheader] = MALLOC(BUFFER_SIZE)) == NULL) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(&buf, '\0', INET_ADDRSTRLEN+1);
			inet_ntop(AF_INET, (void *)&IPAddr, buf, INET_ADDRSTRLEN+1);
			memset(header[nrheader], '\0', BUFFER_SIZE);
			sprintf(header[nrheader], "NOTIFY * HTTP/1.1\r\n"
				"Host:239.255.255.250:1900\r\n"
				"Cache-Control:max-age=900\r\n"
				"Location:%s:%d\r\n"
				"NT:urn:schemas-upnp-org:service:pilight:1\r\n"
				"USN:uuid:%s::urn:schemas-upnp-org:service:pilight:1\r\n"
				"NTS:ssdp:alive\r\n"
				"SERVER: %s UPnP/1.1 pilight (%s)/%s\r\n\r\n", buf, socket_get_port(), id, distro, hname, PILIGHT_VERSION);
			nrheader++;
		}
	}

	if(pIPAddrTable != NULL) {
		FREE(pIPAddrTable);
		pIPAddrTable = NULL;
	}
#else

#ifdef __FreeBSD__
	if(rep_getifaddrs(&ifaddr) == -1) {
		logprintf(LOG_ERR, "could not get network adapter information");
		exit(EXIT_FAILURE);
	}
#else
	if(getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}
#endif

	for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr == NULL) {
			continue;
		}

		family = ifa->ifa_addr->sa_family;

		if((strstr(ifa->ifa_name, "lo") == NULL && strstr(ifa->ifa_name, "vbox") == NULL
		    && strstr(ifa->ifa_name, "dummy") == NULL) && (family == AF_INET || family == AF_INET6)) {
			if((id = genuuid(ifa->ifa_name)) == NULL) {
				logprintf(LOG_ERR, "could not generate the device uuid");
				exit(EXIT_FAILURE);
			}
			memset(host, '\0', NI_MAXHOST);

			s = getnameinfo(ifa->ifa_addr,
                           (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                                 sizeof(struct sockaddr_in6),
                           host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if(s != 0) {
				logprintf(LOG_ERR, "getnameinfo() failed: %s", gai_strerror(s));
				exit(EXIT_FAILURE);
			}
			if(strlen(host) > 0) {
				if(!(header = REALLOC(header, sizeof(char *)*((size_t)nrheader+1)))) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				if(!(header[nrheader] = MALLOC(BUFFER_SIZE))) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				memset(header[nrheader], '\0', BUFFER_SIZE);
				sprintf(header[nrheader], "NOTIFY * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"Cache-Control:max-age=900\r\n"
					"Location:%s:%d\r\n"
					"NT:urn:schemas-upnp-org:service:pilight:1\r\n"
					"USN:uuid:%s::urn:schemas-upnp-org:service:pilight:1\r\n"
					"NTS:ssdp:alive\r\n"
					"SERVER: %s UPnP/1.1 pilight (%s)/%s\r\n\r\n", host, socket_get_port(), id, distro, hname, PILIGHT_VERSION);
				nrheader++;
			}
		}
	}

	freeifaddrs(ifaddr);
#endif

	if(id != NULL) {
		FREE(id);
	}
	FREE(distro);
	FREE(hname);

	while(ssdp_loop) {
		memset(message, '\0', BUFFER_SIZE);
		if(recvfrom(ssdp_socket, message, sizeof(message), 0, (struct sockaddr *)&addr, &addrlen) < 1) {
			//perror("read");
			break;
		}

		if(strstr(message, "M-SEARCH * HTTP/1.1") > 0 && strstr(message, "urn:schemas-upnp-org:service:pilight:1") > 0) {
			for(x=0;x<nrheader;x++) {
				if(strlen(header[x]) > 0) {
					if((len = sendto(ssdp_socket, header[x], BUFFER_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr))) >= 0) {
						logprintf(LOG_DEBUG, "ssdp sent notify");
					}
				}
			}
		}
	}

	for(x=0;x<nrheader;x++) {
		FREE(header[x]);
	}

	FREE(header);
	return 0;
}

void ssdp_close(int sock) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	close(sock);
}
