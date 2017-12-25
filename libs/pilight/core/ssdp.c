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
	#if _WIN32_WINNT < 0x0501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#define WIN32_LEAN_AND_MEAN
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

#include "pilight.h"
#include "network.h"
#include "socket.h"
#include "log.h"
#include "ssdp.h"

#include "../../libuv/uv.h"

#define CLIENT	0
#define SERVER	1

struct data_t {
	uv_timer_t *timer_req;
	uv_udp_t *ssdp_req;
	int type;

	struct data_t *next;
} data_t;

/*
 * Override values for unit testing
 */
static int ssdp_override_port = -1;
static char ssdp_override_server[INET_ADDRSTRLEN+1] = { 0 };

static uv_mutex_t nodes_lock;
// static pthread_mutexattr_t nodes_attr;
static int lock_init = 0;
static int ssdp_started = 0;
static struct data_t *data = NULL;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

void ssdp_override(char *server, int port) {
	ssdp_override_port = port;
	if(server == NULL) {
		memset(&ssdp_override_server, '\0', INET_ADDRSTRLEN+1);
	} else {
		strcpy(ssdp_override_server, server);
	}
}

void ssdp_gc(void) {
	struct data_t *nodes = NULL;

	uv_mutex_lock(&nodes_lock);
	while(data) {
		nodes = data;
		if(nodes->type == CLIENT && data->timer_req != NULL) {
			uv_timer_stop(data->timer_req);
		}
		uv_udp_recv_stop(data->ssdp_req);
		uv_close((uv_handle_t *)data->ssdp_req, close_cb);
		data = data->next;
		FREE(nodes);
	}
	uv_mutex_unlock(&nodes_lock);
	ssdp_started = 0;
}

static void node_remove(struct data_t *node) {
	uv_mutex_lock(&nodes_lock);
	struct data_t *currP, *prevP;

	prevP = NULL;

	for(currP = data; currP != NULL; prevP = currP, currP = currP->next) {
		if(currP == node) {
			if(prevP == NULL) {
				data = currP->next;
			} else {
				prevP->next = currP->next;
			}

			if(node->type == CLIENT) {
				uv_timer_stop(node->timer_req);
			}
			uv_udp_recv_stop(node->ssdp_req);
			uv_close((uv_handle_t *)node->ssdp_req, close_cb);

			FREE(node);
			break;
		}
	}
	uv_mutex_unlock(&nodes_lock);
}

