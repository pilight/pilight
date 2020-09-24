/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/config/settings.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/libuv/uv.h"
#include "mqtt.h"
#include "network.h"

#include "../../libuv/uv.h"

static void client_close_cb(uv_poll_t *req);

static int terminated = 0;

static struct mqtt_client_t *mqtt_clients = NULL;
#ifdef _WIN32
	static uv_mutex_t mqtt_lock;
#else
	static pthread_mutex_t mqtt_lock;
	static pthread_mutexattr_t mqtt_attr;
#endif
static int lock_init = 0;

int mqtt_client_gc(void) {
	terminated = 1;
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	{
		struct mqtt_client_t *node = NULL;
		while(mqtt_clients) {
			node = mqtt_clients->next;
			client_close_cb(mqtt_clients->poll_req);
			mqtt_clients = node;
		}
	}
#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
	return 0;
}

static void mqtt_client_remove(uv_poll_t *req) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	struct mqtt_client_t *currP, *prevP;

	prevP = NULL;

	for(currP = mqtt_clients; currP != NULL; prevP = currP, currP = currP->next) {
		if(currP->poll_req == req) {
			if(prevP == NULL) {
				mqtt_clients = currP->next;
			} else {
				prevP->next = currP->next;
			}

			FREE(currP);
			break;
		}
	}
#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void client_close_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *client = custom_poll_data->data;
	char buffer[BUFFER_SIZE] = { 0 };

	if(!uv_is_closing((uv_handle_t *)client->async_req)) {
		uv_close((uv_handle_t *)client->async_req, close_cb);
	}

	struct mqtt_pkt_t pkt;
	pkt.type = MQTT_DISCONNECTED;

	if(terminated == 0) {
		if(custom_poll_data->started == 1) {
			client->step = MQTT_DISCONNECTED;
			client->callback(client, &pkt, client->userdata);
		} else {
			client->callback(NULL, &pkt, client->userdata);
		}
	}

	FREE(client->id);

	int i = 0;
	for(i=0;i<1024;i++) {
		struct mqtt_message_t *message = &client->messages[i];
		if(message->step > 0) {
			uv_timer_stop(message->timer_req);
			uv_close((uv_handle_t *)message->timer_req, close_cb);
			FREE(message->topic);
			FREE(message->payload);
			memset(message, 0, sizeof(struct mqtt_message_t));
		}
	}

	if(client->haswill == 1) {
		client->haswill = 0;
		FREE(client->will.topic);
		FREE(client->will.message);
	}

	iobuf_free(&client->send_iobuf);
	iobuf_free(&client->recv_iobuf);

	if(client->ping_req != NULL) {
		uv_timer_stop(client->ping_req);
		uv_close((uv_handle_t *)client->ping_req, close_cb);
	}

	if(client->timeout_req != NULL) {
		uv_timer_stop(client->timeout_req);
		uv_close((uv_handle_t *)client->timeout_req, close_cb);
	}

	int fd = -1, r = 0;

	if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		/*LCOV_EXCL_STOP*/
	}

	if(fd > -1) {
#ifdef _WIN32
		shutdown(fd, SD_BOTH);
		closesocket(fd);
#else
		shutdown(fd, SHUT_WR);
		while(recv(fd, buffer, BUFFER_SIZE, 0) > 0);
		close(fd);
#endif
	}

	if(!uv_is_closing((uv_handle_t *)req)) {
		uv_close((uv_handle_t *)req, close_cb);
	}

	if(req->data != NULL) {
		uv_custom_poll_free(custom_poll_data);
		req->data = NULL;
	}

	mqtt_client_remove(req);
}

static void do_write(uv_async_t *handle) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct mqtt_client_t *client = handle->data;
	uv_custom_write(client->poll_req);
}

static void ping(uv_timer_t *handle) {
	mqtt_ping(handle->data);
}

static void timeout(uv_timer_t *handle) {
	struct mqtt_client_t *client = handle->data;

	uv_timer_stop(client->timeout_req);

	uv_custom_close(client->poll_req);
}

