/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "threadpool.h"
#include "pilight.h"
#include "network.h"
#include "socket.h"
#include "log.h"
#include "gc.h"
#include "ssdp.h"

static struct eventpool_fd_t *ssdp_server = NULL;
static struct eventpool_fd_t *ssdp_client = NULL;
static int client_lock_init = 0;
static pthread_mutex_t client_lock;
static pthread_mutexattr_t client_attr;

void *reason_ssdp_received_free(void *param) {
	struct reason_ssdp_received_t *data = param;
	FREE(data);
	return NULL;
}

void *reason_ssdp_disconnected_free(void *param) {
	struct reason_ssdp_disconnected_t *data = param;
	FREE(data);
	return NULL;
}

static int ssdp_create_header(char ***header) {
	char **devs = NULL, *id = NULL, host[INET_ADDRSTRLEN+1], *p = host;
	char *distro = distroname();
	char *hname = hostname();
	int nrdevs = 0, x = 0, nrheader = 0;

	if(distro == NULL) {
		logprintf(LOG_ERR, "failed to determine the distribution");
		return -1;
	}

	char *name = NULL;
	if(settings_select_string(ORIGIN_SSDP, "name", &name) != 0) {
		name = hname;
	}

	if((nrdevs = inetdevs(&devs)) > 0) {
		for(x=0;x<nrdevs;x++) {
			if(dev2ip(devs[x], &p, AF_INET) == 0) {
				if((id = genuuid(devs[x])) == NULL) {
					logprintf(LOG_ERR, "could not generate the device uuid");
					continue;
				}
				if((*header = REALLOC(*header, sizeof(char *)*((size_t)nrheader+1))) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				if(((*header)[nrheader] = MALLOC(BUFFER_SIZE)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				memset((*header)[nrheader], '\0', BUFFER_SIZE);
				sprintf((*header)[nrheader], "NOTIFY * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"Cache-Control:max-age=900\r\n"
					"Location:%s:%d\r\n"
					"NT:urn:schemas-upnp-org:service:pilight:1\r\n"
					"USN:uuid:%s::urn:schemas-upnp-org:service:pilight:1\r\n"
					"NTS:ssdp:alive\r\n"
					"SERVER:%s UPnP/1.1 pilight (%s)/%s\r\n\r\n", host, socket_get_port(), id, distro, name, PILIGHT_VERSION);
				nrheader++;
			}
		}
	} else {
		logprintf(LOG_ERR, "could not determine default network interface");
		return -1;
	}
	array_free(&devs, nrdevs);

	if(id != NULL) {
		FREE(id);
	}
	FREE(distro);
	FREE(hname);

	return nrheader;
}

static int server_callback(struct eventpool_fd_t *node, int event) {
	socklen_t socklen = sizeof(node->data.socket.addr);
	char message[BUFFER_SIZE];
	int x = 0;

	switch(event) {
		case EV_SOCKET_SUCCESS: {
			struct ip_mreq mreq;
			int opt = 1;
			mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);

			if(setsockopt(node->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int)) == -1) {
				perror("setsockopt");
				return 0;
			}

			if(setsockopt(node->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
				logprintf(LOG_ERR, "cannot bind to the ssdp multicast network");
				return -1;
			}
		} break;
		case EV_BIND_SUCCESS: {
			node->stage = EVENTPOOL_STAGE_CONNECTED;
			eventpool_fd_enable_read(node);
		} break;
		case EV_READ: {
			memset(message, '\0', BUFFER_SIZE);
			if(recvfrom(node->fd, message, sizeof(message), 0, (struct sockaddr *)&node->data.socket.addr, &socklen) < 1) {
				return -1;
			}

			inet_ntop(AF_INET, (void *)&(node->data.socket.addr.sin_addr), node->data.socket.ip, INET_ADDRSTRLEN+1);
			node->data.socket.port = ntohs(node->data.socket.addr.sin_port);

			if(strstr(message, "M-SEARCH * HTTP/1.1") > 0 && strstr(message, "urn:schemas-upnp-org:service:pilight:1") > 0) {
				char **header = NULL;
				int nrheader = ssdp_create_header(&header);

				if(nrheader == -1) {
					logprintf(LOG_ERR, "could not generate ssdp header");
					return 0;
				}
				for(x=0;x<nrheader;x++) {
					if(strlen(header[x]) > 0) {
						eventpool_fd_write(node->fd, header[x], strlen(header[x]));
					}
				}
				for(x=0;x<nrheader;x++) {
					FREE(header[x]);
				}
				FREE(header);
			}

			eventpool_fd_enable_read(node);
		} break;
	}
	return 0;
}

static int extract_name(char *message, char **name) {
	int s = -1, e = -1;
	char *p = strstr(message, "(");

	if(p != NULL) {
		s = p-message;
	}
	if((p = strstr(message, ")")) != NULL) {
		e = p-message;
	}
	if(s >= 0 && e >= 0 && e > s) {
		strncpy(*name, &message[s+1], e-(s+1));
		(*name)[e-(s+1)] = '\0';
		return 0;
	}
	return -1;
}

static void *client_timeout(void *param) {
	pthread_mutex_lock(&client_lock);

	if(ssdp_client != NULL) {
		struct reason_ssdp_disconnected_t *data = MALLOC(sizeof(struct reason_ssdp_disconnected_t));
		if(data == NULL) {
			OUT_OF_MEMORY
		}
		data->fd = ssdp_client->fd;
		eventpool_trigger(REASON_SSDP_DISCONNECTED, reason_ssdp_disconnected_free, data);

		eventpool_fd_remove(ssdp_client);
		ssdp_client = NULL;
	}
	pthread_mutex_unlock(&client_lock);
	return NULL;
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	char header[BUFFER_SIZE];
	char message[BUFFER_SIZE];
	char name[BUFFER_SIZE];
	char *p = name;

	switch(event) {
		case EV_SOCKET_SUCCESS: {
			node->stage = EVENTPOOL_STAGE_CONNECTED;
			eventpool_fd_enable_write(node);
		} break;
		case EV_CONNECT_SUCCESS: {
			eventpool_fd_enable_write(node);
		}	break;
		case EV_READ: {
			memset(message, '\0', BUFFER_SIZE);
			if(recvfrom(node->fd, message, sizeof(message), 0, (struct sockaddr *)&addr, &socklen) < 1) {
				return -1;
			}
			if(strstr(message, "pilight") > 0) {
				char **array = NULL, uuid[21], *x = uuid;
				unsigned short int nip[4], port = 0, match1 = 0, match2 = 0, match3 = 0;
				unsigned int n = explode(message, "\r\n", &array), q = 0;
				memset(&uuid, '\0', 21);
				for(q=0;q<n;q++) {
					if(match1 == 0 && sscanf(array[q], "Location:%hu.%hu.%hu.%hu:%hu\r\n", &nip[0], &nip[1], &nip[2], &nip[3], &port) > 0) {
						match1 = 1;
					}
					if(match3 == 0 && sscanf(array[q], "USN:uuid:%20[A-Za-z0-9-]::urn:schemas-upnp-org:service:pilight:1\r\n", x) > 0) {
						match3 = 1;
					}
					if(match2 == 0) {
						match2 = (extract_name(array[q], &p) == 0);
					}
					if(match1 == 1 && match2 == 1 && match3 == 1) {
						break;
					}
				}

				array_free(&array, n);
				if(match1 == 1 && match2 == 1 && match3 == 1) {
					char buf[INET_ADDRSTRLEN+1];
					snprintf(buf, INET_ADDRSTRLEN, "%d.%d.%d.%d", nip[0], nip[1], nip[2], nip[3]);

					struct reason_ssdp_received_t *data = MALLOC(sizeof(struct reason_ssdp_received_t));
					if(data == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(data->name, name);
					strcpy(data->uuid, uuid);
					strcpy(data->ip, buf);
					data->port = port;

					eventpool_trigger(REASON_SSDP_RECEIVED, reason_ssdp_received_free, data);
				}
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_WRITE: {
			memset(header, '\0', BUFFER_SIZE);
			strcpy(header, "M-SEARCH * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"ST:urn:schemas-upnp-org:service:pilight:1\r\n"
					"Man:\"ssdp:discover\"\r\n"
					"MX:3\r\n\r\n");

			memset((void *)&addr, '\0', sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(1900);
			addr.sin_addr.s_addr = inet_addr("239.255.255.250");

			if(sendto(node->fd, header, BUFFER_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr)) >= 0) {
				logprintf(LOG_DEBUG, "ssdp sent search");
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_DISCONNECTED: {
			pthread_mutex_lock(&client_lock);
			if(ssdp_client != NULL) {
				struct reason_ssdp_disconnected_t *data = MALLOC(sizeof(struct reason_ssdp_disconnected_t));
				if(data == NULL) {
					OUT_OF_MEMORY
				}
				data->fd = node->fd;
				eventpool_trigger(REASON_SSDP_DISCONNECTED, reason_ssdp_disconnected_free, data);

				eventpool_fd_remove(ssdp_client);
				ssdp_client = NULL;
				pthread_mutex_unlock(&client_lock);
			} else {
				pthread_mutex_unlock(&client_lock);
				break;
			}
		} break;
	}
	return 0;
}

static void *adhoc_mode(void *param) {
	if(ssdp_server != NULL) {
		eventpool_fd_remove(ssdp_server);
		logprintf(LOG_INFO, "shut down ssdp server due to adhoc mode");
		ssdp_server = NULL;
	}

	return NULL;
}

static void *ssdp_restart(void *param) {
	if(ssdp_server != NULL) {
		eventpool_fd_remove(ssdp_server);
		ssdp_server = NULL;
	}

	ssdp_server = eventpool_socket_add("ssdp server", NULL, 1900, AF_INET, SOCK_DGRAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, NULL, NULL);

	return NULL;
}

void ssdp_seek(void) {
	struct timeval tv;

	if(client_lock_init == 0) {	
		pthread_mutexattr_init(&client_attr);
		pthread_mutexattr_settype(&client_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&client_lock, &client_attr);	
		client_lock_init = 1;
	}
	
	pthread_mutex_lock(&client_lock);
	if(ssdp_client != NULL) {
		eventpool_fd_remove(ssdp_client);
		ssdp_client = NULL;
	}
	pthread_mutex_unlock(&client_lock);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work("ssdp client", client_timeout, tv, NULL);
	ssdp_client = eventpool_socket_add("ssdp client", "239.255.255.250", 1900, AF_INET, SOCK_DGRAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, client_callback, NULL, NULL);
}

void ssdp_start(void) {

	if(ssdp_server != NULL) {
		eventpool_fd_remove(ssdp_server);
		ssdp_server = NULL;
	}
	ssdp_server = eventpool_socket_add("ssdp server", NULL, 1900, AF_INET, SOCK_DGRAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, NULL, NULL);
	eventpool_callback(REASON_ADHOC_CONNECTED, adhoc_mode);
	eventpool_callback(REASON_ADHOC_DISCONNECTED, ssdp_restart);
}
