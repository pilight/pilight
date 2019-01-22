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
#include <assert.h>
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
#include "socket.h"

typedef struct data_t {
	char *buffer;
	ssize_t len;
	void (*read_cb)(int, char *, ssize_t, char **, ssize_t *);
} data_t;

typedef struct client_list_t {
	uv_poll_t *req;
	struct uv_custom_poll_t *data;
	int fd;

	struct client_list_t *next;
} client_list_t;

#ifdef _WIN32
	static uv_mutex_t lock;
#else
	static pthread_mutex_t lock;
	static pthread_mutexattr_t attr;
#endif
static int lock_init = 0;
static unsigned int static_port = 0;
static unsigned int socket_started = 0;
static struct client_list_t *client_list = NULL;
static int socket_override_whitelist_check = -1;

static void poll_close_cb(uv_poll_t *req);

void socket_override(int whitelist) {
	socket_override_whitelist_check = whitelist;
}

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

int socket_gc(void) {

	if(lock_init == 1) {
#ifdef _WIN32
		uv_mutex_lock(&lock);
#else
		pthread_mutex_lock(&lock);
#endif
	}

	struct client_list_t *node = NULL;
	while(client_list) {
		node = client_list->next;
		poll_close_cb(client_list->req);
		client_list = node;
	}

	if(lock_init == 1) {
#ifdef _WIN32
		uv_mutex_unlock(&lock);
#else
		pthread_mutex_unlock(&lock);
#endif
	}

	socket_started = 0;
	static_port = 0;

	logprintf(LOG_DEBUG, "garbage collected socket library");
	return EXIT_SUCCESS;
}