void *reason_ssdp_received_free(void *param) {
	if(param != NULL) {
		struct reason_ssdp_received_t *data = param;
		FREE(data);
	}
	return NULL;
}
/*
void *reason_ssdp_disconnected_free(void *param) {
	struct reason_ssdp_disconnected_t *data = param;
	FREE(data);
	return NULL;
}

static void *client_timeout(void *param) {
	uv_mutex_lock(&client_lock);

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
	uv_mutex_unlock(&client_lock);
	return NULL;
}
*/
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
			if(strlen(ssdp_override_server) > 0 || dev2ip(devs[x], &p, AF_INET) == 0) {
				if((id = genuuid(devs[x])) == NULL) {
					logprintf(LOG_ERR, "could not generate the device uuid");
					continue;
				}
				if((*header = REALLOC(*header, sizeof(char *)*((size_t)nrheader+1))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				if(((*header)[nrheader] = MALLOC(BUFFER_SIZE)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				int port = socket_get_port();
				if(ssdp_override_port > -1) {
					port = ssdp_override_port;
				}
				if(strlen(ssdp_override_server) > 0) {
					strcpy(host, ssdp_override_server);
				}
				memset((*header)[nrheader], '\0', BUFFER_SIZE);
				sprintf((*header)[nrheader], "NOTIFY * HTTP/1.1\r\n"
					"Host:239.255.255.250:1900\r\n"
					"Cache-Control:max-age=900\r\n"
					"Location:%s:%d\r\n"
					"NT:urn:schemas-upnp-org:service:pilight:1\r\n"
					"USN:uuid:%s::urn:schemas-upnp-org:service:pilight:1\r\n"
					"NTS:ssdp:alive\r\n"
					"SERVER:%s UPnP/1.1 pilight (%s)/%s\r\n\r\n", host, port, id, distro, name, PILIGHT_VERSION);
				nrheader++;
				FREE(id);
			}
		}
	} else {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "could not determine default network interface");
		return -1;
		/*LCOV_EXCL_STOP*/
	}
	array_free(&devs, nrdevs);

	if(id != NULL) {
		FREE(id);
	}
	FREE(distro);
	FREE(hname);

	return nrheader;
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
/*
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
		// pthread_mutexattr_init(&client_attr);
		// pthread_mutexattr_settype(&client_attr, PTHREAD_MUTEX_RECURSIVE);
		// pthread_mutex_init(&client_lock, &client_attr);	
		uv_mutex_init(&client_lock);	
		client_lock_init = 1;
	}
	
	uv_mutex_lock(&client_lock);
	if(ssdp_client != NULL) {
		eventpool_fd_remove(ssdp_client);
		ssdp_client = NULL;
	}
	uv_mutex_unlock(&client_lock);

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work("ssdp client", client_timeout, tv, NULL);
	ssdp_client = eventpool_socket_add("ssdp client", "239.255.255.250", 1900, AF_INET, SOCK_DGRAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, client_callback, NULL, NULL);
}

*/

static void on_send(uv_udp_send_t *req, int status) {
	if(req->data != NULL) {
		FREE(req->data);
	}
	FREE(req);
}

static void alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	if((buf->base = malloc(len)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(buf->base, 0, len);
}

static void read_cb(uv_udp_t *stream, ssize_t len, const uv_buf_t *buf, const struct sockaddr *addr, unsigned int port) {
	struct data_t *node = stream->data;
	char name[BUFFER_SIZE];
	char *p = name;

	if(node->type == SERVER) {
		if(strstr(buf->base, "urn:schemas-upnp-org:service:pilight:1") != NULL) {
			char **header = NULL;
			int nrheader = ssdp_create_header(&header), x = 0;

			if(nrheader == -1) {
				/*LCOV_EXCL_START*/
				logprintf(LOG_ERR, "could not generate ssdp header");
				return;
				/*LCOV_EXCL_STOP*/
			}
			for(x=0;x<nrheader;x++) {
				if(strlen(header[x]) > 0) {
					uv_udp_send_t *send_req = MALLOC(sizeof(uv_udp_send_t));
					if(send_req == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					memset(send_req, 0, sizeof(uv_udp_send_t));

					uv_buf_t buf1 = uv_buf_init(header[x], strlen(header[x]));
					/*
					 * Freeing header[x] immediatly creates
					 * unaddressable memory in udp send.
					 */
					send_req->data = header[x];

					uv_udp_send(send_req, node->ssdp_req, &buf1, 1, addr, on_send);
				}
			}
			FREE(header);
		}
	} else if(node->type == CLIENT) {
		if(len > 0 && strstr(buf->base, "pilight") > 0) {
			char **array = NULL, uuid[21], *x = uuid;
			unsigned short int nip[4], port = 0, match1 = 0, match2 = 0, match3 = 0;
			unsigned int n = explode(buf->base, "\r\n", &array), q = 0;
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

				struct reason_ssdp_received_t *data1 = MALLOC(sizeof(struct reason_ssdp_received_t));
				if(data1 == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(data1->name, name);
				strcpy(data1->uuid, uuid);
				strcpy(data1->ip, buf);
				data1->port = port;

				eventpool_trigger(REASON_SSDP_RECEIVED, reason_ssdp_received_free, data1);
			}
		}
	}

	free(buf->base);
}

void ssdp_start(void) {
	if(ssdp_started == 1) {
		return;
	}
	ssdp_started = 1;
	struct sockaddr_in addr;
	int r = 0;

	if(lock_init == 0) {
		lock_init = 1;
		// pthread_mutexattr_init(&nodes_attr);
		// pthread_mutexattr_settype(&nodes_attr, PTHREAD_MUTEX_RECURSIVE);
		// pthread_mutex_init(&nodes_lock, &nodes_attr);
		uv_mutex_init(&nodes_lock);
	}	
	
	struct data_t *node = MALLOC(sizeof(struct data_t));
	node->type = SERVER;
	node->timer_req = NULL;
	if((node->ssdp_req = MALLOC(sizeof(uv_udp_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	node->ssdp_req->data = node;

	r = uv_udp_init(uv_default_loop(), node->ssdp_req);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}
	r = uv_ip4_addr("0.0.0.0", 1900, &addr);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_ip4_addr: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}
	r = uv_udp_bind(node->ssdp_req, (const struct sockaddr *)&addr, UV_UDP_REUSEADDR);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_bind: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}
	r = uv_udp_set_membership(node->ssdp_req, "239.255.255.250", NULL, UV_JOIN_GROUP);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_set_membership: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}
	r = uv_udp_recv_start(node->ssdp_req, alloc, read_cb);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_recv_start: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	uv_mutex_lock(&nodes_lock);
	node->next = data;
	data = node;
	uv_mutex_unlock(&nodes_lock);

	logprintf(LOG_INFO, "ssdp server started");

	return;

close:
	FREE(node->ssdp_req);
	FREE(node);
	return;
	// eventpool_callback(REASON_ADHOC_CONNECTED, adhoc_mode);
	// eventpool_callback(REASON_ADHOC_DISCONNECTED, ssdp_restart);
}

static void stop(uv_timer_t *handle) {
	struct data_t *data = handle->data;
	uv_close((uv_handle_t *)handle, close_cb);
	uv_udp_recv_stop(data->ssdp_req);
	// uv_close((uv_handle_t *)data->ssdp_req, close_cb);
	node_remove(handle->data);
}

void ssdp_seek(void) {
	struct sockaddr_in addr;
	uv_udp_send_t *send_req = NULL;
	int r = 0;

	if(lock_init == 0) {
		lock_init = 1;
		// pthread_mutexattr_init(&nodes_attr);
		// pthread_mutexattr_settype(&nodes_attr, PTHREAD_MUTEX_RECURSIVE);
		// pthread_mutex_init(&nodes_lock, &nodes_attr);
		uv_mutex_init(&nodes_lock);
	}
	
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
#endif

	struct data_t *node = MALLOC(sizeof(struct data_t));
	node->type = CLIENT;
	if((node->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	if((node->ssdp_req = MALLOC(sizeof(uv_udp_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	
	node->ssdp_req->data = node;
	node->timer_req->data = node;	
	
	r = uv_udp_init(uv_default_loop(), node->ssdp_req);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		return;
		/*LCOV_EXCL_STOP*/
	}
	
	if((send_req = MALLOC(sizeof(uv_udp_send_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	memset(send_req, 0, sizeof(uv_udp_send_t));

	char *msg = "M-SEARCH * HTTP/1.1\r\n"
			"Host:239.255.255.250:1900\r\n"
			"ST:urn:schemas-upnp-org:service:pilight:1\r\n"
			"MAN:\"ssdp:discover\"\r\n"
			"MX:3\r\n\r\n";

	uv_buf_t buf = uv_buf_init(msg, strlen(msg));

	r = uv_ip4_addr("239.255.255.250", 1900, &addr);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_ip4_addr: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}
	r = uv_udp_send(send_req, node->ssdp_req, &buf, 1, (const struct sockaddr *)&addr, on_send);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_send: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}
	r = uv_udp_recv_start(node->ssdp_req, alloc, read_cb);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_recv_start: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	uv_mutex_lock(&nodes_lock);
	node->next = data;
	data = node;
	uv_mutex_unlock(&nodes_lock);

	uv_timer_init(uv_default_loop(), node->timer_req);
	uv_timer_start(node->timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	return;

close:
	FREE(node->timer_req);
	FREE(node->ssdp_req);
	FREE(node);
	return;
}