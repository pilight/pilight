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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>

#include "../../pilight.h"
#include "common.h"
#include "socket.h"
#include "log.h"
#include "ssdp.h"

char *ssdp_mac = NULL;
char *ssdp_hostname = NULL;
/* Allow an ip address for either a wlan of eth0 adapter */
char sspd_header[2][BUFFER_SIZE];
int ssdp_socket = 0;
int ssdp_loop = 1;

char *ssdp_gethostname(void) {
	if(!ssdp_hostname) {
		char hostname[255] = {'\0'};
		gethostname(hostname, 254);
		char *pch = strtok(hostname, ".");
		ssdp_hostname = malloc(strlen(pch)+1);
		strcpy(ssdp_hostname, pch);
	}
	return ssdp_hostname;
}

char *ssdp_getdistroname(void) {
	int fd[2];
	int pid;
	int rc = 1;
	char buff[32];
	char dist[32];
	float version = 0;
	memset(dist, '\0', 32);

	if(pipe(fd) < 0) {
		perror("pipe");
		return NULL;
	} else {
		pid = fork();
		switch(pid) {
			case 0:
				dup2(fd[1], 1);
				close(fd[0]);
				close(fd[1]);
#ifndef __FreeBSD__
				system("lsb_release -ri");
#elif defined(__FreeBSD__)
				system("uname -rs");
#endif
				close(fd[1]);
				exit(0);
			break;
			case -1:
				exit(0);
			break;
			default:
				close(fd[1]);
				waitpid(pid, NULL ,0);

				while(rc > 0) {
					rc = read(fd[0], buff, 32);
#ifndef __FreeBSD__						
					sscanf(buff, "Distributor ID:%*[ \n\t]%s", dist);
					sscanf(buff, "%f", &version);
#else
					sscanf(buff, "%s%*[ \n\t]%f-.*", dist, &version);
#endif
					if(strlen(dist) > 0 && (int)version != 0) {
						break;
					}
				}
			break;
		}
		char *ssdp_distro = malloc(strlen(dist)+5);
		memset(ssdp_distro, '\0', strlen(dist)+5);
		sprintf(ssdp_distro, "%s/%.1f", dist, version);
		return ssdp_distro;
	}
	return NULL;
}

void ssdp_getethmac(void) {
#if defined(SIOCGIFHWADDR)
	if(!ssdp_mac) {
		ssdp_mac = malloc(13);
		memset(ssdp_mac, '\0', 13);
		struct ifreq s;

		strcpy(s.ifr_name, "eth0");
		if(ioctl(ssdp_socket, SIOCGIFHWADDR, &s) == 0) {
			int i;
			for(i = 0; i < 12; i+=2) {
				sprintf(&ssdp_mac[i], "%02x", (unsigned char)s.ifr_addr.sa_data[i/2]);
			}
		}
	}
#elif defined(HAVE_GETIFADDRS)
	ifaddrs* iflist;
    if(getifaddrs(&iflist) == 0) {
        for(ifaddrs* cur = iflist; cur; cur = cur->ifa_next) {
            if((cur->ifa_addr->sa_family == AF_LINK) && (strcmp(cur->ifa_name, if_name) == 0) && cur->ifa_addr) {
                sockaddr_dl* sdl = (sockaddr_dl*)cur->ifa_addr;
                memcpy(&ssdp_mac[i], LLADDR(sdl), sdl->sdl_alen);
                break;
            }
        }

        freeifaddrs(iflist);
    }
#elif defined(SIOCGIFADDR)
	if(!ssdp_mac) {
		ssdp_mac = malloc(13);
		memset(ssdp_mac, '\0', 13);
		struct ifreq s;

		strcpy(s.ifr_name, "re0");
		if(ioctl(ssdp_socket, SIOCGIFADDR, &s) == 0) {
			int i;
			for(i = 0; i < 12; i+=2) {
				sprintf(&ssdp_mac[i], "%02x", (unsigned char)s.ifr_addr.sa_data[i/2]);
			}
		}
	}
#endif
}

unsigned long ssdp_genid(void) {
	ssdp_getethmac();
	char *end;
	unsigned long i = (unsigned long)strtol(ssdp_mac, &end, 12);
	return i;
}

char *ssdp_genuuid(void) {
	char *upnp_id = malloc(21);
	memset(upnp_id, '\0', 21);
	unsigned long r;
	unsigned int a, b, c, d, e, f;

	r = ssdp_genid();
	a = (r>> 0) & 0xFFFF;
	b = (r>>16) & 0xFF;
	c = (r>>24) & 0xFF;
	r = ssdp_genid();
	d = (r>> 0) & 0xFF;
	e = (r>> 8) & 0xFFFF;
	f = (r>>24) & 0xFF;

	sprintf(upnp_id, "%04X-%02X-%02X-%02X-%04X%02X", (a&0xFFFF), (b&0xFF), (c&0xFF), (d&0xFF), (e&0xFFFF), (f&0xFF));

	return upnp_id;
}