ssize_t socket_recv(char *buffer, int bytes, char **data, ssize_t *ptr) {
	int len = (int)strlen(EOSS);
	int end = strncmp(&buffer[bytes-len], EOSS, len);
	int i = 0, nr = 0;

	for(i=0;i<bytes;i++) {
		if(strncmp(&buffer[i], EOSS, len) == 0) {
			nr++;
		}
	}

	str_replace(EOSS, "\n", &buffer);
	bytes -= (nr*(len-1));

	if(end == 0) {
		buffer[bytes-1] = '\0';
		bytes--;
	}

	*ptr += bytes;

	if((*data = REALLOC(*data, *ptr+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	memset(&(*data)[(*ptr-bytes)], '\0', bytes+1);
	memcpy(&(*data)[(*ptr-bytes)], buffer, bytes);

	if(end == 0) {
		return *ptr;
	} else {
		return 0;
	}

	return 0;	
}

static void client_remove(int fd) {
	if(lock_init == 1) {
#ifdef _WIN32
		uv_mutex_lock(&lock);
#else
		pthread_mutex_lock(&lock);
#endif
	}

	struct client_list_t *currP, *prevP;

	prevP = NULL;

	for(currP = client_list; currP != NULL; prevP = currP, currP = currP->next) {
		if(currP->fd == fd) {
			if(prevP == NULL) {
				client_list = currP->next;
			} else {
				prevP->next = currP->next;
			}

			FREE(currP);
			break;
		}
	}

	if(lock_init == 1) {
#ifdef _WIN32
		uv_mutex_unlock(&lock);
#else
		pthread_mutex_unlock(&lock);
#endif
	}
}

static void client_add(uv_poll_t *req, int fd, struct uv_custom_poll_t *data) {
	if(lock_init == 0) {
		lock_init = 1;
#ifdef _WIN32
		uv_mutex_init(&lock);
#else
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&lock, &attr);
#endif
	}

	struct client_list_t *node = MALLOC(sizeof(struct client_list_t));
	if(node == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	node->req = req;
	node->fd = (int)fd;
	node->data = data;

#ifdef _WIN32
	uv_mutex_lock(&lock);
#else
	pthread_mutex_lock(&lock);
#endif
	node->next = client_list;
	client_list = node;
#ifdef _WIN32
	uv_mutex_unlock(&lock);
#else
	pthread_mutex_unlock(&lock);
#endif
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void client_read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct data_t *data = custom_poll_data->data;

	uv_os_fd_t fd = 0;

	int r = uv_fileno((uv_handle_t *)req, &fd);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return;
		/*LCOV_EXCL_STOP*/
	}

	if(*nread == -1) {
		/*LCOV_EXCL_START*/
		struct reason_socket_disconnected_t *data1 = MALLOC(sizeof(struct reason_socket_disconnected_t));
		if(data1 == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		data1->fd = (int)fd;
		eventpool_trigger(REASON_SOCKET_DISCONNECTED, reason_socket_disconnected_free, data1);

		uv_custom_close(req);

		return;
		/*LCOV_EXCL_STOP*/
	}
	if(*nread > 0) {
		buf[*nread] = '\0';

		data->read_cb((int)fd, buf, *nread, &data->buffer, &data->len);

		if(!uv_is_closing((uv_handle_t *)req)) {
			*nread = 0;
			uv_custom_read(req);
		}
	}
}

static void poll_close_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	char buf[INET_ADDRSTRLEN+1];
	int fd = -1, r = 0;

	if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return;
		/*LCOV_EXCL_STOP*/
	}

	if(fd > -1) {
#ifdef _WIN32
		if(getpeername((SOCKET)fd, (struct sockaddr *)&addr, &socklen) == 0) {
#else
		if(getpeername(fd, (struct sockaddr *)&addr, &socklen) == 0) {
#endif
			memset(&buf, '\0', INET_ADDRSTRLEN+1);
			uv_ip4_name((void *)&(addr.sin_addr), buf, INET_ADDRSTRLEN+1);
			logprintf(LOG_DEBUG, "client disconnected, fd %d, ip %s, port %d", fd, buf, htons(addr.sin_port));
		}

		struct reason_socket_disconnected_t *data1 = MALLOC(sizeof(struct reason_socket_disconnected_t));
		if(data1 == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		data1->fd = (int)fd;
		eventpool_trigger(REASON_SOCKET_DISCONNECTED, reason_socket_disconnected_free, data1);

#ifdef _WIN32
		shutdown(fd, SD_BOTH);
		closesocket(fd);
#else
		shutdown(fd, SHUT_RDWR);
		close(fd);
#endif
	}

	if(!uv_is_closing((uv_handle_t *)req)) {
		uv_close((uv_handle_t *)req, close_cb);
	}

	if(custom_poll_data->data != NULL) {
		FREE(custom_poll_data->data);
	}

	if(req->data != NULL) {
		uv_custom_poll_free(custom_poll_data);
		req->data = NULL;
	}

	client_remove(fd);
}

static void server_read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct sockaddr_in servaddr;
	struct uv_custom_poll_t *server_poll_data = req->data;
	struct data_t *server_data = server_poll_data->data;
	struct uv_custom_poll_t *custom_poll_data = NULL;
	socklen_t socklen = sizeof(servaddr);
	char buffer[BUFFER_SIZE];
	int client = 0, r = 0, fd = 0;

	memset(buffer, '\0', BUFFER_SIZE);

	if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return;
		/*LCOV_EXCL_STOP*/
	}

	if((client = accept(fd, (struct sockaddr *)&servaddr, (socklen_t *)&socklen)) < 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_NOTICE, "accept: %s", strerror(errno));
		return;
		/*LCOV_EXCL_STOP*/
	}

	memset(&buffer, '\0', INET_ADDRSTRLEN+1);
	uv_ip4_name((void *)&(servaddr.sin_addr), buffer, INET_ADDRSTRLEN+1);

	if(socket_override_whitelist_check > -1 || whitelist_check(buffer) != 0) {
		logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", buffer, ntohs(servaddr.sin_port));
#ifdef _WIN32
		closesocket(client);
#else
		close(client);
#endif
		return;
	} else {
		logprintf(LOG_DEBUG, "new client, fd: %d, ip: %s, port: %d", client, buffer, ntohs(servaddr.sin_port));
	}

#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(client, FIONBIO, &on);
#else
	long arg = fcntl(client, F_GETFL, NULL);
	fcntl(client, F_SETFL, arg | O_NONBLOCK);
#endif

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(data, 0, sizeof(struct data_t));
	data->read_cb = server_data->read_cb;

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_custom_poll_init(&custom_poll_data, poll_req, NULL);

	custom_poll_data->read_cb = client_read_cb;
	custom_poll_data->close_cb = poll_close_cb;
	custom_poll_data->data = data;

	r = uv_poll_init_socket(uv_default_loop(), poll_req, client);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif
		if(custom_poll_data != NULL) {
			uv_custom_poll_free(custom_poll_data);
			poll_req->data = NULL;
		}
		FREE(poll_req);
		return;
		/*LCOV_EXCL_STOP*/
	}

	client_add(poll_req, client, custom_poll_data);

	struct reason_socket_connected_t *data1 = MALLOC(sizeof(struct reason_socket_connected_t));
	if(data1 == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	data1->fd = (int)client;
	eventpool_trigger(REASON_SOCKET_CONNECTED, reason_socket_connected_free, data1);
	uv_custom_read(req);
	uv_custom_read(poll_req);
}

