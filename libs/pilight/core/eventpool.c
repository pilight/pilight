/*
	Copyright (C) 2015 CurlyMo

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

#include "eventpool.h"

#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <poll.h>

#include "log.h"
#include "gc.h"
#include "threadpool.h"
#include "mem.h"
#include "network.h"

static int nrfd = 0;
static int nrpollfds = 0;
static int nrlisteners[REASON_END] = {0};

static int threads = EVENTPOOL_NO_THREADS;
static pthread_mutex_t listeners_lock;

static struct eventpool_fd_t **eventpool_fds = NULL;
static struct eventpool_listener_t *eventpool_listeners = NULL;

static struct reasons_t {
	int number;
	const char *reason;
	int priority;
} reasons[REASON_END+1] = {
	{	REASON_SEND_CODE, 						"REASON_SEND_CODE",							0 },
	{	REASON_CODE_SENT, 						"REASON_CODE_SENT",							0 },
	{	REASON_CODE_RECEIVED, 				"REASON_CODE_RECEIVED",					0 },
	{	REASON_RECEIVED_PULSETRAIN, 	"REASON_RECEIVED_PULSETRAIN",		0 },
	{	REASON_BROADCAST, 						"REASON_BROADCAST",							0 },
	{	REASON_BROADCAST_CORE, 				"REASON_BROADCAST_CORE",				0 },
	{	REASON_FORWARD, 							"REASON_FORWARD",								0 },
	{	REASON_CONFIG_UPDATE, 				"REASON_CONFIG_UPDATE",					0 },
	{	REASON_SOCKET_RECEIVED, 			"REASON_SOCKET_RECEIVED",				0 },
	{	REASON_SOCKET_DISCONNECTED,	 	"REASON_SOCKET_DISCONNECTED",		0 },
	{	REASON_SOCKET_CONNECTED,			"REASON_SOCKET_CONNECTED",			0 },
	{	REASON_SOCKET_SEND,						"REASON_SOCKET_SEND",						0 },
	{	REASON_SSDP_RECEIVED, 				"REASON_SSDP_RECEIVED",					0 },
	{	REASON_SSDP_RECEIVED_FREE,		"REASON_SSDP_RECEIVED_FREE",		0 },
	{	REASON_SSDP_DISCONNECTED,			"REASON_SSDP_DISCONNECTED",			0 },
	{	REASON_SSDP_CONNECTED,				"REASON_SSDP_CONNECTED",				0 },
	{	REASON_WEBSERVER_CONNECTED,		"REASON_WEBSERVER_CONNECTED",		0 },
	{	REASON_DEVICE_ADDED,					"REASON_DEVICE_ADDED",					0 },
	{	REASON_ADHOC_MODE,						"REASON_ADHOC_MODE",						0 },
	{	REASON_ADHOC_CONNECTED,				"REASON_ADHOC_CONNECTED",				0 },
	{	REASON_ADHOC_CONFIG_RECEIVED,	"REASON_ADHOC_CONFIG_RECEIVED",	0 },
	{	REASON_ADHOC_DATA_RECEIVED,		"REASON_ADHOC_DATA_RECEIVED",		0 },
	{	REASON_ADHOC_UPDATE_RECEIVED,	"REASON_ADHOC_UPDATE_RECEIVED",	0 },
	{	REASON_ADHOC_DISCONNECTED,		"REASON_ADHOC_DISCONNECTED",		0 },
	{	REASON_SEND_BEGIN,						"REASON_SEND_BEGIN",						0 },
	{	REASON_SEND_END,							"REASON_SEND_END",							0 },
	{	REASON_END,										"REASON_END",										0 }
};

void eventpool_callback(int reason, void *(*func)(void *)) {
	pthread_mutex_lock(&listeners_lock);
	struct eventpool_listener_t *node = MALLOC(sizeof(struct eventpool_listener_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	node->func = func;
	node->reason = reason;
	node->next = NULL;

	node->next = eventpool_listeners;
	eventpool_listeners = node;

	__sync_add_and_fetch(&nrlisteners[reason], 1);
	pthread_mutex_unlock(&listeners_lock);
}

void eventpool_trigger(int reason, void *(*free)(void *), void *data) {
	pthread_mutex_lock(&listeners_lock);
	struct eventpool_listener_t *listeners = eventpool_listeners;

	char name[255];
	snprintf(name, 255, "trigger %s", reasons[reason].reason);
	int nr = 0, nr1 = 0;
	sem_t *ref = NULL;

	if((nr1 = __sync_add_and_fetch(&nrlisteners[reason], 0)) == 0) {
		free((void *)data);
	} else {
		if(threads == EVENTPOOL_THREADED) {
			if((ref = MALLOC(sizeof(sem_t))) == NULL) {
				OUT_OF_MEMORY
			}
			sem_init(ref, 0, nr1-1);
		}

		while(listeners) {
			if(listeners->reason == reason) {
				nr++;
				if(threads == EVENTPOOL_NO_THREADS) {
					struct threadpool_tasks_t node;
					node.userdata = data;
					node.name = NULL;
					node.reason = reason;
					node.next = NULL;

					listeners->func(&node);

					if(nr == __sync_add_and_fetch(&nrlisteners[reason], 0)) {
						free((void *)data);
					}
				} else {
					if(threadpool_add_work(reason, ref, name, reasons[reason].priority, listeners->func, free, (void *)data) == -1) {
						free((void *)data);
						logprintf(LOG_ERR, "failed to add trigger %s to threadpool", name);
						break;
					}
				}
			}
			listeners = listeners->next;
		}
	}
	pthread_mutex_unlock(&listeners_lock);
}

int eventpool_gc(void) {
	pthread_mutex_lock(&listeners_lock);
	struct eventpool_listener_t *listeners = NULL;
	while(eventpool_listeners) {
		listeners = eventpool_listeners;
		eventpool_listeners = eventpool_listeners->next;
		FREE(listeners);
	}
	if(eventpool_listeners != NULL) {
		FREE(eventpool_listeners);
	}
	pthread_mutex_unlock(&listeners_lock);
	return 0;
}

static void eventpool_iobuf_init(struct eventpool_iobuf_t *iobuf, size_t initial_size) {
  iobuf->len = iobuf->size = 0;
  iobuf->buf = NULL;
	pthread_mutex_init(&iobuf->lock, NULL);
}

static void eventpool_iobuf_free(struct eventpool_iobuf_t *iobuf) {
  if(iobuf != NULL) {
		pthread_mutex_lock(&iobuf->lock);
    if(iobuf->buf != NULL) {
			FREE(iobuf->buf);
		}
		iobuf->len = iobuf->size = 0;
  }
	pthread_mutex_unlock(&iobuf->lock);
}

static void eventpool_iobuf_remove(struct eventpool_iobuf_t *io, size_t n) {
	pthread_mutex_lock(&io->lock);
  if(n > 0 && n <= io->len) {
    memmove(io->buf, io->buf + n, io->len - n);
    io->len -= n;
  }
	pthread_mutex_unlock(&io->lock);
}

static size_t eventpool_iobuf_append(struct eventpool_iobuf_t *io, const void *buf, size_t len) {
  char *p = NULL;

	pthread_mutex_lock(&io->lock);
  assert(io != NULL);
  assert(io->len <= io->size);

  if(len <= 0) {
  } else if(io->len + len <= io->size) {
    memcpy(io->buf + io->len, buf, len);
    io->len += len;
  } else if((p = REALLOC(io->buf, io->len + len)) != NULL) {
    io->buf = p;
    memcpy(io->buf + io->len, buf, len);
    io->len += len;
    io->size = io->len;
  } else {
    len = 0;
  }
	pthread_mutex_unlock(&io->lock);

  return len;
}

void eventpool_fd_enable_write(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->dowrite, 0);
	if(nr == 0) {
		__sync_add_and_fetch(&node->dowrite, 1);
	}
}

void eventpool_fd_enable_flush(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->doflush, 0);
	if(nr == 0) {
		__sync_add_and_fetch(&node->doflush, 1);
	}
}

void eventpool_fd_enable_read(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->doread, 0);
	if(nr == 0) {
		__sync_add_and_fetch(&node->doread, 1);
	}
}

void eventpool_fd_enable_highpri(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->dohighpri, 0);
	if(nr == 0) {
		__sync_add_and_fetch(&node->dohighpri, 1);
	}
}

void eventpool_fd_disable_flush(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->doflush, 0);
	if(nr == 1) {
		__sync_add_and_fetch(&node->doflush, -1);
	}
}

void eventpool_fd_disable_write(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->dowrite, 0);
	if(nr == 1) {
		__sync_add_and_fetch(&node->dowrite, -1);
	}
}

void eventpool_fd_disable_highpri(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->dohighpri, 0);
	if(nr == 1) {
		__sync_add_and_fetch(&node->dohighpri, -1);
	}
}

void eventpool_fd_disable_read(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	int nr = __sync_add_and_fetch(&node->doread, 0);
	if(nr == 1) {
		__sync_add_and_fetch(&node->doread, -1);
	}
}

int eventpool_fd_select(int fd, struct eventpool_fd_t **node) {
	if(running() == 0) {
		return -1;
	}

	int i = 0;
	struct eventpool_fd_t *nodes = NULL;
	for(i=0;i<nrfd;i++) {
		nodes = eventpool_fds[i];
		if(nodes->fd == fd && __sync_add_and_fetch(&nodes->remove, 0) == 0) {
			*node = nodes;
			return 0;
		}
	}
	return -1;
}

void eventpool_fd_remove(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	if(__sync_add_and_fetch(&node->remove, 0) == 0 &&
	   __sync_add_and_fetch(&node->active, 0) == 1) {
		__sync_add_and_fetch(&node->remove, 1);
	}
}

int eventpool_fd_write(int fd, char *data, unsigned long len) {
	if(running() == 0) {
		return -1;
	}

	struct eventpool_fd_t *node = NULL;
	if(eventpool_fd_select(fd, &node) == 0) {
		eventpool_iobuf_append(&node->send_iobuf, data, len);
		eventpool_fd_enable_flush(node);
		return 0;
	}
	return -1;
}

void eventpool_socket_reconnect(struct eventpool_fd_t *node) {
	if(node->fd > 0) {
		close(node->fd);
	}
	int x = __sync_add_and_fetch(&node->remove, 0);
	while(__sync_bool_compare_and_swap(&node->remove, x, 0) == 0) { };
	node->error = 0;
	node->stage = EVENTPOOL_STAGE_SOCKET;
}

struct eventpool_fd_t *eventpool_fd_add(char *name, int fd, int (*callback)(struct eventpool_fd_t *, int), int (*send)(struct eventpool_fd_t *), void *userdata) {
	if(running() == 0) {
		return NULL;
	}
	char buffer[INET_ADDRSTRLEN+1];
	memset(&buffer, '\0', INET_ADDRSTRLEN+1);

	int i = 0, freefd = -1;
	for(i=0;i<nrfd;i++) {
		if(__sync_add_and_fetch(&eventpool_fds[i]->active, 0) == 0) {
			freefd = i;
			break;
		}
	}
	if(freefd == -1) {
		if(pilight.debuglevel == 1) {
			logprintf(LOG_DEBUG, "increasing eventpool_fd_t struct size to %d", nrfd+16);
		}
		eventpool_fds = REALLOC(eventpool_fds, sizeof(struct eventpool_fd_t *)*(nrfd+16));
		for(i=nrfd;i<(nrfd+16);i++) {
			eventpool_fds[i] = MALLOC(sizeof(struct eventpool_fd_t));
			memset(eventpool_fds[i], 0, sizeof(struct eventpool_fd_t));
		}
		freefd = nrfd+1;
		nrfd += 16;
	}

	struct eventpool_fd_t *node = eventpool_fds[freefd];
	memset(node, 0, sizeof(struct eventpool_fd_t));

	memset(&node->data, '\0', sizeof(node->data));
	node->buffer = NULL;
	node->name = name;
	node->active = 0;
	node->len = 0;
	node->error = 0;
	node->steps = 0;
	node->fd = fd;
	node->remove = 0;
	node->doread = 0;
	node->dowrite = 0;
	node->doflush = 0;
	node->dohighpri = 0;
	node->type = EVENTPOOL_TYPE_IO;
	node->stage = EVENTPOOL_STAGE_CONNECTED;
	node->callback = callback;
	node->send = send;
	node->userdata = userdata;

	eventpool_iobuf_init(&node->send_iobuf, 0);
	eventpool_iobuf_init(&node->recv_iobuf, 0);

	if(node->callback(node, EV_CONNECT_SUCCESS) == -1) {
		node->error = -1;
		node->stage = EVENTPOOL_STAGE_DISCONNECTED;
		node->callback(node, EV_DISCONNECTED);
		if(node->fd > 0) {
			close(node->fd);
		}
	}

	__sync_add_and_fetch(&node->active, 1);

	return node;
}

struct eventpool_fd_t *eventpool_socket_add(char *name, char *server, unsigned int port, int domain, int socktype, int protocol, int fdtype, int (*callback)(struct eventpool_fd_t *, int), int (*send)(struct eventpool_fd_t *), void *userdata) {
	if(running() == 0) {
		return NULL;
	}

	int i = 0, freefd = -1;
	for(i=0;i<nrfd;i++) {
		if(__sync_add_and_fetch(&eventpool_fds[i]->active, 0) == 0) {
			freefd = i;
			break;
		}
	}
	if(freefd == -1) {
		if(pilight.debuglevel == 1) {
			logprintf(LOG_DEBUG, "increasing eventpool_fd_t struct size to %d", nrfd+16);
		}
		eventpool_fds = REALLOC(eventpool_fds, sizeof(struct eventpool_fd_t *)*(nrfd+16));
		for(i=nrfd;i<(nrfd+16);i++) {
			eventpool_fds[i] = MALLOC(sizeof(struct eventpool_fd_t));
			memset(eventpool_fds[i], 0, sizeof(struct eventpool_fd_t));
		}
		freefd = nrfd+1;
		nrfd += 16;
	}

	struct eventpool_fd_t *node = eventpool_fds[freefd];
	memset(node, 0, sizeof(struct eventpool_fd_t));
	if(server != NULL) {
		if((node->data.socket.server = MALLOC(strlen(server)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->data.socket.server, server);
	} else {
		node->data.socket.server = NULL;
	}
	node->buffer = NULL;
	node->active = 0;
	node->len = 0;
	node->name = name;
	node->data.socket.domain = domain;
	node->data.socket.port = port;
	node->data.socket.type = socktype;
	node->data.socket.protocol = protocol;
	node->error = 0;
	node->steps = 0;
	node->fd = -1;
	node->remove = 0;
	node->doread = 0;
	node->dowrite = 0;
	node->doflush = 0;
	node->dohighpri = 0;
	node->type = fdtype;
	node->stage = EVENTPOOL_STAGE_SOCKET;
	node->callback = callback;
	node->send = send;
	node->userdata = userdata;

	eventpool_iobuf_init(&node->send_iobuf, 0);
	eventpool_iobuf_init(&node->recv_iobuf, 0);

	__sync_add_and_fetch(&node->active, 1);

	return node;
}

static int eventpool_socket_create(struct eventpool_fd_t *node) {
	char *p = node->data.socket.ip;

	if(node->data.socket.server == NULL || host2ip(node->data.socket.server, p) > -1) {
		if((node->fd = socket(node->data.socket.domain, node->data.socket.type, node->data.socket.protocol)) == -1) {
			return -1;
		}
#ifdef _WIN32
		unsigned long on = 1;
		ioctlsocket(sockfd, FIONBIO, &on);
#else
		long arg = fcntl(node->fd, F_GETFL, NULL);
		fcntl(node->fd, F_SETFL, arg | O_NONBLOCK);
#endif

		int opt = 1;
		setsockopt(node->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));

		if(node->data.socket.protocol == IPPROTO_ICMP) {
			node->stage = EVENTPOOL_STAGE_CONNECTED;
		} else if(node->type == EVENTPOOL_TYPE_SOCKET_CLIENT) {
			node->stage = EVENTPOOL_STAGE_CONNECT;
		} else if(node->type == EVENTPOOL_TYPE_SOCKET_SERVER) {
			node->stage = EVENTPOOL_STAGE_BIND;
		}
		return 0;
	} else {
		return -1;
	}
	return -1;
}

static int eventpool_socket_bind(struct eventpool_fd_t *node) {
	struct sockaddr_in servaddr;

	memset(&servaddr, '\0', sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if(node->data.socket.port == -1) {
		servaddr.sin_port = 0;
	} else {
		servaddr.sin_port = htons(node->data.socket.port);
	}

	if(node->data.socket.server == NULL) {
		servaddr.sin_addr.s_addr = INADDR_ANY;
	} else {
		inet_pton(AF_INET, node->data.socket.ip, &servaddr.sin_addr);
	}

	if(bind(node->fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		return -1;
	}	else {
		node->stage = EVENTPOOL_STAGE_LISTEN;
		return 0;
	}
}

static int eventpool_socket_listen(struct eventpool_fd_t *node) {
	if(listen(node->fd, 3) < 0) {
		return -1;
	}	else {
		node->stage = EVENTPOOL_STAGE_LISTENING;
		return 0;
	}
}

static int eventpool_socket_connect(struct eventpool_fd_t *node) {
	int ret = 0;
	struct sockaddr_in servaddr;

	memset(&servaddr, '\0', sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if(node->data.socket.port == -1) {
		servaddr.sin_port = 0;
	} else {
		servaddr.sin_port = htons(node->data.socket.port);
	}

	if(node->data.socket.server == NULL) {
		servaddr.sin_addr.s_addr = INADDR_ANY;
	} else {
		inet_pton(AF_INET, node->data.socket.ip, &servaddr.sin_addr);
	}

	if((ret = connect(node->fd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr))) < 0) {
		if(errno == EINPROGRESS || errno == EISCONN) {
			node->stage = EVENTPOOL_STAGE_CONNECTING;
			eventpool_fd_enable_write(node);
			return 0;
		} else {
			return -1;
		}
	} else if(ret == 0) {
		node->stage = EVENTPOOL_STAGE_CONNECTING;
		eventpool_fd_enable_write(node);
		return 0;
	} else {
		return -1;
	}

	return -1;
}

static int eventpool_socket_connecting(struct eventpool_fd_t *node) {
	socklen_t lon = 0;
	int valopt = 0;
	lon = sizeof(int);
	getsockopt(node->fd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
	if(valopt <= 0) {
		node->stage = EVENTPOOL_STAGE_CONNECTED;
		return 0;
	}
	return -1;
}

static void eventpool_clear_nodes(void) {
	int i = 0, x = 0;
	for(i=0;i<nrfd;i++) {
		struct eventpool_fd_t *tmp = eventpool_fds[i];
		if((x = __sync_add_and_fetch(&tmp->remove, 0)) == 1) {
			while(__sync_bool_compare_and_swap(&tmp->remove, x, 0) == 0);

			x = __sync_add_and_fetch(&tmp->active, 0);
			while(__sync_bool_compare_and_swap(&tmp->active, x, 0) == 0);

			if(tmp->type == EVENTPOOL_TYPE_SOCKET_SERVER ||
				tmp->type == EVENTPOOL_TYPE_SOCKET_CLIENT) {
				if(tmp->data.socket.server != NULL) {
					FREE(tmp->data.socket.server);
				}
			}
			eventpool_iobuf_free(&tmp->send_iobuf);
			eventpool_iobuf_free(&tmp->recv_iobuf);
			if(tmp->fd > 0) {
				shutdown(tmp->fd, SHUT_RDWR);
				close(tmp->fd);
			}
		}
	}
}

void *eventpool_process(void *param) {
	struct eventpool_fd_t *tmp = NULL;
	int ret = 0, i = 0, timeout = 0, nrpoll = 0;
	struct pollfd *pollfds = NULL;
	struct timeval tv;
	int interval = 0;
	unsigned long now = 0, then = 0;

	while(running() == 1) {
		eventpool_clear_nodes();

		if(nrpollfds <= nrfd) {
			if((pollfds = REALLOC(pollfds, (nrpollfds + 16)*sizeof(struct pollfd *))) == NULL) {
				OUT_OF_MEMORY
			}
			nrpollfds += 16;
		}

		nrpoll = 0;
		for(i=0;i<nrfd;i++) {
			if(__sync_add_and_fetch(&eventpool_fds[i]->active, 0) == 1) {
				// __sync_add_and_fetch(&eventpool_fds[i]->active, -1);
				tmp = eventpool_fds[i];
				if(tmp->fd >= 0) {
					tmp->idx = nrpoll;
					pollfds[tmp->idx].fd = tmp->fd;
					pollfds[tmp->idx].events = 0;
					pollfds[tmp->idx].revents = 0;
					if(__sync_add_and_fetch(&tmp->dowrite, 0) == 1 ||
						__sync_add_and_fetch(&tmp->doflush, 0) == 1) {
						pollfds[tmp->idx].events |= POLLOUT;
					}
					if(__sync_add_and_fetch(&tmp->doread, 0) == 1) {
						pollfds[tmp->idx].events |= POLLIN;
					}
					if(__sync_add_and_fetch(&tmp->dohighpri, 0) == 1) {
						pollfds[tmp->idx].events |= POLLPRI;
					}
					nrpoll++;
				} else {
					tmp->idx = -1;
				}
				// __sync_add_and_fetch(&eventpool_fds[i]->active, 1);
			}
		}
		ret = poll(pollfds, nrpoll, 100);

		interval = 0;
		gettimeofday(&tv, NULL);
		now = (1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec);
		if(then == 0) {
			then = now;
		}

		if((now - then) > 100000) {
			interval = 1;
			timeout++;
			if(timeout >= 10) {
				timeout = 0;
			}
			then = now;
		}

		for(i=0;i<nrfd;i++) {
			tmp = eventpool_fds[i];
			if(__sync_add_and_fetch(&tmp->active, 0) == 1) {
				// __sync_add_and_fetch(&tmp->active, -1);
				if(tmp->error == 0) {
					if(interval == 1) {
						if(tmp->stage != EVENTPOOL_STAGE_DISCONNECT && tmp->callback(tmp, EV_POLL) == -1) {
							tmp->stage = EVENTPOOL_STAGE_DISCONNECT;
							continue;
						}
					}
					if(tmp->type == EVENTPOOL_TYPE_SOCKET_SERVER ||
						 tmp->type == EVENTPOOL_TYPE_SOCKET_CLIENT ||
						 tmp->type == EVENTPOOL_TYPE_IO) {
						switch(tmp->stage) {
							case EVENTPOOL_STAGE_SOCKET:
								if((tmp->error = eventpool_socket_create(tmp)) == -1) {
									tmp->callback(tmp, EV_SOCKET_FAILED);
								} else {
									tmp->callback(tmp, EV_SOCKET_SUCCESS);
								}
							break;
							case EVENTPOOL_STAGE_BIND: {
								if((tmp->error = eventpool_socket_bind(tmp)) == -1) {
									if(tmp->callback(tmp, EV_BIND_FAILED) == -1) {
										tmp->error = -1;
										tmp->stage = EVENTPOOL_STAGE_DISCONNECTED;
										tmp->callback(tmp, EV_DISCONNECTED);
										if(tmp->fd > 0) {
											close(tmp->fd);
										}
									}
								} else {
									if(tmp->callback(tmp, EV_BIND_SUCCESS) == -1) {
										tmp->error = -1;
										tmp->stage = EVENTPOOL_STAGE_DISCONNECTED;
										tmp->callback(tmp, EV_DISCONNECTED);
										if(tmp->fd > 0) {
											close(tmp->fd);
										}
									}
								}
							} break;
							case EVENTPOOL_STAGE_LISTEN: {
								if((tmp->error = eventpool_socket_listen(tmp)) == -1) {
									tmp->callback(tmp, EV_LISTEN_FAILED);
								} else {
									tmp->callback(tmp, EV_LISTEN_SUCCESS);
									eventpool_fd_enable_read(tmp);
								}
							} break;
							case EVENTPOOL_STAGE_CONNECT:
								if((tmp->error = eventpool_socket_connect(tmp)) == -1) {
									tmp->callback(tmp, EV_CONNECT_FAILED);
								}
							break;
							case EVENTPOOL_STAGE_CONNECTING:
								if(tmp->fd >= 0 && ret > 0 && pollfds[tmp->idx].revents & POLLOUT) {
									eventpool_fd_disable_write(tmp);
									if((tmp->error = eventpool_socket_connecting(tmp)) == 0) {
										if(tmp->callback(tmp, EV_CONNECT_SUCCESS) == -1) {
											tmp->error = -1;
											tmp->stage = EVENTPOOL_STAGE_DISCONNECTED;
											tmp->callback(tmp, EV_DISCONNECTED);
											if(tmp->fd > 0) {
												close(tmp->fd);
											}
										}
									} else {
										tmp->callback(tmp, EV_CONNECT_FAILED);
									}
								}
							break;
							case EVENTPOOL_STAGE_LISTENING:
								if(ret > 0 && tmp->fd >= 0) {
									if(pollfds[tmp->idx].revents & POLLIN) {
										if(tmp->callback(tmp, EV_READ) == -1) {
											tmp->error = -1;
											tmp->stage = EVENTPOOL_STAGE_DISCONNECTED;
											tmp->callback(tmp, EV_DISCONNECTED);
											if(tmp->fd > 0) {
												close(tmp->fd);
											}
										}
									}
								}
							break;
							case EVENTPOOL_STAGE_DISCONNECT:
							case EVENTPOOL_STAGE_CONNECTED: {
								if(ret > 0 && tmp->fd >= 0) {
									if(pollfds[tmp->idx].revents & POLLIN && tmp->stage == EVENTPOOL_STAGE_CONNECTED) {
										eventpool_fd_disable_read(tmp);
										if(tmp->stage == EVENTPOOL_STAGE_CONNECTED &&
											 tmp->callback(tmp, EV_READ) == -1) {
											tmp->stage = EVENTPOOL_STAGE_DISCONNECT;
											if(__sync_add_and_fetch(&tmp->doflush, 0) == 0) {
												tmp->error = -1;
												tmp->callback(tmp, EV_DISCONNECTED);
												if(tmp->fd > 0) {
													close(tmp->fd);
												}
												continue;
											}
										}
									}
									if(pollfds[tmp->idx].revents & POLLPRI && tmp->stage == EVENTPOOL_STAGE_CONNECTED) {
										eventpool_fd_disable_highpri(tmp);
										if(tmp->stage == EVENTPOOL_STAGE_CONNECTED &&
											 tmp->callback(tmp, EV_HIGHPRI) == -1) {
											tmp->stage = EVENTPOOL_STAGE_DISCONNECT;
											if(__sync_add_and_fetch(&tmp->dohighpri, 0) == 0) {
												tmp->error = -1;
												tmp->callback(tmp, EV_DISCONNECTED);
												if(tmp->fd > 0) {
													close(tmp->fd);
												}
												continue;
											}
										}
									}
									if(pollfds[tmp->idx].revents & POLLOUT) {
										eventpool_fd_disable_write(tmp);
										eventpool_fd_disable_flush(tmp);
										if(tmp->send_iobuf.len > 0) {
											int n = 0;
											if(tmp->send != NULL) {
												n = tmp->send(tmp);
											} else {
												if(tmp->data.socket.port > 0 && strlen(tmp->data.socket.ip) > 0) {
													n = (int)sendto(tmp->fd, tmp->send_iobuf.buf, tmp->send_iobuf.len, 0,
															(struct sockaddr *)&tmp->data.socket.addr, sizeof(tmp->data.socket.addr));
												} else {
													n = (int)send(tmp->fd, tmp->send_iobuf.buf, tmp->send_iobuf.len, 0);
												}
											}
											if(n > 0) {
												eventpool_iobuf_remove(&tmp->send_iobuf, n);
												// if(tmp->stage == EVENTPOOL_STAGE_DISCONNECT) {
													// tmp->error = -1;
													// tmp->callback(tmp, EV_DISCONNECTED);
													// if(tmp->fd > 0) {
														// close(tmp->fd);
													// }
													// continue;
												// }
												eventpool_fd_enable_flush(tmp);
												continue;
											} else if(n == -1 && errno != EAGAIN) {
												tmp->error = -1;
												tmp->callback(tmp, EV_DISCONNECTED);
												if(tmp->fd > 0) {
													close(tmp->fd);
												}
												continue;
											}
											if(tmp->send_iobuf.len > 0) {
												eventpool_fd_enable_flush(tmp);
											} else {
												if(tmp->stage == EVENTPOOL_STAGE_DISCONNECT) {
													tmp->error = -1;
													tmp->callback(tmp, EV_DISCONNECTED);
													if(tmp->fd > 0) {
														close(tmp->fd);
													}
													continue;
												}
											}
										} else {
											if(tmp->stage == EVENTPOOL_STAGE_DISCONNECT) {
												tmp->error = -1;
												tmp->callback(tmp, EV_DISCONNECTED);
												if(tmp->fd > 0) {
													close(tmp->fd);
												}
												continue;
											}
										}
										if(tmp->stage == EVENTPOOL_STAGE_CONNECTED &&
											 tmp->callback(tmp, EV_WRITE) == -1) {
											tmp->stage = EVENTPOOL_STAGE_DISCONNECT;
										}
									}
								}
								if(tmp->stage == EVENTPOOL_STAGE_DISCONNECT &&
									__sync_add_and_fetch(&tmp->dowrite, 0) == 0 &&
									__sync_add_and_fetch(&tmp->doflush, 0) == 0) {
									tmp->error = -1;
									tmp->callback(tmp, EV_DISCONNECTED);
									if(tmp->fd > 0) {
										close(tmp->fd);
									}
									continue;
								}
							} break;
							default:
							break;
						}
					}
				}
				// __sync_add_and_fetch(&tmp->active, 1);
			}
		}
	}

	for(i=0;i<nrfd;i++) {
		tmp = eventpool_fds[i];
		if(__sync_add_and_fetch(&tmp->active, 0) == 1) {
			tmp->callback(tmp, EV_DISCONNECTED);
			if(tmp->fd > 0) {
				shutdown(tmp->fd, SHUT_RDWR);
				close(tmp->fd);
			}
			if(tmp->type == EVENTPOOL_TYPE_SOCKET_SERVER ||
				 tmp->type == EVENTPOOL_TYPE_SOCKET_CLIENT) {
				if(tmp->data.socket.server != NULL) {
					FREE(tmp->data.socket.server);
				}
			}
			eventpool_iobuf_free(&tmp->send_iobuf);
			eventpool_iobuf_free(&tmp->recv_iobuf);
		}
		FREE(tmp);
	}
	if(pollfds != NULL) {
		FREE(pollfds);
	}
	if(eventpool_fds != NULL) {
		FREE(eventpool_fds);
	}

	return NULL;
}

void eventpool_init(enum eventpool_threads_t t) {
	if(running() == 0) {
		return;
	}

	threads = t;

	pthread_mutex_init(&listeners_lock, NULL);
}

enum eventpool_threads_t eventpool_threaded(void) {
	return threads;
}
