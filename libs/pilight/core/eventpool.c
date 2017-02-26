/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "eventpool.h"

#include <sys/types.h>
#ifdef _WIN32
	#if _WIN32_WINNT < 0x0501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <mstcpip.h>
	#include <windows.h>
	struct pollfd {
		int fd;
		short events;
		short revents;
	};
	#define POLLIN	0x0001
	#define POLLPRI	0x0002
	#define POLLOUT	0x0004
#else
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <poll.h>
	#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

#include "log.h"
#include "gc.h"
#include "threadpool.h"
#include "../../libuv/uv.h"
#include "mem.h"
#include "network.h"

static int nrfd = 0;
static int nrpollfds = 0;
static struct eventpool_fd_t **eventpool_fds = NULL;
static uv_async_t *async_req = NULL;

static void eventpool_iobuf_init(struct eventpool_iobuf_t *iobuf, size_t initial_size) {
  iobuf->len = iobuf->size = 0;
  iobuf->buf = NULL;
	uv_mutex_init(&iobuf->lock);
}

static void eventpool_iobuf_free(struct eventpool_iobuf_t *iobuf) {
  if(iobuf != NULL) {
		uv_mutex_lock(&iobuf->lock);
    if(iobuf->buf != NULL) {
			FREE(iobuf->buf);
		}
		iobuf->len = iobuf->size = 0;
  }
	uv_mutex_unlock(&iobuf->lock);
}

static void eventpool_iobuf_remove(struct eventpool_iobuf_t *io, size_t n) {
	uv_mutex_lock(&io->lock);
  if(n > 0 && n <= io->len) {
    memmove(io->buf, io->buf + n, io->len - n);
    io->len -= n;
  }
	uv_mutex_unlock(&io->lock);
}

