/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "pilight.h"
#include "eventpool.h"
#include "threadpool.h"
#include "network.h"
#include "log.h"
#include "gc.h"
#include "socket.h"

static char recvBuff[BUFFER_SIZE];
static unsigned short socket_loop = 1;
static unsigned int socket_port = 0;
static unsigned int static_port = 0;

struct eventpool_fd_t *socket_server = NULL;

static void *reason_socket_disconnected_free(void *param) {
	struct reason_socket_disconnected_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_socket_connected_free(void *param) {
	struct reason_socket_connected_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_socket_received_free(void *param) {
	struct reason_socket_received_t *data = param;
	FREE(data->buffer);
	FREE(data);
	return NULL;
}

int socket_gc(void) {
	// int x = 0;

	socket_loop = 0;
	// /* Wakeup all our select statement so the socket_wait and
		 // socket_read functions can actually close and the
		 // all threads using sockets can end gracefully */

	// if(socket_loopback > 0) {
		// send(socket_loopback, "1", 1, MSG_NOSIGNAL);
		// socket_close(socket_loopback);
	// }

	// if(waitMessage != NULL) {
		// FREE(waitMessage);
	// }

	logprintf(LOG_DEBUG, "garbage collected socket library");
	return EXIT_SUCCESS;
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	struct sockaddr_in servaddr;
	socklen_t socklen = sizeof(servaddr);
	char buf[INET_ADDRSTRLEN+1];
	switch(event) {
		case EV_CONNECT_SUCCESS:
			eventpool_fd_enable_read(node);
		break;
		case EV_READ: {
			int x = socket_recv(node->fd, &node->buffer, &node->len);

			if(x == -1) {
				return -1;
			} else if(x == 0) {
				eventpool_fd_enable_read(node);
				return 0;
			} else {
				if(strstr(node->buffer, "\n") != NULL && json_validate(node->buffer) == true) {
					char **array = NULL;
					unsigned int n = explode(node->buffer, "\n", &array), q = 0;
					for(q=0;q<n;q++) {
						struct reason_socket_received_t *data = MALLOC(sizeof(struct reason_socket_received_t));
						if(data == NULL) {
							OUT_OF_MEMORY
						}
						data->fd = node->fd;
						if((data->buffer = MALLOC(strlen(array[q])+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(data->buffer, array[q]);
						strncpy(data->type, "socket", 255);
						eventpool_trigger(REASON_SOCKET_RECEIVED, reason_socket_received_free, data);
					}
					array_free(&array, n);
				} else {
					struct reason_socket_received_t *data = MALLOC(sizeof(struct reason_socket_received_t));
					if(data == NULL) {
						OUT_OF_MEMORY
					}
					data->fd = node->fd;
					if((data->buffer = MALLOC(strlen(node->buffer)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(data->buffer, node->buffer);
					strncpy(data->type, "socket", 255);
					eventpool_trigger(REASON_SOCKET_RECEIVED, reason_socket_received_free, data);
				}
				FREE(node->buffer);
				eventpool_fd_enable_read(node);
				node->len = 0;
			}
		} break;
		case EV_DISCONNECTED: {
			if(getpeername(node->fd, (struct sockaddr *)&servaddr, &socklen) == 0) {
				memset(&buf, '\0', INET_ADDRSTRLEN+1);
				inet_ntop(AF_INET, (void *)&(servaddr.sin_addr), buf, INET_ADDRSTRLEN+1);
				logprintf(LOG_DEBUG, "client disconnected, ip %s, port %d", buf, ntohs(servaddr.sin_port));
			}
			struct reason_socket_disconnected_t *data = MALLOC(sizeof(struct reason_socket_disconnected_t));
			if(data == NULL) {
				OUT_OF_MEMORY
			}
			data->fd = node->fd;
			eventpool_trigger(REASON_SOCKET_DISCONNECTED, reason_socket_disconnected_free, data);

			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

static int server_callback(struct eventpool_fd_t *node, int event) {
	struct sockaddr_in servaddr;
	socklen_t socklen = sizeof(servaddr);
	int socket_client = 0;
	char buf[INET_ADDRSTRLEN+1];

	switch(event) {
		case EV_LISTEN_SUCCESS: {
			static struct linger linger = { 0, 0 };
#ifdef _WIN32
			unsigned int lsize = sizeof(struct linger);
#else
			socklen_t lsize = sizeof(struct linger);
#endif
			setsockopt(node->fd, SOL_SOCKET, SO_LINGER, (void *)&linger, lsize);

			if(getsockname(node->fd, (struct sockaddr *)&servaddr, (socklen_t *)&socklen) == -1) {
				perror("getsockname");
			} else {
				socket_port = ntohs(servaddr.sin_port);
				logprintf(LOG_INFO, "daemon listening to port: %d", socket_port);
			}
		} break;
		case EV_READ: {
			if((socket_client = accept(node->fd, (struct sockaddr *)&servaddr, (socklen_t *)&socklen)) < 0) {
				logprintf(LOG_ERR, "failed to accept client");
				return -1;
			}
			memset(&buf, '\0', INET_ADDRSTRLEN+1);
			inet_ntop(AF_INET, (void *)&(servaddr.sin_addr), buf, INET_ADDRSTRLEN+1);

			if(whitelist_check(buf) != 0) {
				logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", buf, ntohs(servaddr.sin_port));
				eventpool_fd_remove(node);
			} else {
				logprintf(LOG_INFO, "new client, ip: %s, port: %d", buf, ntohs(servaddr.sin_port));
				logprintf(LOG_DEBUG, "client fd: %d", socket_client);

				struct reason_socket_connected_t *data = MALLOC(sizeof(struct reason_socket_connected_t));
				if(data == NULL) {
					OUT_OF_MEMORY
				}
				data->fd = socket_client;
				eventpool_trigger(REASON_SOCKET_CONNECTED, reason_socket_connected_free, data);

				eventpool_fd_add("socket client", socket_client, client_callback, NULL, NULL);
			}
		}
		break;
		case EV_BIND_FAILED: {
			if(getpeername(node->fd, (struct sockaddr *)&servaddr, &socklen) == 0) {
				logprintf(LOG_ERR, "cannot bind to socket port %d, address already in use?", ntohs(servaddr.sin_port));
			} else {
				logprintf(LOG_ERR, "cannot bind to socket port, address already in use?");
			}
		} break;
	}
	return 0;
}

int socket_recv(int fd, char **data, size_t *ptr) {
	int bytes = 0;
	size_t msglen = 0;
	int len = (int)strlen(EOSS);
	char buffer[BUFFER_SIZE];

	memset(buffer, '\0', BUFFER_SIZE);

	bytes = recv(fd, buffer, BUFFER_SIZE, 0);

	if(bytes <= 0) {
		return -1;
	} else {
		*ptr += bytes;
		if((*data = REALLOC(*data, *ptr+1)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(&(*data)[(*ptr-bytes)], '\0', bytes+1);
		memcpy(&(*data)[(*ptr-bytes)], buffer, bytes);
		msglen = strlen(*data);
	}

	if(*data != NULL && msglen > 0) {
		/* When a stream is larger then the buffer size, it has to contain
			 the pilight delimiter to know when the stream ends. If the stream
			 is shorter then the buffer size, we know we received the full stream */
		int l = 0;
		if(((l = strncmp(&(*data)[*ptr-len], EOSS, (unsigned int)len)) == 0) || *ptr < BUFFER_SIZE) {
			/* If the socket contains buffered TCP buffers, separate them by
				 changing the delimiters into newlines */
			if(*ptr > msglen) {
				int i = 0;
				for(i=0;i<*ptr;i++) {
					if(i+(len-1) < *ptr && strncmp(&(*data)[i], EOSS, (size_t)len) == 0) {
						memmove(&(*data)[i], &(*data)[i+(len-1)], (size_t)(*ptr-(i+(len-1))));
						*ptr -= (len-1);
						*data[i] = '\n';
					}
				}
				*data[*ptr] = '\0';
			} else {
				if(l == 0) {
					(*data)[*ptr-len] = '\0'; // remove delimiter
					return *ptr;
				} else {
					(*data)[*ptr] = '\0';
					return 0;
				}
				if(strcmp(*data, "1") == 0 || strcmp(*data, "BEAT") == 0) {
					FREE(*data);
					return *ptr;
				}
			}
		}
	}
	return 0;
}

static void *adhoc_mode(void *param) {
	if(socket_server != NULL) {
		eventpool_fd_remove(socket_server);
		logprintf(LOG_INFO, "shut down socket server due to adhoc mode");
		socket_server = NULL;
	}
	return NULL;
}

static void *socket_restart(void *param) {
	if(socket_server != NULL) {
		eventpool_fd_remove(socket_server);
		socket_server = NULL;
	}

	socket_server = eventpool_socket_add("socket server", NULL, static_port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, NULL, NULL);

	return NULL;
}

static void *socket_send_thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_socket_send_t *data = task->userdata;

	if(strcmp(data->type, "socket") == 0) {
		socket_write(data->fd, data->buffer);
	}
	return NULL;
}

/* Start the socket server */
int socket_start(unsigned short port) {
	if(socket_server != NULL) {
		eventpool_fd_remove(socket_server);
		socket_server = NULL;
	}
	static_port = port;
	socket_server = eventpool_socket_add("socket server", NULL, port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, NULL, NULL);
	eventpool_callback(REASON_ADHOC_CONNECTED, adhoc_mode);
	eventpool_callback(REASON_ADHOC_DISCONNECTED, socket_restart);
	eventpool_callback(REASON_SOCKET_SEND, socket_send_thread);
	return 0;
}

int socket_timeout_connect(int sockfd, struct sockaddr *serv_addr, int sec) {
	struct timeval tv;
	fd_set fdset;
	int valopt;
	socklen_t lon = 0;

#ifdef _WIN32
	unsigned long on = 1;
	unsigned long off = 0;
	ioctlsocket(sockfd, FIONBIO, &on);
#else
	long arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg | O_NONBLOCK);
#endif

	if(connect(sockfd, serv_addr, sizeof(struct sockaddr)) < 0) {
		if(errno == EINPROGRESS) {
			tv.tv_sec = sec;
			tv.tv_usec = 0;
			FD_ZERO(&fdset);
			FD_SET(sockfd, &fdset);
			if(select(sockfd+1, NULL, &fdset, NULL, &tv) > 0) {
				 lon = sizeof(int);
				 getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
				 if(valopt > 0) {
					return -3;
				 }
			} else {
				return -2;
			}
		} else {
			return -1;
		}
	}

#ifdef _WIN32
	ioctlsocket(sockfd, FIONBIO, &off);
#else
	arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg & ~O_NONBLOCK);
#endif

	return 0;
}

unsigned int socket_get_port(void) {
	return socket_port;
}

int socket_get_fd(void) {
	return -1;
}

int socket_connect(char *address, unsigned short port) {
	eventpool_socket_add("socket client", address, port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, client_callback, NULL, NULL);
	return 0;
}

int socket_connect1(char *address, unsigned short port, int (*callback)(struct eventpool_fd_t *, int)) {
	eventpool_socket_add("socket client", address, port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, callback, NULL, NULL);
	return 0;
}

void socket_close(int sockfd) {
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	char buf[INET_ADDRSTRLEN+1];
	struct eventpool_fd_t *node = NULL;

	if(sockfd > 0) {
		if(getpeername(sockfd, (struct sockaddr*)&address, (socklen_t*)&addrlen) == 0) {
			memset(&buf, '\0', INET_ADDRSTRLEN+1);
			inet_ntop(AF_INET, (void *)&(address.sin_addr), buf, INET_ADDRSTRLEN+1);
			logprintf(LOG_DEBUG, "client disconnected, fd %d, ip %s, port %d", sockfd, buf, ntohs(address.sin_port));
		}

		if(eventpool_fd_select(sockfd, &node) == 0) {
			eventpool_fd_remove(node);
		} else {
			shutdown(sockfd, SHUT_WR);
			close(sockfd);
		}
	}
}

int socket_write(int sockfd, const char *msg, ...) {
	va_list ap;
	// int bytes = -1;
	int /*ptr = 0, */n = 0, /*x = BUFFER_SIZE, */len = (int)strlen(EOSS);
	char *sendBuff = NULL;

	if(strlen(msg) > 0 && sockfd > 0) {

		va_start(ap, msg);
#ifdef _WIN32
		n = _vscprintf(msg, ap);
#else
		n = vsnprintf(NULL, 0, msg, ap);
#endif
		if(n == -1) {
			logprintf(LOG_ERR, "improperly formatted string: %s", msg);
			return -1;
		}
		n += (int)len;
		va_end(ap);

		if((sendBuff = MALLOC((size_t)n+1)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(sendBuff, '\0', (size_t)n+1);

		va_start(ap, msg);
		vsprintf(sendBuff, msg, ap);
		va_end(ap);

		memcpy(&sendBuff[n-len], EOSS, (size_t)len);

		if(eventpool_fd_write(sockfd, sendBuff, strlen(sendBuff)) == -1) {
			FREE(sendBuff);
			return -1;
		}

		sendBuff[n-(len-1)] = '\0';
		sendBuff[n-(len)] = '\n';
		logprintf(LOG_DEBUG, "socket write succeeded: %s", sendBuff);

		FREE(sendBuff);
	}
	return n;
}

int socket_read(int sockfd, char **message, time_t timeout) {
	struct timeval tv;
	int bytes = 0;
	size_t msglen = 0;
	int ptr = 0, n = 0, len = (int)strlen(EOSS);
	fd_set fdsread;
#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(sockfd, FIONBIO, &on);
#else
 	fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif

	if(timeout > 0) {
		tv.tv_sec = timeout;
    tv.tv_usec = 0;
	}

	while(socket_loop && sockfd > 0) {
		FD_ZERO(&fdsread);
		FD_SET((unsigned long)sockfd, &fdsread);

		do {
			if(timeout > 0) {
				n = select(sockfd+1, &fdsread, NULL, NULL, &tv);
			} else {
				n = select(sockfd+1, &fdsread, NULL, NULL, 0);
			}
		} while(n == -1 && errno == EINTR && socket_loop);
		if(timeout > 0 && n == 0) {
			return 1;
		}
		/* Immediatly stop loop if the select was waken up by the garbage collector */
		if(socket_loop == 0) {
			break;
		}
		if(n == -1) {
			return -1;
		} else if(n > 0) {
			if(FD_ISSET((unsigned long)sockfd, &fdsread)) {
				bytes = (int)recv(sockfd, recvBuff, BUFFER_SIZE, 0);

				if(bytes <= 0) {
					return -1;
				} else {
					ptr+=bytes;
					if((*message = REALLOC(*message, (size_t)ptr+1)) == NULL) {
						OUT_OF_MEMORY
					}
					memset(&(*message)[(ptr-bytes)], '\0', (size_t)bytes+1);
					memcpy(&(*message)[(ptr-bytes)], recvBuff, (size_t)bytes);
					msglen = strlen(*message);
				}
				if(*message && msglen > 0) {
					/* When a stream is larger then the buffer size, it has to contain
					   the pilight delimiter to know when the stream ends. If the stream
					   is shorter then the buffer size, we know we received the full stream */
					int l = 0;
					if(((l = strncmp(&(*message)[ptr-(len)], EOSS, (unsigned int)(len))) == 0) || ptr < BUFFER_SIZE) {
						/* If the socket contains buffered TCP messages, separate them by
						   changing the delimiters into newlines */
						if(ptr > msglen) {
							int i = 0;
							for(i=0;i<ptr;i++) {
								if(i+(len-1) < ptr && strncmp(&(*message)[i], EOSS, (size_t)len) == 0) {
									memmove(&(*message)[i], message[i+(len-1)], (size_t)(ptr-(i+(len-1))));
									ptr-=(len-1);
									(*message)[i] = '\n';
								}
							}
							(*message)[ptr] = '\0';
						} else {
							if(l == 0) {
								(*message)[ptr-(len)] = '\0'; // remove delimiter
							} else {
								(*message)[ptr] = '\0';
							}
							if(strcmp(*message, "1") == 0 || strcmp(*message, "BEAT") == 0) {
								return -1;
							}
						}
						return 0;
					}
				}
			}
		}
	}

	return -1;
}
