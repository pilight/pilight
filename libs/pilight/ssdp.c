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
#include <sys/stat.h>
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
#include "gc.h"
#include "ssdp.h"

char *ssdp_mac = NULL;
char *ssdp_hostname = NULL;
char **ssdp_header = NULL;
int ssdp_nrheader = 0;
int ssdp_socket = 0;
int ssdp_loop = 1;

int ssdp_gc(void) {
	int x = 0;
	for(x=0;x<ssdp_nrheader;x++) {
		sfree((void *)&ssdp_header[x]);
	}
	sfree((void *)&ssdp_header);
	return EXIT_SUCCESS;
}

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
	int rc = 1;
	char dist[32];
	memset(dist, '\0', 32);
	struct stat sb;	
	char *ssdp_distro = NULL;

#ifdef __FreeBSD__
	strcpy(dist, "FreeBSD/0.0");
#else
	if((rc = stat("/etc/redhat-release", &sb)) == 0) {
		strcpy(dist, "RedHat/0.0");
	} else if((rc = stat("/etc/SuSE-release", &sb)) == 0) {
		strcpy(dist, "SuSE/0.0");
	} else if((rc = stat("/etc/mandrake-release", &sb)) == 0) {
		strcpy(dist, "Mandrake/0.0");
	} else if((rc = stat("/etc/debian-release", &sb)) == 0) {
		strcpy(dist, "Debian/0.0");
	} else if((rc = stat("/etc/debian_version", &sb)) == 0) {
		strcpy(dist, "Debian/0.0");
	} else {
		strcpy(dist, "Unknown/0.0");
	}
#endif
	if(strlen(dist) > 0) {
		ssdp_distro = malloc(strlen(dist)+1);
		strcpy(ssdp_distro, dist);
		return ssdp_distro;
	} else {
		return NULL;
	}
}

void ssdp_getethmac(void) {
#if defined(SIOCGIFHWADDR)
	if(!ssdp_mac) {
		int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
		ssdp_mac = malloc(13);
		memset(ssdp_mac, '\0', 13);
		struct ifreq s;

		strcpy(s.ifr_name, "eth0");
		if(ioctl(fd, SIOCGIFHWADDR, &s) == 0) {
			int i;
			for(i = 0; i < 12; i+=2) {
				sprintf(&ssdp_mac[i], "%02x", (unsigned char)s.ifr_addr.sa_data[i/2]);
			}
		}
		close(fd);
	}
#elif defined(SIOCGIFADDR)
	if(!ssdp_mac) {
		int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
		ssdp_mac = malloc(13);
		memset(ssdp_mac, '\0', 13);
		struct ifreq s;

		strcpy(s.ifr_name, "re0");
		if(ioctl(fd, SIOCGIFADDR, &s) == 0) {
			int i;
			for(i = 0; i < 12; i+=2) {
				sprintf(&ssdp_mac[i], "%02x", (unsigned char)s.ifr_addr.sa_data[i/2]);
			}
		}
		close(fd);
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
#endif
}

unsigned long ssdp_genid(void) {
	ssdp_getethmac();
	int i = 0;
	unsigned long x = 0;
	if(ssdp_mac && strlen(ssdp_mac) > 0) {
		for(i=0;i<12;i++) {
			x += (unsigned long)ssdp_mac[i];
		}

		return x;
	} else {
		return 0;
	}
}

char *ssdp_genuuid(void) {
	unsigned long r = ssdp_genid();
	if(r > 0) {
	
		char *upnp_id = malloc(UUID_LENGTH);
		memset(upnp_id, '\0', UUID_LENGTH);
		unsigned int a, b, c, d, e, f;	

		a = (r>> 0) & 0xFFFF;
		b = (r>>16) & 0xFF;
		c = (r>>24) & 0xFF;
		if(r > 0) {
			r = ssdp_genid();
			d = (r>> 0) & 0xFF;
			e = (r>> 8) & 0xFFFF;
			f = (r>>24) & 0xFF;

			sprintf(upnp_id, "%04X-%02X-%02X-%02X-%04X%02X", (a&0xFFFF), (b&0xFF), (c&0xFF), (d&0xFF), (e&0xFFFF), (f&0xFF));
		} else {
			sfree((void *)&ssdp_mac);
			return NULL;
		}
		sfree((void *)&ssdp_mac);
		return upnp_id;
	} else {
		return NULL;
	}
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
	int sock, match = 0;
	ssize_t len = 0;
	socklen_t addrlen = sizeof(addr);
	unsigned short int nip[4], port = 0;
	
	tv.tv_sec = 0;
	tv.tv_usec = 100000;

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
			if(match) {
				struct ssdp_list_t *node = malloc(sizeof(struct ssdp_list_t));
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

#ifdef __FreeBSD__
static struct sockaddr *sockaddr_dup(struct sockaddr *sa) {
	struct sockaddr *ret;
	socklen_t socklen;
#ifdef HAVE_SOCKADDR_SA_LEN
	socklen = sa->sa_len;
#else
	socklen = sizeof(struct sockaddr_storage);
#endif
	ret = calloc(1, socklen);
	if (ret == NULL)
		return NULL;
	memcpy(ret, sa, socklen);
	return ret;
}

int rep_getifaddrs(struct ifaddrs **ifap) {
	struct ifconf ifc;
	char buff[8192];
	int fd, i, n;
	struct ifreq ifr, *ifrp=NULL;
	struct ifaddrs *curif = NULL, *ifa = NULL;
	struct ifaddrs *lastif = NULL;

	*ifap = NULL;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}
  
	ifc.ifc_len = sizeof(buff);
	ifc.ifc_buf = buff;

	if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
		close(fd);
		return -1;
	}
  
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifdef __FreeBSD__
#define ifreq_size(i) max(sizeof(struct ifreq),\
     sizeof((i).ifr_name)+(i).ifr_addr.sa_len)
#else
#define ifreq_size(i) sizeof(struct ifreq)  
#endif

	n = ifc.ifc_len;

	/* Loop through interfaces, looking for given IP address */
	for (i = 0; i < n; i+= ifreq_size(*ifrp) ) {
		int match = 0;
		ifrp = (struct ifreq *)((char *) ifc.ifc_buf+i);
		for(ifa = *ifap; ifa != NULL; ifa = ifa->ifa_next) {
			if(strcmp(ifrp->ifr_name, ifa->ifa_name) == 0) {
				match = 1;
				break;
			}
		}
		if(match == 1) {
			continue;
		}
		curif = calloc(1, sizeof(struct ifaddrs));
		if (curif == NULL) {
			freeifaddrs(*ifap);
			close(fd);
			return -1;
		}

		curif->ifa_name = malloc(sizeof(IFNAMSIZ)+1);
		strncpy(curif->ifa_name, ifrp->ifr_name, IFNAMSIZ);
		strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);
		if (curif->ifa_name == NULL) {
			free(curif);
			freeifaddrs(*ifap);
			close(fd);
			return -1;
		}
	
		curif->ifa_flags = ifr.ifr_flags;
		curif->ifa_dstaddr = NULL;
		curif->ifa_data = NULL;
		curif->ifa_next = NULL;

		curif->ifa_addr = NULL;
		if (ioctl(fd, SIOCGIFADDR, &ifr) != -1) {
			curif->ifa_addr = sockaddr_dup(&ifr.ifr_addr);
			if (curif->ifa_addr == NULL) {
				free(curif->ifa_name);
				free(curif);
				freeifaddrs(*ifap);
				close(fd);
				return -1;
			}
		}


		curif->ifa_netmask = NULL;
		if (ioctl(fd, SIOCGIFNETMASK, &ifr) != -1) {
			curif->ifa_netmask = sockaddr_dup(&ifr.ifr_addr);
			if (curif->ifa_netmask == NULL) {
				if (curif->ifa_addr != NULL) {
					free(curif->ifa_addr);
				}
				free(curif->ifa_name);
				free(curif);
				freeifaddrs(*ifap);
				close(fd);
				return -1;
			}
		}

		if (lastif == NULL) {
			*ifap = curif;
		} else {
			lastif->ifa_next = curif;
		}
		lastif = curif;
	}

	close(fd);

	return 0;
}
#endif