static size_t eventpool_iobuf_append(struct eventpool_iobuf_t *io, const void *buf, size_t len) {
  char *p = NULL;

	uv_mutex_lock(&io->lock);
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
	uv_mutex_unlock(&io->lock);

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
		// if(pilight.debuglevel == 1) {
			// logprintf(LOG_DEBUG, "increasing eventpool_fd_t struct size to %d", nrfd+16);
		// }
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
#ifdef _WIN32
	InterlockedExchangeAdd(&nrfd, 16);
#else
	__sync_add_and_fetch(&nrfd, 16);
#endif
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
#ifdef _WIN32
		if(WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEISCONN) {
#else
		if(errno == EINPROGRESS || errno == EISCONN) {
#endif
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

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
#endif
	
	if(node->data.socket.server == NULL || host2ip(node->data.socket.server, p) > -1) {
		if((node->fd = socket(node->data.socket.domain, node->data.socket.type, node->data.socket.protocol)) == -1) {
			return -1;
		}
#ifdef _WIN32
		unsigned long on = 1;
		ioctlsocket(node->fd, FIONBIO, &on);
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
		// if(pilight.debuglevel == 1) {
			// logprintf(LOG_DEBUG, "increasing eventpool_fd_t struct size to %d", nrfd+16);
		// }
		eventpool_fds = REALLOC(eventpool_fds, sizeof(struct eventpool_fd_t *)*(nrfd+16));
		for(i=nrfd;i<(nrfd+16);i++) {
			eventpool_fds[i] = MALLOC(sizeof(struct eventpool_fd_t));
			memset(eventpool_fds[i], 0, sizeof(struct eventpool_fd_t));
		}
		freefd = nrfd;
#ifdef _WIN32
		InterlockedExchangeAdd(&nrfd, 16);
#else
		__sync_add_and_fetch(&nrfd, 16);
#endif
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
	if(getsockopt(node->fd, SOL_SOCKET, SO_ERROR, (void *)(&valopt), &lon) == 0) {
		if(valopt == 0) {
			node->stage = EVENTPOOL_STAGE_CONNECTED;
			return 0;
		}
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
#ifdef _WIN32
				shutdown(tmp->fd, SD_BOTH);
#else
				shutdown(tmp->fd, SHUT_RDWR);
#endif
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
#ifdef _WIN32
	struct timeval timeout;
	fd_set readset;
	fd_set writeset;
	int maxfd = 0;

	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
	
	int dummy = 0;
	if((dummy = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		logprintf(LOG_ERR, "Could not create dummy socket: %d", WSAGetLastError());
		exit(EXIT_FAILURE);
   }
#endif

	while(running() == 1) {
		eventpool_clear_nodes();

#ifdef _WIN32
		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		FD_SET(dummy, &readset);
		timeout.tv_sec = 1;
		timeout.tv_usec = 1000;
		maxfd = 0;
#endif
		
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
#ifdef _WIN32
					if(tmp->doread == 1 || tmp->dohighpri == 1) {
						FD_SET(tmp->fd, &readset);
					}
					if(tmp->dowrite == 1 || tmp->doflush == 1) {
						FD_SET(tmp->fd, &writeset);
					}
					if(tmp->dowrite == 1 || tmp->doflush == 1 ||
						 tmp->doread == 1 || tmp->dohighpri == 1 ||
						 maxfd < tmp->fd) {
						maxfd = tmp->fd;
					}
#else
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
#endif
					nrpoll++;
				} else {
					tmp->idx = -1;
				}
				// eventpool_fds[i]->active = 1;
			}
		}

#ifdef _WIN32
		ret = select(maxfd+1, &readset, &writeset, NULL, &timeout);
		if((ret == -1 && errno == EINTR) || ret == 0) {
			continue;
		}
#else
		ret = poll(pollfds, nrpoll, 1000);
		if((ret == -1 && errno == EINTR) || ret == 0) {
			continue;
		}
#endif
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
#ifdef _WIN32
								if(tmp->fd >= 0 && ret > 0 && FD_ISSET(tmp->fd, &writeset)) {
#else
								if(tmp->fd >= 0 && ret > 0 && pollfds[tmp->idx].revents & POLLOUT) {
#endif
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
#ifdef _WIN32
									if(FD_ISSET(tmp->fd, &readset)) {
#else
									if(pollfds[tmp->idx].revents & POLLIN) {
#endif
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
							case EVENTPOOL_STAGE_DISCONNECT: {
								char c = 0;
#ifdef _WIN32
								if(recv(tmp->fd, &c, 1, MSG_PEEK) <= 0) {
#else
								if(recv(tmp->fd, &c, 1, MSG_PEEK | MSG_DONTWAIT) <= 0) {
#endif									
									if(errno == ECONNRESET) {
										if(tmp->fd > 0) {
											logprintf(LOG_DEBUG, "client disconnected, fd %d", tmp->fd);
										}
										eventpool_fd_remove(tmp);
										continue;
									}
								}
							}
							case EVENTPOOL_STAGE_CONNECTED: {
								if(ret > 0 && tmp->fd >= 0) {
#ifdef _WIN32
									if(FD_ISSET(tmp->fd, &readset) && tmp->stage == EVENTPOOL_STAGE_CONNECTED) {
#else
									if(pollfds[tmp->idx].revents & POLLIN && tmp->stage == EVENTPOOL_STAGE_CONNECTED) {
#endif
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
#ifdef _WIN32
									if(FD_ISSET(tmp->fd, &readset) && tmp->stage == EVENTPOOL_STAGE_CONNECTED) {
#else
									if(pollfds[tmp->idx].revents & POLLPRI && tmp->stage == EVENTPOOL_STAGE_CONNECTED) {
#endif
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
#ifdef _WIN32
									if(FD_ISSET(tmp->fd, &writeset)) {
#else
									if(pollfds[tmp->idx].revents & POLLOUT) {
#endif
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
											} else if(n == 0) {
												eventpool_fd_remove(tmp);
												continue;
											}
											if(n < 0 && errno != EAGAIN && errno != EINTR) {
												if(errno == ECONNRESET) {
													eventpool_fd_remove(tmp);
													continue;
												} else {
													tmp->error = -1;
													tmp->callback(tmp, EV_DISCONNECTED);
													if(tmp->fd > 0) {
														close(tmp->fd);
														tmp->fd = 0;
													}
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
#ifdef _WIN32
				shutdown(tmp->fd, SD_BOTH);
#else
				shutdown(tmp->fd, SHUT_RDWR);
#endif
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

/*
 * NEW
 */
struct eventqueue_t {
	int reason;
	void *(*done)(void *);
	void *data;
	struct eventqueue_t *next;
} eventqueue_t;

static int nrlisteners[REASON_END] = {0};
static int nrlisteners1[REASON_END] = {0};
static struct eventqueue_t *eventqueue = NULL;

static int threads = EVENTPOOL_NO_THREADS;
static uv_mutex_t listeners_lock;
// static pthread_mutexattr_t listeners_attr;
static int lockinit = 0;
static int eventpoolinit = 0;
static ssize_t minus_one = -1;
static ssize_t one = 1;

static struct eventpool_listener_t *eventpool_listeners = NULL;

static struct reasons_t {
	int number;
	const char *reason;
	int priority;
} reasons[REASON_END+1] = {
	{	REASON_SEND_CODE, 						"REASON_SEND_CODE",							0 },
	{	REASON_CONTROL_DEVICE, 				"REASON_CONTROL_DEVICE",				0 },
	{	REASON_CODE_SENT, 						"REASON_CODE_SENT",							0 },
	{	REASON_SOCKET_SEND,						"REASON_CODE_SEND_FAIL",				0 },
	{	REASON_SOCKET_SEND,						"REASON_CODE_SEND_SUCCESS",			0 },
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
	{	REASON_DEVICE_ADAPT,					"REASON_DEVICE_ADAPT",					0 },
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

static void fib_free(uv_work_t *req, int status) {
	FREE(req);
}

static void fib(uv_work_t *req) {
	struct {
		struct timespec first;
		struct timespec second;
	}	timestamp;
	
	struct cpu_usage_t cpu_usage;
	struct threadpool_data_t *data = req->data;

	data->func(data->reason, data->userdata);

	int x = 0;
	if(data->ref != NULL) {
		x = uv_sem_trywait(data->ref);
	}
	if((data->ref == NULL) || (x == UV__EAGAIN)) {
		if(data->done != NULL && data->reason != REASON_END) {
			data->done(data->userdata);
		}
		if(data->ref != NULL) {
			FREE(data->ref);
		}
	}
	FREE(req->data);
}

void eventpool_callback(int reason, void *(*func)(int, void *)) {
	if(lockinit == 1) {
		uv_mutex_lock(&listeners_lock);
	}
	struct eventpool_listener_t *node = MALLOC(sizeof(struct eventpool_listener_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	node->func = func;
	node->reason = reason;
	node->next = NULL;

	node->next = eventpool_listeners;
	eventpool_listeners = node;

#ifdef _WIN32
	InterlockedIncrement(&nrlisteners[reason]);
#else
	__sync_add_and_fetch(&nrlisteners[reason], 1);
#endif
	if(lockinit == 1) {
		uv_mutex_unlock(&listeners_lock);
	}
}

void eventpool_trigger(int reason, void *(*done)(void *), void *data) {
#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = 80;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched);
#endif

	struct eventqueue_t *node = MALLOC(sizeof(struct eventqueue_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	memset(node, 0, sizeof(struct eventqueue_t));
	node->reason = reason;
	node->done = done;
	node->data = data;

	uv_mutex_lock(&listeners_lock);
	struct eventqueue_t *tmp = eventqueue;
	if(tmp != NULL) {
		while(tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp->next = node;
		node = tmp;
	} else {
		node->next = eventqueue;
		eventqueue = node;
	}
	uv_mutex_unlock(&listeners_lock);
	uv_async_send(async_req);
}

static void eventpool_execute(uv_async_t *handle) {
	/*
	 * Make sure we execute in the main thread
	 */
	assert(pthread_equal(pth_main_id, pthread_self()));

	struct threadpool_tasks_t **node = NULL;
	struct threadpool_data_t *tpdata = NULL;
	int x = 0, nr1 = 0, nrnodes = 16, nrnodes1 = 0, i = 0;

	if((node = MALLOC(sizeof(struct threadpool_tasks_t *)*nrnodes)) == NULL) {
		OUT_OF_MEMORY
	}

	uv_mutex_lock(&listeners_lock);
	struct eventqueue_t *queue = NULL;
	while(eventqueue) {
		queue = eventqueue;
		// char name[255];
		// snprintf(name, 255, "trigger %s", reasons[reason].reason);
		uv_sem_t *ref = NULL;

#ifdef _WIN32
		if((nr1 = InterlockedExchangeAdd(&nrlisteners[queue->reason], 0)) == 0) {
#else
		if((nr1 = __sync_add_and_fetch(&nrlisteners[queue->reason], 0)) == 0) {
#endif
			if(queue->done != NULL) {
				queue->done((void *)queue->data);
			}
		} else {
			if(threads == EVENTPOOL_THREADED) {
				if((ref = MALLOC(sizeof(uv_sem_t))) == NULL) {
					OUT_OF_MEMORY
				}
				uv_sem_init(ref, nr1-1);
			}

			struct eventpool_listener_t *listeners = eventpool_listeners;
			if(listeners == NULL) {
				if(queue->done != NULL) {
					queue->done((void *)queue->data);
				}
			}

			while(listeners) {
				if(listeners->reason == queue->reason) {
					if(nrnodes1 == nrnodes) {
						nrnodes *= 2;
						if((node = REALLOC(node, sizeof(struct threadpool_tasks_t *)*nrnodes)) == NULL) {
							OUT_OF_MEMORY
						}
					}
					if((node[nrnodes1] = MALLOC(sizeof(struct threadpool_tasks_t))) == NULL) {
						OUT_OF_MEMORY
					}
					node[nrnodes1]->func = listeners->func;
					node[nrnodes1]->userdata = queue->data;
					node[nrnodes1]->done = queue->done;
					node[nrnodes1]->ref = ref;
					node[nrnodes1]->reason = listeners->reason;
					nrnodes1++;
				}
				listeners = listeners->next;
			}
		}
		eventqueue = eventqueue->next;
		FREE(queue);
	}
	uv_mutex_unlock(&listeners_lock);

	if(nrnodes1 > 0) {
		for(i=0;i<nrnodes1;i++) {
			nrlisteners1[node[i]->reason]++;
			if(threads == EVENTPOOL_NO_THREADS) {
				node[i]->func(node[i]->reason, node[i]->userdata);

#ifdef _WIN32
				if(nrlisteners1[node[i]->reason] == InterlockedExchangeAdd(&nrlisteners[node[i]->reason], 0)) {
#else
				if(nrlisteners1[node[i]->reason] == __sync_add_and_fetch(&nrlisteners[node[i]->reason], 0)) {
#endif
					if(node[i]->done != NULL) {
						node[i]->done((void *)node[i]->userdata);
					}
					for(x=0;x<REASON_END;x++) {
						nrlisteners1[x] = 0;
					}
				}
			} else {
				tpdata = MALLOC(sizeof(struct threadpool_data_t));
				if(tpdata == NULL) {
					OUT_OF_MEMORY
				}
				tpdata->userdata = node[i]->userdata;
				tpdata->func = node[i]->func;
				tpdata->done = node[i]->done;
				tpdata->ref = node[i]->ref;
				tpdata->reason = node[i]->reason;
				tpdata->priority = reasons[node[i]->reason].priority;

				uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
				if(tp_work_req == NULL) {
					OUT_OF_MEMORY
				}
				tp_work_req->data = tpdata;
				if(uv_queue_work(uv_default_loop(), tp_work_req, fib, fib_free) < 0) {
					if(node[i]->done != NULL) {
						node[i]->done((void *)node[i]->userdata);
					}
					FREE(tpdata);
					FREE(node[i]->ref);
				}
			}
			FREE(node[i]);
		}
	}
	for(i=0;i<REASON_END;i++) {
		nrlisteners1[i] = 0;
	}
	FREE(node);
	uv_mutex_lock(&listeners_lock);
	if(eventqueue != NULL) {
		uv_async_send(async_req);
	}
	uv_mutex_unlock(&listeners_lock);
}

int eventpool_gc(void) {
	if(lockinit == 1) {
		uv_mutex_lock(&listeners_lock);
		struct eventqueue_t *queue = NULL;
		while(eventqueue) {
			queue = eventqueue;
			if(eventqueue->data != NULL && eventqueue->done != NULL) {
				eventqueue->done(eventqueue->data);
			}
			eventqueue = eventqueue->next;
			FREE(queue);
		}
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

		int i = 0;
		for(i=0;i<REASON_END;i++) {
			nrlisteners[i] = 0;
			nrlisteners1[i] = 0;
		}
		uv_mutex_unlock(&listeners_lock);
	}
	eventpoolinit = 0;
	return 0;
}

void iobuf_remove(struct iobuf_t *io, size_t n) {
	uv_mutex_lock(&io->lock);
  if(n > 0 && n <= io->len) {
    memmove(io->buf, io->buf + n, io->len - n);
    io->len -= n;
		io->buf[io->len] = 0;
  }
	uv_mutex_unlock(&io->lock);
}

size_t iobuf_append(struct iobuf_t *io, const void *buf, int len) {
  char *p = NULL;

	uv_mutex_lock(&io->lock);
  assert(io != NULL);
  assert(io->len <= io->size);

  if(len <= 0) {
  } else if(io->len + len <= io->size) {
    memcpy(io->buf + io->len, buf, len);
    io->len += len;
  } else if((p = REALLOC(io->buf, io->len + len + 1)) != NULL) {
    io->buf = p;
    memcpy(io->buf + io->len, buf, len);
    io->len += len;
    io->size = io->len;
  } else {
    len = 0;
  }
	uv_mutex_unlock(&io->lock);

  return len;
}

void uv_custom_poll_cb(uv_poll_t *req, int status, int events) {
	/*
	 * Make sure we execute in the main thread
	 */
	assert(pthread_equal(pth_main_id, pthread_self()));	
	
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct iobuf_t *send_io = NULL;
	char buffer[BUFFER_SIZE];
	uv_os_fd_t fd = 0;
	long int fromlen = 0;
	int r = 0, n = 0, action = 0;

	custom_poll_data = req->data;
	if(custom_poll_data == NULL) {
		uv_poll_stop(req);
	}
	send_io = &custom_poll_data->send_iobuf;

	if(custom_poll_data->doread == 1) {
		action |= UV_READABLE;
	}
	if(custom_poll_data->dowrite == 1) {
		action |= UV_WRITABLE;
	}

	memset(&buffer, 0, BUFFER_SIZE);

	if(uv_is_closing((uv_handle_t *)req)) {
		return;
	}
	uv_poll_stop(req);

	r = uv_fileno((uv_handle_t *)req, &fd);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return;
	}

	if(custom_poll_data->is_ssl == 1 && custom_poll_data->ssl.init == 0) {
		custom_poll_data->ssl.init = 1;
		struct mbedtls_ssl_config *ssl_conf = &ssl_client_conf;
		if(custom_poll_data->is_server == 1) {
			custom_poll_data->ssl.handshake = 1;
			ssl_conf = &ssl_server_conf;
		}
		if((r = mbedtls_ssl_setup(&custom_poll_data->ssl.ctx, ssl_conf)) < 0) {
			mbedtls_strerror(r, (char *)&buffer, BUFFER_SIZE);
			logprintf(LOG_ERR, "mbedtls_ssl_setup: %s", buffer);
			FREE(req);
			return;
		}

		if((r = mbedtls_ssl_session_reset(&custom_poll_data->ssl.ctx)) < 0) {
			mbedtls_strerror(r, (char *)&buffer, BUFFER_SIZE);
			logprintf(LOG_ERR, "mbedtls_ssl_session_reset: %s", buffer);
			FREE(req);
			return;
		}

		mbedtls_ssl_set_bio(&custom_poll_data->ssl.ctx, &fd, mbedtls_net_send, mbedtls_net_recv, NULL);
	}

	if(custom_poll_data->is_ssl == 1 && custom_poll_data->ssl.handshake == 0) {
		n = mbedtls_ssl_handshake(&custom_poll_data->ssl.ctx);
		if(n == MBEDTLS_ERR_SSL_WANT_READ) {
			action |= UV_READABLE;
			r = uv_poll_start(req, action, uv_custom_poll_cb);
		} else if(n == MBEDTLS_ERR_SSL_WANT_WRITE) {
			action |= UV_WRITABLE;
			r = uv_poll_start(req, action, uv_custom_poll_cb);
		}
		if(n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
			if(r != 0) {
				logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
			}
			return;
		}			
		if(n < 0) {
			mbedtls_strerror(n, (char *)&buffer, BUFFER_SIZE);
			logprintf(LOG_NOTICE, "mbedtls_ssl_handshake: %s", buffer);
			uv_poll_stop(req);
			return;
		}
		custom_poll_data->ssl.handshake = 1;
		action |= UV_WRITABLE;
		r = uv_poll_start(req, action, uv_custom_poll_cb);
		if(r != 0) {
			logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
			return;
		}
	}

	if(events & UV_WRITABLE) {
		if(send_io->len > 0) {
			if(custom_poll_data->is_ssl == 1) {
				n = mbedtls_ssl_write(&custom_poll_data->ssl.ctx, (unsigned char *)send_io->buf, send_io->len);
				if(n == MBEDTLS_ERR_SSL_WANT_READ) {
					action |= UV_READABLE;
					r = uv_poll_start(req, action, uv_custom_poll_cb);
				} else if(n == MBEDTLS_ERR_SSL_WANT_WRITE) {
					action |= UV_WRITABLE;
					r = uv_poll_start(req, action, uv_custom_poll_cb);
				}
				if(n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
					if(r != 0) {
						logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
						return;
					}
					return;
				} else if(n < 0) {
					mbedtls_strerror(n, (char *)&buffer, BUFFER_SIZE);
					logprintf(LOG_NOTICE, "mbedtls_ssl_handshake: %s", buffer);
					uv_poll_stop(req);
					return;
				}
			} else {
				n = (int)send((unsigned int)fd, send_io->buf, send_io->len, 0);
			}
			if(n > 0) {
				iobuf_remove(send_io, n);
				if(send_io->len > 0) {
					action |= UV_WRITABLE;
					r = uv_poll_start(req, action, uv_custom_poll_cb);
					if(r != 0) {
						logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
						return;
					}
					return;
				} else {
					action &= ~UV_WRITABLE;
					if(custom_poll_data->doclose == 1 && send_io->len == 0) {
						action &= ~UV_READABLE;
						if(custom_poll_data->close_cb != NULL) {
							custom_poll_data->close_cb(req);
							return;
						}
					} else {
						custom_poll_data->dowrite = 0;
						if(custom_poll_data->write_cb != NULL) {
							custom_poll_data->write_cb(req);
						}
					}
				}
			} else if(n == 0) {
			} else if(custom_poll_data->is_ssl == 0 && n < 0 && errno != EAGAIN && errno != EINTR) {
				if(errno == ECONNRESET) {
					uv_poll_stop(req);
					return;
				} else {
					uv_poll_stop(req);
					return;
				}
			}
		} else {
			action &= ~UV_WRITABLE;
			if(custom_poll_data->doclose == 1 && send_io->len == 0) {
				action &= ~UV_READABLE;
				if(custom_poll_data->close_cb != NULL) {
					custom_poll_data->close_cb(req);
					return;
				}
			} else {
				custom_poll_data->dowrite = 0;
				if(custom_poll_data->write_cb != NULL) {
					custom_poll_data->write_cb(req);
				}
			}

		}
	}

	if(send_io->len > 0) {
		action |= UV_WRITABLE;
		r = uv_poll_start(req, action, uv_custom_poll_cb);
		if(r != 0) {
			logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
		}
		return;
	}

	if(events & UV_READABLE) {
		if(custom_poll_data->is_ssl == 1) {
			n = mbedtls_ssl_read(&custom_poll_data->ssl.ctx, (unsigned char *)buffer, BUFFER_SIZE);
			if(n == MBEDTLS_ERR_SSL_WANT_READ) {
				action |= UV_READABLE;
				r = uv_poll_start(req, action, uv_custom_poll_cb);
			} else if(n == MBEDTLS_ERR_SSL_WANT_WRITE) {
				action |= UV_WRITABLE;
				r = uv_poll_start(req, action, uv_custom_poll_cb);
			} else if(n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
				custom_poll_data->doread = 0;
				action &= ~UV_READABLE;
				if(custom_poll_data->read_cb != NULL) {
					custom_poll_data->read_cb(req, &custom_poll_data->recv_iobuf.len, custom_poll_data->recv_iobuf.buf);
				}
			}
			if(n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
				if(r != 0) {
					logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
					return;
				}
				return;
			} else if(n < 0) {
				if(n == MBEDTLS_ERR_NET_RECV_FAILED) {
					/*
					 * FIXME: New client not yet accepted
					 */
					if(custom_poll_data->read_cb != NULL) {
						one = 1;
						custom_poll_data->read_cb(req, &one, NULL);
					}
				} else if(n != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
					mbedtls_strerror(n, (char *)&buffer, BUFFER_SIZE);
					logprintf(LOG_NOTICE, "mbedtls_ssl_handshake: %s", buffer);
					uv_poll_stop(req);
				}
				return;
			}
		} else {
			if(custom_poll_data->is_udp == 1) {
				n = (int)recv((unsigned int)fd, buffer, BUFFER_SIZE, 0);
			} else {
				n = recvfrom(fd, buffer, BUFFER_SIZE, 0, NULL, (socklen_t *)&fromlen);
			}
		}

		if(n > 0) {
			iobuf_append(&custom_poll_data->recv_iobuf, buffer, n);
			custom_poll_data->doread = 0;
			action &= ~UV_READABLE;
			if(custom_poll_data->read_cb != NULL) {
				custom_poll_data->read_cb(req, &custom_poll_data->recv_iobuf.len, custom_poll_data->recv_iobuf.buf);
			}
		} else if(n < 0 && errno != EAGAIN && errno != EINTR) {
#ifdef _WIN32
			switch(WSAGetLastError()) {
				case WSAENOTCONN:
					if(custom_poll_data->read_cb != NULL) {
						one = 1;
						custom_poll_data->read_cb(req, &one, NULL);
					}
				break;
				case WSAEWOULDBLOCK:
#else
			switch(errno) {
				case ENOTCONN:
					if(custom_poll_data->read_cb != NULL) {
						one = 1;
						custom_poll_data->read_cb(req, &one, NULL);
					}
				break;				
#if defined EAGAIN
				case EAGAIN:
#endif
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
				case EWOULDBLOCK:
#endif
#endif
				action |= UV_READABLE;
				r = uv_poll_start(req, action, uv_custom_poll_cb);
				if(r != 0) {
					logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
					return;
				}
				break;
				default:
				break;
			}
		} else if(n == 0) {
			if(custom_poll_data->read_cb != NULL) {
				minus_one = -1;
				custom_poll_data->read_cb(req, &minus_one, NULL);
			}
			custom_poll_data->doread = 0;
			action &= ~UV_READABLE;
		}
	}
	if(action > 0) {
		r = uv_poll_start(req, action, uv_custom_poll_cb);
		if(r != 0) {
			logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
			return;
		}
	}
}

static void iobuf_free(struct iobuf_t *iobuf) {
  if(iobuf != NULL) {
		uv_mutex_lock(&iobuf->lock);
    if(iobuf->buf != NULL) {
			FREE(iobuf->buf);
		}
		iobuf->len = iobuf->size = 0;
  }
	uv_mutex_unlock(&iobuf->lock);
}

void uv_custom_poll_free(struct uv_custom_poll_t *data) {
	if(data->is_ssl == 1) {
		mbedtls_ssl_free(&data->ssl.ctx);
	}
	if(data->send_iobuf.size > 0) {
		iobuf_free(&data->send_iobuf);
	}
	if(data->recv_iobuf.size > 0) {
		iobuf_free(&data->recv_iobuf);
	}

	FREE(data);
}

static void iobuf_init(struct iobuf_t *iobuf, size_t initial_size) {
  iobuf->len = iobuf->size = 0;
  iobuf->buf = NULL;
	uv_mutex_init(&iobuf->lock);
}

void uv_custom_poll_init(struct uv_custom_poll_t **custom_poll, uv_poll_t *poll, void *data) {
	if((*custom_poll = MALLOC(sizeof(struct uv_custom_poll_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(*custom_poll, '\0', sizeof(struct uv_custom_poll_t));
	iobuf_init(&(*custom_poll)->send_iobuf, 0);
	iobuf_init(&(*custom_poll)->recv_iobuf, 0);	
	
	poll->data = *custom_poll;
	(*custom_poll)->data = data;
}

int uv_custom_close(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;

	if(custom_poll_data != NULL) {
		custom_poll_data->doclose = 1;
	}
	return 0;
}

int uv_custom_read(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	int r = 0, doread = 1, dowrite = 0, action = 0;

	if(uv_is_closing((uv_handle_t *)req)) {
		return -1;
	}

	if(custom_poll_data != NULL) {
		doread = custom_poll_data->doread = 1;
		dowrite = custom_poll_data->dowrite;
	}

	if(doread == 1) {
		action |= UV_READABLE;
	}
	if(dowrite == 1) {
		action |= UV_WRITABLE;
	}

	uv_poll_start(req, action, uv_custom_poll_cb);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
		return -1;
	}
	return 0;
}

int uv_custom_write(uv_poll_t *req) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	int r = 0, doread = 0, dowrite = 1, action = 0;

	if(uv_is_closing((uv_handle_t *)req)) {
		return -1;
	}

	if(custom_poll_data != NULL) {
		doread = custom_poll_data->doread;
		dowrite = custom_poll_data->dowrite = 1;
	}

	if(doread == 1) {
		action |= UV_READABLE;
	}
	if(dowrite == 1) {
		action |= UV_WRITABLE;
	}

	r = uv_poll_start(req, action, uv_custom_poll_cb);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_poll_start: %s", uv_strerror(r));
		return -1;
	}
	return 0;
}

void eventpool_init(enum eventpool_threads_t t) {
	/*
	 * Make sure we execute in the main thread
	 */
	assert(pthread_equal(pth_main_id, pthread_self()));

	if(running() == 0) {
		return;
	}

	if(eventpoolinit == 1) {
		return;
	}
	eventpoolinit = 1;
	threads = t;
	if((async_req = MALLOC(sizeof(uv_async_t))) == NULL) {
		OUT_OF_MEMORY
	}
	uv_async_init(uv_default_loop(), async_req, eventpool_execute);

	if(lockinit == 0) {
		lockinit = 1;
		// pthread_mutexattr_init(&listeners_attr);
		// pthread_mutexattr_settype(&listeners_attr, PTHREAD_MUTEX_RECURSIVE);
		// pthread_mutex_init(&listeners_lock, &listeners_attr);
		uv_mutex_init(&listeners_lock);
	}
}

enum eventpool_threads_t eventpool_threaded(void) {
	return threads;
}