static ssize_t read_cb(uv_poll_t *req, ssize_t nread, const char *buf) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *client = custom_poll_data->data;
	struct iobuf_t *iobuf = NULL;

	struct mqtt_pkt_t *pkt = NULL;
	unsigned int pos = 0;
	int ret = 0;

	if(nread > 0) {
		iobuf = &client->recv_iobuf;
		iobuf_append(iobuf, buf, nread);

		while(iobuf->len > 0 && (ret = mqtt_decode(&pkt, (unsigned char *)iobuf->buf, iobuf->len, &pos)) == 0) {
			iobuf_remove(iobuf, pos);
			pos = 0;

			// printf("-------------------\nclient\n");
			// mqtt_dump(pkt);
			// printf("\n");

			uv_timer_start(client->timeout_req, (void (*)(uv_timer_t *))timeout, 9000, 9000);

			switch(pkt->type) {
				case MQTT_PUBCOMP:
				case MQTT_PUBACK: {
					struct mqtt_message_t *message = &client->messages[pkt->payload.puback.msgid];
					if(message->step == MQTT_PUBACK || message->step == MQTT_PUBCOMP) {
						uv_timer_stop(message->timer_req);
						uv_close((uv_handle_t *)message->timer_req, close_cb);
						FREE(message->topic);
						FREE(message->payload);
						memset(message, 0, sizeof(struct mqtt_message_t));
					}
				} break;
				case MQTT_PUBREC: {
					struct mqtt_message_t *message = &client->messages[pkt->payload.puback.msgid];
					if(message->step == MQTT_PUBREC) {
						message->step = MQTT_PUBCOMP;
						mqtt_pubrel(client, message->msgid);
					}
				} break;
				case MQTT_CONNACK: {
					if(client->step == MQTT_CONNACK) {

						if((client->ping_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						client->ping_req->data = client;
						uv_timer_init(uv_default_loop(), client->ping_req);
#ifdef PILIGHT_UNITTEST
						uv_timer_start(client->ping_req, (void (*)(uv_timer_t *))ping, 100, 100);
#else
						uv_timer_start(client->ping_req, (void (*)(uv_timer_t *))ping, 3000, 3000);
#endif

						client->step = MQTT_CONNECTED;
						client->callback(client, pkt, client->userdata);
					} else {
						// error
					}
				} break;
				case MQTT_PUBREL: {
					mqtt_pubcomp(client, pkt->payload.pubrel.msgid);
				} break;
				case MQTT_PUBLISH: {
					if(pkt->qos == 1) {
						mqtt_puback(client, pkt->payload.publish.msgid);
					}
					if(pkt->qos == 2) {
						mqtt_pubrec(client, pkt->payload.publish.msgid);
					}
					client->callback(client, pkt, client->userdata);
				} break;
				case MQTT_PINGREQ: {
					mqtt_pingresp(client);
				} break;
				case MQTT_PINGRESP: {
				} break;
				default: {
					client->callback(client, pkt, client->userdata);
				} break;
			}

			mqtt_free(pkt);
			FREE(pkt);
		}
	}

	uv_custom_write(req);
	uv_custom_read(req);

	return nread;
}

static void write_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *client = custom_poll_data->data;

	if(client != NULL) {
		if(client->step == MQTT_CONNECT) {
			struct mqtt_pkt_t pkt;
			unsigned char *buf = NULL;
			unsigned int len = 0;

			memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

#ifdef PILIGHT_UNITTEST
			mqtt_pkt_connect(&pkt, 0, client->will.qos, 0, client->id, "MQIsdp", 3, NULL, NULL, client->will.retain, (client->will.topic != NULL && client->will.message != NULL), 1, 1, client->will.topic, client->will.message);
#else
			mqtt_pkt_connect(&pkt, 0, client->will.qos, 0, client->id, "MQIsdp", 3, NULL, NULL, client->will.retain, (client->will.topic != NULL && client->will.message != NULL), 1, 60, client->will.topic, client->will.message);
#endif
			if(mqtt_encode(&pkt, &buf, &len) == 0) {
				iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
				FREE(buf);
			}
			mqtt_free(&pkt);
			client->step = MQTT_CONNACK;
		}

		iobuf_append_remove(&client->send_iobuf, &custom_poll_data->send_iobuf);
	}

	if(custom_poll_data->send_iobuf.len > 0) {
		uv_custom_write(req);
	}

	uv_custom_read(req);
}

#ifdef PILIGHT_UNITTEST
void mqtt_activate(void) {
	terminated = 0;
}
#endif

static void thread(uv_work_t *req) {
	struct mqtt_client_t *client = req->data;
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct sockaddr_in addr4;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	memset(&addr4, '\0', sizeof(struct sockaddr_in));
	r = uv_ip4_addr(client->ip, client->port, &addr4);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
		goto free;
		/*LCOV_EXCL_END*/
	}

	if((client->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "socket: %s", strerror(errno));
		goto free;
		/*LCOV_EXCL_STOP*/
	}