void *ssdp_wait(void *param) {

	gc_attach(ssdp_gc);

	struct sockaddr_in addr;
	struct ifaddrs *ifaddr, *ifa;
	char message[BUFFER_SIZE];
	char host[NI_MAXHOST];
	int x = 0;
	ssize_t len = 0;
	socklen_t addrlen = sizeof(addr);
	int family = 0, s = 0;

	char *id = ssdp_genuuid();	
	char *ssdp_distro = ssdp_getdistroname();	
	
	if(id == NULL) {
		logprintf(LOG_ERR, "could not generate uuid");
		exit(EXIT_FAILURE);
	}
	
	if(!ssdp_distro) {
		logprintf(LOG_ERR, "failed to determine the distribution", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

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
		if((strstr(ifa->ifa_name, "lo") == NULL && strstr(ifa->ifa_name, "vbox") == NULL) 
		    && (family == AF_INET || AF_INET6)) {
			memset(host, '\0', NI_MAXHOST);
			s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if(s != 0) {
				logprintf(LOG_ERR, "getnameinfo() failed: %s", gai_strerror(s));
				exit(EXIT_FAILURE);
			}
			if(strlen(host) > 0) {
				ssdp_header = realloc(ssdp_header, sizeof(char *)*((unsigned int)ssdp_nrheader+1));
				ssdp_header[ssdp_nrheader] = malloc(BUFFER_SIZE);
				memset(ssdp_header[ssdp_nrheader], '\0', BUFFER_SIZE);	
				sprintf(ssdp_header[ssdp_nrheader], "NOTIFY * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"Cache-Control:max-age=900\r\n"
					"Location:%s:%d\r\n"
					"NT:urn:schemas-upnp-org:service:pilight:1\r\n"
					"USN:uuid:%s::urn:schemas-upnp-org:service:pilight:1\r\n"
					"NTS:ssdp:alive\r\n"
					"SERVER: %s UPnP/1.1 pilight (%s)/%s\r\n\r\n", host, socket_get_port(), id, ssdp_distro, ssdp_gethostname(), VERSION);
				ssdp_nrheader++;	
			}
		}
	}

	freeifaddrs(ifaddr);

	sfree((void *)&id);
	sfree((void *)&ssdp_distro);
	sfree((void *)&ssdp_hostname);

	while(ssdp_loop) {
		memset(message, '\0', BUFFER_SIZE);
		if(recvfrom(ssdp_socket, message, sizeof(message), 0, (struct sockaddr *)&addr, &addrlen) < 1) {
			//perror("read");
			break;
		}
		if(strstr(message, "M-SEARCH * HTTP/1.1") > 0 && strstr(message, "urn:schemas-upnp-org:service:pilight:1") > 0) {
			for(x=0;x<ssdp_nrheader;x++) {
				if(strlen(ssdp_header[x]) > 0) {
					if((len = sendto(ssdp_socket, ssdp_header[x], BUFFER_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr))) >= 0) {
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
