/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#ifndef _WIN32
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "../libs/libuv/uv.h"
#include "pilight.h"
#include "network.h"
#include "log.h"
#include "gc.h"
#include "socket.h"

typedef struct data_t {
	int steps;
	char *buffer;
	size_t buflen;
	int endpoint;
} data_t;

typedef struct client_list_t {
	uv_stream_t *req;
	int fd;
	struct data_t *data;

	struct client_list_t *next;
} client_list_t;

static uv_mutex_t lock;
static int lock_init = 0;
static unsigned int static_port = 0;
static unsigned int socket_started = 0;
static struct client_list_t *client_list = NULL;

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
	uv_mutex_lock(&lock);
	struct client_list_t *tmp = NULL;
	while(client_list) {
		tmp = client_list;
		if(tmp->data != NULL) {
			if(tmp->data->buflen > 0) {
				FREE(tmp->data->buffer);
				tmp->data->buflen = 0;
			}
			FREE(tmp->data);
			tmp->data = NULL;
		}
		client_list = client_list->next;
		FREE(tmp);
	}
	uv_mutex_unlock(&lock);

	socket_started = 0;
	static_port = 0;
	client_list = NULL;

	logprintf(LOG_DEBUG, "garbage collected socket library");
	return EXIT_SUCCESS;
}

int socket_recv(char *buffer, int bytes, char **data, size_t *ptr) {
	int len = (int)strlen(EOSS);
	size_t msglen = 0;
	*ptr += bytes;
	if((*data = REALLOC(*data, *ptr+1)) == NULL) {
		OUT_OF_MEMORY
	}
	memset(&(*data)[(*ptr-bytes)], '\0', bytes+1);
	memcpy(&(*data)[(*ptr-bytes)], buffer, bytes);
	msglen = strlen(*data);

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
				} else {
					(*data)[*ptr] = '\0';
				}
				return *ptr;
				if(strcmp(*data, "1") == 0 || strcmp(*data, "BEAT") == 0) {
					FREE(*data);
					return *ptr;
				}
			}
		}
	}
	return 0;	
}

// static void *adhoc_mode(int reason, void *param) {
	// if(socket_server != NULL) {
		// eventpool_fd_remove(socket_server);
		// logprintf(LOG_INFO, "shut down socket server due to adhoc mode");
		// socket_server = NULL;
	// }
	// return NULL;
// }

// static void *socket_restart(int reason, void *param) {
	// if(socket_server != NULL) {
		// eventpool_fd_remove(socket_server);
		// socket_server = NULL;
	// }

	// socket_server = eventpool_socket_add("socket server", NULL, static_port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, NULL, NULL);

	// return NULL;
// }

// static void *socket_send_thread(int reason, void *param) {
	// struct threadpool_tasks_t *task = param;
	// struct reason_socket_send_t *data = task->userdata;

	// if(strcmp(data->type, "socket") == 0) {
		// socket_write(data->fd, data->buffer);
	// }
	// return NULL;
// }

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
	buf->base = malloc(suggested_size);
	buf->len = suggested_size;
	memset(buf->base, '\0', suggested_size);
}