#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(client->fd, FIONBIO, &on);
#else
	long arg = fcntl(client->fd, F_GETFL, NULL);
	fcntl(client->fd, F_SETFL, arg | O_NONBLOCK);
#endif

	if(connect(client->fd, (struct sockaddr *)&addr4, sizeof(addr4)) < 0) {
#ifdef _WIN32
		if(!(WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEISCONN)) {
#else
		if(!(errno == EINPROGRESS || errno == EISCONN)) {
#endif
			/*LCOV_EXCL_START*/
			logprintf(LOG_ERR, "connect: %s", strerror(errno));
			goto free;
			/*LCOV_EXCL_STOP*/
		}
	}

	iobuf_init(&client->recv_iobuf, 0);
	iobuf_init(&client->send_iobuf, 0);

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_start(client->timeout_req, (void (*)(uv_timer_t *))timeout, 9000, 9000);

	client->poll_req = poll_req;

	uv_custom_poll_init(&custom_poll_data, client->poll_req, client);

	custom_poll_data->is_ssl = 0;
	custom_poll_data->write_cb = write_cb;
	custom_poll_data->read_cb = read_cb;
	custom_poll_data->close_cb = client_close_cb;

	r = uv_poll_init_socket(uv_default_loop(), client->poll_req, client->fd);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		FREE(client->poll_req);
		goto free;
		/*LCOV_EXCL_STOP*/
	}

#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	client->next = mqtt_clients;
	mqtt_clients = client;
#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif

	uv_custom_write(client->poll_req);

	FREE(client->ip);

	return;

/*LCOV_EXCL_START*/
free:
	if(custom_poll_data != NULL) {
		uv_custom_poll_free(custom_poll_data);
	}
	if(client->fd > 0) {
#ifdef _WIN32
		closesocket(client->fd);
#else
		close(client->fd);
#endif
	}
	FREE(client->ip);
	return;
/*LCOV_EXCL_STOP*/
}

static void thread_free(uv_work_t *req, int status) {
	FREE(req);
}

int mqtt_client(char *ip, int port, char *clientid, char *willtopic, char *willmsg, void (*callback)(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata), void *userdata) {
	if(terminated == 1) {
		return -1;
	} else if(willtopic != NULL && willmsg == NULL) {
		return -1;
	} else if(willmsg != NULL && willtopic == NULL) {
		return -1;
	}
	if(ip == NULL && port < 0 && callback == NULL) {
		return -1;
	}

	if(lock_init == 0) {
		lock_init = 1;
#ifdef _WIN32
		uv_mutex_init(&mqtt_lock);
#else
		pthread_mutexattr_init(&mqtt_attr);
		pthread_mutexattr_settype(&mqtt_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mqtt_lock, &mqtt_attr);
#endif
	}

	struct mqtt_client_t *client = NULL;

	if((client = MALLOC(sizeof(struct mqtt_client_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(client, 0, sizeof(struct mqtt_client_t));

	client->poll_req = NULL;

#ifdef PILIGHT_UNITTEST
	client->keepalive = 1;
#else
	client->keepalive = 60;
#endif

	client->step = MQTT_CONNECT;
	client->callback = callback;
	client->userdata = userdata;
	client->fd = -1;
	client->port = port;

	if((client->ip = STRDUP(ip)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	if((client->id = STRDUP(clientid)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	if(willtopic != NULL && willmsg != NULL) {
		if((client->will.message = STRDUP(willmsg)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		if((client->will.topic = STRDUP(willtopic)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		client->will.retain = 0;
		client->will.qos = 0;
		client->haswill = 1;
	}

	int i = 0;
	for(i=0;i<1024;i++) {
		memset(&client->messages[i], 0, sizeof(struct mqtt_message_t));
	}

	if((client->async_req = MALLOC(sizeof(uv_async_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	client->async_req->data = client;
	uv_async_init(uv_default_loop(), client->async_req, do_write);

	if((client->timeout_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	client->timeout_req->data = client;
	uv_timer_init(uv_default_loop(), client->timeout_req);

	uv_work_t *work_req = NULL;
	if((work_req = MALLOC(sizeof(uv_work_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	work_req->data = client;
	uv_queue_work_s(work_req, "mqtt", 1, thread, thread_free);

	return 0;
}