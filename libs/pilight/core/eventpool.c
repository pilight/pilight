/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
	{	REASON_LOG,										"REASON_LOG",										0 },
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

		if(listeners == NULL) {
			free((void *)data);
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
	threads = EVENTPOOL_NO_THREADS;
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

	node->dowrite = 1;
}

void eventpool_fd_enable_flush(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	node->doflush = 1;
}

void eventpool_fd_enable_read(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	node->doread = 1;
}

void eventpool_fd_enable_highpri(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	node->dohighpri = 1;
}

void eventpool_fd_disable_flush(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	node->doflush = 0;
}

void eventpool_fd_disable_write(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	node->dowrite = 0;
}

void eventpool_fd_disable_highpri(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	node->dohighpri = 0;
}

void eventpool_fd_disable_read(struct eventpool_fd_t *node) {
	if(running() == 0) {
		return;
	}

	node->doread = 0;
}

int eventpool_fd_select(int fd, struct eventpool_fd_t **node) {
	if(running() == 0) {
		return -1;
	}

	int i = 0;
	struct eventpool_fd_t *nodes = NULL;
	for(i=0;i<nrfd;i++) {
		nodes = eventpool_fds[i];
		if(nodes->fd == fd && nodes->remove == 0) {
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

	if(node->remove == 0 && node->active == 1) {
		node->remove = 1;
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

struct eventpool_fd_t *eventpool_fd_add(char *name, int fd, int (*callback)(struct eventpool_fd_t *, int), int (*send)(struct eventpool_fd_t *), void *userdata) {
	if(running() == 0) {
		return NULL;
	}
	char buffer[INET_ADDRSTRLEN+1];
	memset(&buffer, '\0', INET_ADDRSTRLEN+1);

	int i = 0, freefd = -1;
	for(i=0;i<nrfd;i++) {
		if(eventpool_fds[i]->active == 0) {
			freefd = i;
			break;
		}
	}
	if(freefd == -1) {
		if(pilight.debuglevel == 1) {
			logprintf(LOG_DEBUG, "increasing eventpool_fd_t struct size to %d", nrfd+16);
		}
		if((eventpool_fds = REALLOC(eventpool_fds, sizeof(struct eventpool_fd_t *)*(nrfd+16))) == NULL) {
			OUT_OF_MEMORY
		}
		for(i=nrfd;i<(nrfd+16);i++) {
			if((eventpool_fds[i] = MALLOC(sizeof(struct eventpool_fd_t))) == NULL) {
				OUT_OF_MEMORY
			}
			memset(eventpool_fds[i], 0, sizeof(struct eventpool_fd_t));
		}
		freefd = nrfd;
		__sync_add_and_fetch(&nrfd, 16);
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
			node->fd = 0;
		}
	}

	node->active = 1;

	return node;
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

static int eventpool_socket_listen(struct eventpool_fd_t *node) {
	if(listen(node->fd, 3) < 0) {
		return -1;
	}	else {
		node->stage = EVENTPOOL_STAGE_LISTENING;
		return 0;
	}
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
		if((node->error = eventpool_socket_listen(node)) == -1) {
			node->callback(node, EV_LISTEN_FAILED);
		} else {
			node->callback(node, EV_LISTEN_SUCCESS);
			eventpool_fd_enable_read(node);
		}		
		return 0;
	}
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

void eventpool_socket_reconnect(struct eventpool_fd_t *node) {
	if(node->fd > 0) {
		close(node->fd);
		node->fd = 0;
	}
	node->remove = 0;
	node->error = 0;
	node->stage = EVENTPOOL_STAGE_SOCKET;
	if((node->error = eventpool_socket_create(node)) == -1) {
		node->callback(node, EV_SOCKET_FAILED);
	} else {
		node->callback(node, EV_SOCKET_SUCCESS);
	}
}

struct eventpool_fd_t *eventpool_socket_add(char *name, char *server, unsigned int port, int domain, int socktype, int protocol, int fdtype, int (*callback)(struct eventpool_fd_t *, int), int (*send)(struct eventpool_fd_t *), void *userdata) {
	if(running() == 0) {
		return NULL;
	}

	int i = 0, freefd = -1;
	for(i=0;i<nrfd;i++) {
		if(eventpool_fds[i]->active == 0) {
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
		freefd = nrfd;
		__sync_add_and_fetch(&nrfd, 16);
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

	node->active = 1;

	if((node->error = eventpool_socket_create(node)) == -1) {
		node->callback(node, EV_SOCKET_FAILED);
	} else {
		node->callback(node, EV_SOCKET_SUCCESS);
		switch(node->stage) {
			case EVENTPOOL_STAGE_CONNECT: {
				if((node->error = eventpool_socket_connect(node)) == -1) {
					node->callback(node, EV_CONNECT_FAILED);
				}
			} break;
			case EVENTPOOL_STAGE_BIND: {
				if((node->error = eventpool_socket_bind(node)) == -1) {
					if(node->callback(node, EV_BIND_FAILED) == -1) {
						node->error = -1;
						node->stage = EVENTPOOL_STAGE_DISCONNECTED;
						node->callback(node, EV_DISCONNECTED);
						if(node->fd > 0) {
							close(node->fd);
							node->fd = 0;
						}
					}
				} else {
					if(node->callback(node, EV_BIND_SUCCESS) == -1) {
						node->error = -1;
						node->stage = EVENTPOOL_STAGE_DISCONNECTED;
						node->callback(node, EV_DISCONNECTED);
						if(node->fd > 0) {
							close(node->fd);
							node->fd = 0;
						}
					}
				}
			} break;
			default:
			break;
		}
	}	
	
	return node;
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
	int i = 0;
	for(i=0;i<nrfd;i++) {
		struct eventpool_fd_t *tmp = eventpool_fds[i];
		if(tmp->remove == 1) {
			tmp->remove = 0;
			tmp->active = 0;

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
				tmp->fd = 0;
			}
		}
	}
}

void *eventpool_process(void *param) {
	struct eventpool_fd_t *tmp = NULL;
	int ret = 0, i = 0, nrpoll = 0;
	struct pollfd *pollfds = NULL;
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
			if(eventpool_fds[i]->active == 1 && eventpool_fds[i]->stage != EVENTPOOL_STAGE_DISCONNECTED) {
				// eventpool_fds[i]->active = 0;
				tmp = eventpool_fds[i];
				if(tmp->fd >= 0) {
					tmp->idx = nrpoll;
					pollfds[tmp->idx].fd = tmp->fd;
					pollfds[tmp->idx].events = 0;
					pollfds[tmp->idx].revents = 0;
					if(tmp->dowrite == 1 || tmp->doflush == 1) {
						pollfds[tmp->idx].events |= POLLOUT;
					}
					if(tmp->doread == 1) {
						pollfds[tmp->idx].events |= POLLIN;
					}
					if(tmp->dohighpri == 1) {
						pollfds[tmp->idx].events |= POLLPRI;
					}
					nrpoll++;
				} else {
					tmp->idx = -1;
				}
				// eventpool_fds[i]->active = 1;
			}
		}
		ret = poll(pollfds, nrpoll, 1000);
		if((ret == -1 && errno == EINTR) || ret == 0) {
			continue;
		}

		for(i=0;i<nrfd;i++) {
			tmp = eventpool_fds[i];
			if(tmp->active == 1 && tmp->stage != EVENTPOOL_STAGE_DISCONNECTED) {
				// tmp->active = 0;
				if(tmp->error == 0) {
					if(tmp->type == EVENTPOOL_TYPE_SOCKET_SERVER ||
						 tmp->type == EVENTPOOL_TYPE_SOCKET_CLIENT ||
						 tmp->type == EVENTPOOL_TYPE_IO) {
						switch(tmp->stage) {
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
												tmp->fd = 0;
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
												tmp->fd = 0;
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
											if(tmp->doflush == 0) {
												tmp->error = -1;
												tmp->callback(tmp, EV_DISCONNECTED);
												if(tmp->fd > 0) {
													close(tmp->fd);
													tmp->fd = 0;
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
											if(tmp->dohighpri == 0) {
												tmp->error = -1;
												tmp->callback(tmp, EV_DISCONNECTED);
												if(tmp->fd > 0) {
													close(tmp->fd);
													tmp->fd = 0;
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
														//close(tmp->fd);
														// tmp->fd = 0;
													// }
													// continue;
												// }
											} else if(n == -1 && errno != EAGAIN) {
												tmp->error = -1;
												tmp->callback(tmp, EV_DISCONNECTED);
												if(tmp->fd > 0) {
													close(tmp->fd);
													tmp->fd = 0;
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
														tmp->fd = 0;
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
													tmp->fd = 0;
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
									tmp->dowrite == 0 && tmp->doflush == 0) {
									tmp->error = -1;
									tmp->callback(tmp, EV_DISCONNECTED);
									if(tmp->fd > 0) {
										close(tmp->fd);
										tmp->fd = 0;
									}
									continue;
								}
							} break;
							default:
							break;
						}
					}
				}
				// tmp->active = 1;
			}
		}
	}

	for(i=0;i<nrfd;i++) {
		tmp = eventpool_fds[i];
		if(tmp->active == 1) {
			tmp->callback(tmp, EV_DISCONNECTED);
			if(tmp->fd > 0) {
				shutdown(tmp->fd, SHUT_RDWR);
				close(tmp->fd);
				tmp->fd = 0;
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