static void *socket_send_thread(int reason, void *param, void *userdata) {
	struct reason_socket_send_t *data = param;

	if(strcmp(data->type, "socket") == 0) {
		socket_write(data->fd, data->buffer);
	}
	return NULL;
}

/* Start the socket server */
int socket_start(unsigned short port, void (*read_cb)(int, char *, ssize_t, char **, ssize_t *)) {
	const uv_thread_t pth_cur_id = uv_thread_self();
	if(uv_thread_equal(&pth_main_id, &pth_cur_id) == 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "webserver_start can only be started from the main thread");
		return -1;
		/*LCOV_EXCL_STOP*/
	}

	if(socket_started == 1) {
		return 0;
	}

	static_port = port;
	socket_started = 1;

	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	int r = 0, sockfd = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "socket: %s", strerror(errno));
#ifdef _WIN32
		closesocket(sockfd);
#else
		close(sockfd);
#endif
		return -1;
		/*LCOV_EXCL_STOP*/
	}

	unsigned long on = 1;

#ifdef _WIN32
	ioctlsocket(sockfd, FIONBIO, &on);
#else
	long arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg | O_NONBLOCK);
#endif

	if((r = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(int))) < 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "setsockopt: %s", strerror(errno));
#ifdef _WIN32
		closesocket(sockfd);
#else
		close(sockfd);
#endif
		return -1;
		/*LCOV_EXCL_STOP*/
	}

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if((r = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
		/*LCOV_EXCL_START*/
		int x = 0;
		if((x = getpeername(sockfd, (struct sockaddr *)&addr, (socklen_t *)&socklen)) == 0) {
			logprintf(LOG_ERR, "cannot bind to socket port %d, address already in use?", ntohs(addr.sin_port));
		} else {
			logprintf(LOG_ERR, "cannot bind to socket port, address already in use?");
		}
#ifdef _WIN32
		closesocket(sockfd);
#else
		close(sockfd);
#endif
		return -1;
		/*LCOV_EXCL_STOP*/
	}

	if((listen(sockfd, 0)) < 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "listen: %s", strerror(errno));
#ifdef _WIN32
		closesocket(sockfd);
#else
		close(sockfd);
#endif
		return -1;
		/*LCOV_EXCL_STOP*/
	}

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_custom_poll_init(&custom_poll_data, poll_req, (void *)NULL);
	custom_poll_data->is_ssl = 0;
	custom_poll_data->read_cb = server_read_cb;

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(data, 0, sizeof(struct data_t));
	data->read_cb = read_cb;
	custom_poll_data->data = data;

	r = uv_poll_init_socket(uv_default_loop(), poll_req, sockfd);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		FREE(poll_req);
		goto free;
		/*LCOV_EXCL_STOP*/
	}

	uv_custom_write(poll_req);
	uv_custom_read(poll_req);

	if(getsockname(sockfd, (struct sockaddr *)&addr, (socklen_t *)&socklen) == -1) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "getsockname");
		/*LCOV_EXCL_STOP*/
	} else {
		port = static_port = ntohs(addr.sin_port);
	}

	logprintf(LOG_INFO, "socket server started at port #%d, fd: %d", port, sockfd);

	client_add(poll_req, sockfd, custom_poll_data);

	eventpool_callback(REASON_SOCKET_SEND, socket_send_thread, NULL);

	return sockfd;

free:
		/*LCOV_EXCL_START*/
	if(sockfd > 0) {
#ifdef _WIN32
		closesocket(sockfd);
#else
		close(sockfd);
#endif
	}

	return -1;
	/*LCOV_EXCL_STOP*/

	// if(socket_server != NULL) {
		// eventpool_fd_remove(socket_server);
		// socket_server = NULL;
	// }
	// socket_server = eventpool_socket_add("socket server", NULL, port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, NULL, NULL);
	// eventpool_callback(REASON_ADHOC_CONNECTED, adhoc_mode);
	// eventpool_callback(REASON_ADHOC_DISCONNECTED, socket_restart);
}