static void read_cb(uv_stream_t *client_req, ssize_t nread, const uv_buf_t *buf) {
	uv_os_fd_t fd = 0;

	struct data_t *data = client_req->data;

	int r = uv_fileno((uv_handle_t *)client_req, &fd);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		FREE(data);
		return;
	}

  if(nread < 0) {
		struct sockaddr_in addr;
		socklen_t socklen = sizeof(addr);
		char buf1[INET_ADDRSTRLEN+1];

#ifdef _WIN32
		if(getpeername((SOCKET)fd, (struct sockaddr *)&addr, &socklen) == 0) {
#else
		if(getpeername(fd, (struct sockaddr *)&addr, &socklen) == 0) {
#endif
			memset(&buf1, '\0', INET_ADDRSTRLEN+1);
			inet_ntop(AF_INET, (void *)&(addr.sin_addr), buf1, INET_ADDRSTRLEN+1);
			logprintf(LOG_DEBUG, "client disconnected, ip %s, port %d", buf1, ntohs(addr.sin_port));
		}
		struct reason_socket_disconnected_t *data1 = MALLOC(sizeof(struct reason_socket_disconnected_t));
		if(data1 == NULL) {
			OUT_OF_MEMORY
		}
		data1->fd = (int)fd;
		eventpool_trigger(REASON_SOCKET_DISCONNECTED, reason_socket_disconnected_free, data1);

		if(data->buflen > 0) {
			FREE(data->buffer);
			data->buflen = 0;
		}
		FREE(data);
		client_req->data = NULL;

		free(buf->base);
    uv_close((uv_handle_t *)client_req, close_cb);
    return;
  }
	
	if(nread > 0) {
		int x = socket_recv(buf->base, nread, &data->buffer, &data->buflen);
		if(x > 0) {
			if(strstr(data->buffer, "\n") != NULL) {
				char **array = NULL;
				unsigned int n = explode(data->buffer, "\n", &array), q = 0;
				for(q=0;q<n;q++) {
					struct reason_socket_received_t *data1 = MALLOC(sizeof(struct reason_socket_received_t));
					if(data1 == NULL) {
						OUT_OF_MEMORY
					}
					data1->fd = (int)fd;
					if((data1->buffer = MALLOC(strlen(array[q])+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(data1->buffer, array[q]);
					strncpy(data1->type, "socket", 255);
					data1->endpoint = data->endpoint;
					eventpool_trigger(REASON_SOCKET_RECEIVED, reason_socket_received_free, data1);
				}
				array_free(&array, n);
			} else {
				struct reason_socket_received_t *data1 = MALLOC(sizeof(struct reason_socket_received_t));
				if(data1 == NULL) {
					OUT_OF_MEMORY
				}
				data1->fd = (int)fd;
				if((data1->buffer = MALLOC(strlen(data->buffer)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(data1->buffer, data->buffer);
				strncpy(data1->type, "socket", 255);
				data1->endpoint = data->endpoint;
				eventpool_trigger(REASON_SOCKET_RECEIVED, reason_socket_received_free, data1);
			}
			FREE(data->buffer);
			data->buflen = 0;
		}
		free(buf->base);
	}
}

static void connect_cb(uv_stream_t *server_req, int status) {
	uv_os_fd_t fd;
	int r = 0;
	if(status < 0) {
		logprintf(LOG_ERR, "connection_cb: %s", uv_strerror(status));
		return;
	}

	uv_tcp_t *client_req = MALLOC(sizeof(uv_tcp_t));
	if(client_req == NULL) {
		OUT_OF_MEMORY
	}
	r = uv_tcp_init(uv_default_loop(), client_req);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_tcp_init: %s", uv_strerror(r));
		return;
	}

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	memset(data, '\0', sizeof(struct data_t));
	data->buffer = NULL;
	data->buflen = 0;
	data->endpoint = SOCKET_SERVER;
	client_req->data = data;

	if(uv_accept(server_req, (uv_stream_t *)client_req) == 0) {

		struct sockaddr_in addr;
		socklen_t socklen = sizeof(addr);
		char buf[INET_ADDRSTRLEN+1];

		r = uv_fileno((uv_handle_t *)client_req, &fd);
		if(r != 0) {
			logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
			FREE(data);
			return;
		}

#ifdef _WIN32
		if(getpeername((SOCKET)fd, (struct sockaddr *)&addr, &socklen) == 0) {
#else
		if(getpeername(fd, (struct sockaddr *)&addr, &socklen) == 0) {
#endif
			memset(&buf, '\0', INET_ADDRSTRLEN+1);
			inet_ntop(AF_INET, (void *)&(addr.sin_addr), buf, INET_ADDRSTRLEN+1);

			if(whitelist_check(buf) != 0) {
				logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", buf, ntohs(addr.sin_port));
				FREE(data);
				return;
			} else {
				logprintf(LOG_INFO, "new client, ip: %s, port: %d", buf, ntohs(addr.sin_port));
				logprintf(LOG_DEBUG, "client fd: %d", fd);
			}

			if(lock_init == 0) {
				lock_init = 1;
				uv_mutex_init(&lock);
			}

			struct client_list_t *node = MALLOC(sizeof(struct client_list_t));
			if(node == NULL) {
				OUT_OF_MEMORY
			}
			node->req = (uv_stream_t *)client_req;
			node->fd = (int)fd;
			node->data = data;

			uv_mutex_lock(&lock);
			node->next = client_list;
			client_list = node;
			uv_mutex_unlock(&lock);

			struct reason_socket_connected_t *data1 = MALLOC(sizeof(struct reason_socket_connected_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY
			}
			data1->fd = (int)fd;
			eventpool_trigger(REASON_SOCKET_CONNECTED, reason_socket_connected_free, data1);
		}

		uv_read_start((uv_stream_t *)client_req, alloc_cb, read_cb);
	} else {
		FREE(data);
		uv_close((uv_handle_t *)client_req, close_cb);
	}	
}

static void on_write(uv_write_t *req, int status) {
	uv_buf_t *buf = req->data;

	buf->base[buf->len-1] = '\0';
	buf->base[buf->len-2] = '\n';

	logprintf(LOG_DEBUG, "socket write succeeded: %s", buf->base);

	FREE(buf->base);
	FREE(buf);
	FREE(req);
}

void on_connect(uv_connect_t *connect_req, int status) {
	uv_os_fd_t fd = 0;
	int r = uv_fileno((uv_handle_t *)connect_req->handle, &fd);
	if(r != 0) {
		FREE(connect_req);
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return;
	}

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	memset(data, '\0', sizeof(struct data_t));
	data->buffer = NULL;
	data->buflen = 0;
	data->endpoint = SOCKET_CLIENT;
	connect_req->handle->data = data;

	if(lock_init == 0) {
		lock_init = 1;
		uv_mutex_init(&lock);
	}

	struct client_list_t *node = MALLOC(sizeof(struct client_list_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	node->req = connect_req->handle;
	node->fd = (int)fd;
	node->data = data;

	uv_mutex_lock(&lock);
	node->next = client_list;
	client_list = node;
	uv_mutex_unlock(&lock);

	struct reason_socket_connected_t *data1 = MALLOC(sizeof(struct reason_socket_connected_t));
	if(data1 == NULL) {
		OUT_OF_MEMORY
	}
	data1->fd = (int)fd;
	eventpool_trigger(REASON_SOCKET_CONNECTED, reason_socket_connected_free, data1);

	uv_read_start((uv_stream_t *)connect_req->handle, alloc_cb, read_cb);

	FREE(connect_req);
}

int socket_connect(char *address, unsigned short port) {
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	struct sockaddr_in addr;
	int r = 0;

	uv_tcp_t *tcp_req = MALLOC(sizeof(uv_tcp_t));
	if(tcp_req == NULL) {
		OUT_OF_MEMORY
	}

	r = uv_tcp_init(uv_default_loop(), tcp_req);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_tcp_init: %s", uv_strerror(r));
		goto close;
	}

	r = uv_ip4_addr(address, port, &addr);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
		goto close;
	}

	uv_connect_t *connect_req = MALLOC(sizeof(uv_connect_t));
	if(connect_req == NULL) {
		OUT_OF_MEMORY
	}

	uv_tcp_connect(connect_req, tcp_req, (struct sockaddr *)&addr, on_connect);

	return 0;

close:
	FREE(tcp_req);
	return -1;
}

/* Start the socket server */
int socket_start(unsigned short port) {
	if(socket_started == 1) {
		return 0;
	}
	static_port = port;
	socket_started = 1;

	struct sockaddr_in addr;
	int r = 0;

	uv_tcp_t *tcp_req = MALLOC(sizeof(uv_tcp_t));
	if(tcp_req == NULL) {
		OUT_OF_MEMORY
	}

	r = uv_tcp_init(uv_default_loop(), tcp_req);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_tcp_init: %s", uv_strerror(r));
		goto close;
	}

	r = uv_ip4_addr("0.0.0.0", port, &addr);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
		goto close;
	}

	r = uv_tcp_bind(tcp_req, (const struct sockaddr *)&addr, 0);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_tcp_bind: %s", uv_strerror(r));
		goto close;
	}

	r = uv_listen((uv_stream_t *)tcp_req, 3, connect_cb);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_listen: %s", uv_strerror(r));
		goto close;
	}

	logprintf(LOG_INFO, "socket server started at port #%d", port);

	return 0;

close:
	FREE(tcp_req);
	return -1;
	
	// if(socket_server != NULL) {
		// eventpool_fd_remove(socket_server);
		// socket_server = NULL;
	// }
	// socket_server = eventpool_socket_add("socket server", NULL, port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, NULL, NULL);
	// eventpool_callback(REASON_ADHOC_CONNECTED, adhoc_mode);
	// eventpool_callback(REASON_ADHOC_DISCONNECTED, socket_restart);
	// eventpool_callback(REASON_SOCKET_SEND, socket_send_thread);
}

unsigned int socket_get_port(void) {
	return static_port;
}

static void client_remove(int fd) {
	uv_mutex_lock(&lock);
	struct client_list_t *currP, *prevP;

	prevP = NULL;

	for(currP = client_list; currP != NULL; prevP = currP, currP = currP->next) {
		if(currP->fd == fd) {
			if(prevP == NULL) {
				client_list = currP->next;
			} else {
				prevP->next = currP->next;
			}

			if(currP->data != NULL) {
				if(currP->data->buflen > 0) {
					FREE(currP->data->buffer);
					currP->data->buflen = 0;
				}
				FREE(currP->data);
			}
			FREE(currP);
			break;
		}
	}
	uv_mutex_unlock(&lock);
}

void socket_close(int sockfd) {
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	char buf[INET_ADDRSTRLEN+1];

	if(sockfd > 0) {
#ifdef _WIN32
		if(getpeername((SOCKET)sockfd, (struct sockaddr *)&addr, &socklen) == 0) {
#else
		if(getpeername(sockfd, (struct sockaddr *)&addr, &socklen) == 0) {
#endif
			memset(&buf, '\0', INET_ADDRSTRLEN+1);
			inet_ntop(AF_INET, (void *)&(addr.sin_addr), buf, INET_ADDRSTRLEN+1);
			logprintf(LOG_DEBUG, "client disconnected, fd %d, ip %s, port %d", sockfd, buf, htons(addr.sin_port));
		}

		client_remove(sockfd);

#ifdef _WIN32
		shutdown(sockfd, SD_BOTH);
#else
		shutdown(sockfd, SHUT_RDWR);
#endif
		close(sockfd);
	}
}

int socket_write(int sockfd, const char *msg, ...) {
	va_list ap;
	int n = 0, len = (int)strlen(EOSS);
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

		uv_stream_t *req = NULL;
		uv_mutex_lock(&lock);
		struct client_list_t *tmp = client_list;
		while(tmp) {
			if(tmp->fd == sockfd) {
				req = tmp->req;
				break;
			}
			tmp = tmp->next;
		}
		uv_mutex_unlock(&lock);

		if(req != NULL) {
			uv_write_t *write_req = NULL;
			if((write_req = MALLOC(sizeof(uv_write_t))) == NULL) {
				OUT_OF_MEMORY
			}

			uv_buf_t *buf = MALLOC(sizeof(uv_buf_t));
			if(buf == NULL) {
				OUT_OF_MEMORY
			}
			buf->len = strlen(sendBuff);

			if((buf->base = MALLOC(buf->len+1)) == NULL) {
				OUT_OF_MEMORY
			}
			strncpy(buf->base, sendBuff, buf->len);

			write_req->data = buf;
			uv_write(write_req, req, buf, 1, on_write);
		}

		FREE(sendBuff);
	}
	return n;
}