int ssdp_start(void) {
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	int opt = 1;

	ssdp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(1900);

	if(setsockopt(ssdp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) {
		perror("setsockopt");
		return 0;
	}

	mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if(setsockopt(ssdp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		perror("setsockopt");
		return 0;
	}

	if(bind(ssdp_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		return 0;
	}
	return 1;
}

int ssdp_seek(struct ssdp_list_t **ssdp_list) {
	struct sockaddr_in addr;
	struct timeval tv;
	char message[BUFFER_SIZE] = {'\0'};
	char header[BUFFER_SIZE] = {'\0'};
	int len = 1, sock, match = 0;
	socklen_t addrlen = sizeof(addr);
	unsigned short int nip[4], port = 0;
	
	tv.tv_sec = 0;
	tv.tv_usec = 10000;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1900);
    addr.sin_addr.s_addr = inet_addr("239.255.255.250");
	
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));	
	
	strcpy(header,  "M-SEARCH * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"ST:urn:schemas-upnp-org:service:pilight:1\r\n"
					"Man:\"ssdp:discover\"\r\n"
					"MX:3\r\n\r\n");
	if((len = sendto(sock, header, BUFFER_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr))) >= 0) {
		logprintf(LOG_DEBUG, "ssdp sent search");
	}
	
	while(ssdp_loop) {
		memset(message, '\0', BUFFER_SIZE);
		if(recvfrom(sock, message, sizeof(message), 0, (struct sockaddr *)&addr, &addrlen) < 1) {
			//perror("read");
			goto end;
		}
		if(strstr(message, "pilight") > 0) {
			char *pch = strtok(message, "\r\n");
			while(pch) {
				if(sscanf(pch, "Location:%hu.%hu.%hu.%hu:%hu\r\n", &nip[0], &nip[1], &nip[2], &nip[3], &port) > 0) {
					match = 1;
					break;
				}
				pch = strtok(NULL, "\r\n");
			}

			struct ssdp_list_t *node = malloc(sizeof(ssdp_list_t));
			sprintf(node->ip, "%hu.%hu.%hu.%hu", nip[0], nip[1], nip[2], nip[3]);
			node->ip[16] = '\0';
			node->port = port;
			node->next = *ssdp_list;
			*ssdp_list = node;
		}
	}
	goto end;
	
end:
	close(sock);
    struct ssdp_list_t *ptr = *ssdp_list, *next = NULL, *prev = NULL;
   	if(match) {
		while(ptr) {
			next = ptr->next;
			ptr->next = prev;
			prev = ptr;
			ptr = next;
		}
		sfree((void *)&ptr);
		sfree((void *)&next);
		*ssdp_list = prev;
		return 0;
	} else {
		return -1;
	}
}

void *ssdp_wait(void *param) {
	struct sockaddr_in addr;
	struct ifaddrs *ifaddr, *ifa;
	char message[BUFFER_SIZE];
	char host[NI_MAXHOST];
	char *id = ssdp_genuuid();
	int x = 0;
	int len;
	socklen_t addrlen = sizeof(addr);
	int family, s = 0;

	if(getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	char *ssdp_distro = ssdp_getdistroname();	
	
	if(!ssdp_distro) {
		logprintf(LOG_ERR, "failed to determine the distribution", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr == NULL) {
			continue;
		}

		family = ifa->ifa_addr->sa_family;

		if(strcmp(ifa->ifa_name, "lo") != 0 && family == AF_INET) {
			if(family == AF_INET || family == AF_INET6) {
				s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
				if(s != 0) {
					logprintf(LOG_ERR, "getnameinfo() failed: %s", gai_strerror(s));
					exit(EXIT_FAILURE);
				}
				x++;
				memset(sspd_header[x], '\0', BUFFER_SIZE);	
				sprintf(sspd_header[x], "NOTIFY * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"Cache-Control:max-age=900\r\n"
					"Location:%s:%d\r\n"
					"NT:urn:schemas-upnp-org:service:pilight:1\r\n"
					"USN:uuid:%s::urn:schemas-upnp-org:service:pilight:1\r\n"
					"NTS:ssdp:alive\r\n"
					"SERVER: %s UPnP/1.1 pilight (%s)/%s\r\n\r\n", host, socket_get_port(), id, ssdp_distro, ssdp_gethostname(), VERSION);
			}
		}
	}

	freeifaddrs(ifaddr);

	sfree((void *)&id);
	sfree((void *)&ssdp_distro);
	sfree((void *)&ssdp_hostname);
	sfree((void *)&ssdp_mac);

	while(ssdp_loop) {
		memset(message, '\0', BUFFER_SIZE);
		if(recvfrom(ssdp_socket, message, sizeof(message), 0, (struct sockaddr *)&addr, &addrlen) < 1) {
			//perror("read");
			break;
		}
		if(strstr(message, "M-SEARCH * HTTP/1.1") > 0 && strstr(message, "urn:schemas-upnp-org:service:pilight:1") > 0) {
			for(x=0;x<2;x++) {
				if(strlen(sspd_header[x]) > 0) {
					if((len = sendto(ssdp_socket, sspd_header[x], BUFFER_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr))) >= 0) {
						logprintf(LOG_DEBUG, "ssdp sent notify");
					}
				}
			}
		}
	}
	return 0;
}

void ssdp_close(int sock) {
	close(sock);
}
