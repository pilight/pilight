/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#ifdef _WIN32
	#if _WIN32_WINNT < 0x0501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif
#include <stdint.h>
#include <time.h>
#include <signal.h>

#include "../../libuv/uv.h"
#include "network.h"
#include "socket.h"
#include "pilight.h"
#include "threadpool.h"
#include "eventpool.h"
#include "common.h"
#include "log.h"
#include "../storage/storage.h"

typedef struct l_fp {
	union {
		unsigned int Xl_ui;
		int Xl_i;
	} Ul_i;
	union {
		unsigned int Xl_uf;
		int Xl_f;
	} Ul_f;
} l_fp;

typedef struct pkt {
	int	li_vn_mode;
	int rootdelay;
	int rootdispersion;
	int refid;
	struct l_fp ref;
	struct l_fp org;
	struct l_fp rec;
	/* Make sure the pkg is 48 bits */
	double tmp;
} pkt;

typedef struct data_t {
	void (*callback)(int, time_t);
	uv_udp_t *stream;
	uv_timer_t *timer;
} data_t;

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void ntp_timeout(uv_timer_t *param) {
	struct data_t *data = param->data;
	void (*callback)(int, time_t) = data->callback;
	callback(-1, 0);

	if(!uv_is_closing((uv_handle_t *)param)) {
		uv_close((uv_handle_t *)param, close_cb);
	}
	if(!uv_is_closing((uv_handle_t *)data->stream)) {
		uv_close((uv_handle_t *)data->stream, close_cb);
	}
	if(data != NULL) {
		FREE(data);
	}
}

static void on_read(uv_udp_t *stream, ssize_t len, const uv_buf_t *buf, const struct sockaddr *addr, unsigned int port) {
	struct pkt msg;	
	struct data_t *data = stream->data;
	void (*callback)(int, time_t) = data->callback;

	if(len == 48) {
		memcpy(&msg, buf->base, 48);

		if(msg.refid > 0) {
			(msg.rec).Ul_i.Xl_ui = ntohl((msg.rec).Ul_i.Xl_ui);
			(msg.rec).Ul_f.Xl_f = (int)ntohl((unsigned int)(msg.rec).Ul_f.Xl_f);

			unsigned int adj = 2208988800u;
			callback(0, (time_t)(msg.rec.Ul_i.Xl_ui - adj));

			if(!uv_is_closing((uv_handle_t *)stream)) {
				uv_close((uv_handle_t *)stream, close_cb);
			}
			if(!uv_is_closing((uv_handle_t *)data->timer)) {
				uv_close((uv_handle_t *)data->timer, close_cb);
			}
			if(data != NULL) {
				FREE(data);
			}
		}
		free(buf->base);
	}
}

static void alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	buf->base = malloc(len);
	memset(buf->base, 0, len);
}

static void on_send(uv_udp_send_t *req, int status) {
	FREE(req);
}

int ntpsync(char *server, void (*callback)(int, time_t)) {
	struct sockaddr_in addr;
	struct data_t *data = NULL;
	uv_udp_t *client_req = NULL;
	uv_udp_send_t *send_req = NULL;
	uv_timer_t *timeout_req = NULL;
	char ip[INET_ADDRSTRLEN+1], *p = ip;
	char buffer[BUFFER_SIZE];
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	r = host2ip(server, p);
	if(r != 0) {
		logprintf(LOG_ERR, "host2ip");
		return -1;
	}

	r = uv_ip4_addr(ip, 123, &addr);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
		return -1;
	}
	if((client_req = MALLOC(sizeof(uv_udp_t))) == NULL) {
		OUT_OF_MEMORY
	}

	r = uv_udp_init(uv_default_loop(), client_req);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_udp_init: %s", uv_strerror(r));
		FREE(client_req);
		return -1;
	}

	if((send_req = MALLOC(sizeof(uv_udp_send_t))) == NULL) {
		OUT_OF_MEMORY
	}

	if((timeout_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY
	}

	if((data = MALLOC(sizeof(struct data_t))) == NULL) {
		OUT_OF_MEMORY
	}

	memset(data, 0, sizeof(struct data_t));
	data->callback = callback;
	data->stream = client_req;
	data->timer = timeout_req;

	struct pkt msg;
	memset(&msg, '\0', sizeof(struct pkt));
	msg.li_vn_mode = 227;

	memcpy(&buffer, &msg, sizeof(struct pkt));
	uv_buf_t buf = uv_buf_init(buffer, sizeof(struct pkt));

	client_req->data = data;

	r = uv_udp_send(send_req, client_req, &buf, 1, (const struct sockaddr *)&addr, on_send);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_udp_send: %s", uv_strerror(r));
		FREE(send_req);
		FREE(client_req);
		return -1;
	}

	timeout_req->data = data;

	uv_timer_init(uv_default_loop(), timeout_req);
	uv_timer_start(timeout_req, ntp_timeout, 1000, 0);

	r = uv_udp_recv_start(client_req, alloc, on_read);
	if(r != 0) {
		logprintf(LOG_ERR, "uv_udp_recv_start: %s", uv_strerror(r));
		FREE(send_req);
		FREE(client_req);
		return -1;
	}
	logprintf(LOG_DEBUG, "syncing with ntp-server %s", server);

	return 0;
}