unsigned int socket_get_port(void) {
	return static_port;
}

void socket_close(int sockfd) {
	uv_poll_t *req = NULL;
	if(lock_init == 1) {
#ifdef _WIN32
		uv_mutex_lock(&lock);
#else
		pthread_mutex_lock(&lock);
#endif
	}
	struct client_list_t *tmp = client_list;
	while(tmp) {
		if(tmp->fd == sockfd && !uv_is_closing((uv_handle_t *)tmp->req)) {
			req = tmp->req;
			break;
		}
		tmp = tmp->next;
	}
	if(lock_init == 1) {
#ifdef _WIN32
		uv_mutex_unlock(&lock);
#else
		pthread_mutex_unlock(&lock);
#endif
	}

	if(req != NULL) {
		uv_custom_close(req);
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
			/*LCOV_EXCL_START*/
			logprintf(LOG_ERR, "improperly formatted string: %s", msg);
			return -1;
			/*LCOV_EXCL_STOP*/
		}
		n += (int)len;
		va_end(ap);

		if((sendBuff = MALLOC((size_t)n+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(sendBuff, '\0', (size_t)n+1);

		va_start(ap, msg);
		vsprintf(sendBuff, msg, ap);
		va_end(ap);

		memcpy(&sendBuff[n-len], EOSS, (size_t)len);

		struct uv_custom_poll_t *custom_poll_data = NULL;
		uv_poll_t *req = NULL;

		if(lock_init == 1) {
#ifdef _WIN32
			uv_mutex_lock(&lock);
#else
			pthread_mutex_lock(&lock);
#endif
		}

		struct client_list_t *tmp = client_list;
		while(tmp) {
			if(tmp->fd == sockfd && !uv_is_closing((uv_handle_t *)tmp->req)) {
				req = tmp->req;
				custom_poll_data = tmp->data;
				break;
			}
			tmp = tmp->next;
		}

		if(lock_init == 1) {
#ifdef _WIN32
			uv_mutex_unlock(&lock);
#else
			pthread_mutex_unlock(&lock);
#endif
		}

		if(req != NULL && custom_poll_data != NULL) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)sendBuff, strlen(sendBuff));
			uv_custom_write(req);
		}

		FREE(sendBuff);
	}
	return n;
}

int socket_connect(char *server, unsigned short port, void (*read_cb)(int, char *, ssize_t, char **, ssize_t *)) {
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct sockaddr_in addr;
	int r = 0, sockfd = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	r = uv_ip4_addr(server, port, &addr);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
		goto freeuv;
		/*LCOV_EXCL_STOP*/
	}

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "socket: %s", strerror(errno));
		goto freeuv;
		/*LCOV_EXCL_STOP*/
	}

#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(sockfd, FIONBIO, &on);
#else
	long arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg | O_NONBLOCK);
#endif

	if(connect(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
#ifdef _WIN32
		if(!(WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEISCONN)) {
#else
		if(!(errno == EINPROGRESS || errno == EISCONN)) {
#endif
			/*LCOV_EXCL_START*/
			logprintf(LOG_ERR, "connect: %s", strerror(errno));
			goto freeuv;
			/*LCOV_EXCL_STOP*/
		}
	}

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_custom_poll_init(&custom_poll_data, poll_req, NULL);
	custom_poll_data->is_ssl = 0;
	custom_poll_data->read_cb = client_read_cb;
	custom_poll_data->close_cb = poll_close_cb;

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(data, 0, sizeof(struct data_t));
	data->read_cb = read_cb;
	custom_poll_data->data = data;

	r = uv_poll_init_socket(uv_default_loop(), poll_req, sockfd);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		FREE(poll_req);
		goto freeuv;
		/*LCOV_EXCL_STOP*/
	}

	client_add(poll_req, sockfd, custom_poll_data);

	struct reason_socket_connected_t *data1 = MALLOC(sizeof(struct reason_socket_connected_t));
	if(data1 == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	data1->fd = (int)sockfd;
	eventpool_trigger(REASON_SOCKET_CONNECTED, reason_socket_connected_free, data1);

	uv_custom_read(poll_req);

	return sockfd;

freeuv:
	/*LCOV_EXCL_START*/
	if(sockfd > 0) {
		close(sockfd);
	}
	return 0;
	/*LCOV_EXCL_STOP*/
}
