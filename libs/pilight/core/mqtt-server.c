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

#define MIN(a,b) (((a)<(b))?(a):(b))

/*
 * FIXME:
 * - QoS 1 & 2 persistent session queue
 * - Unsubscribe
 * - Client takeover
 */

static void client_close_cb(uv_poll_t *req);

static char **mqtt_blacklist = NULL;
static char **mqtt_whitelist = NULL;
static int mqtt_blacklist_nr = 0;
static int mqtt_whitelist_nr = 0;

static uv_poll_t *server_poll_req = NULL;
static struct mqtt_client_t *mqtt_clients = NULL;

#ifdef _WIN32
	static uv_mutex_t mqtt_lock;
#else
	static pthread_mutex_t mqtt_lock;
	static pthread_mutexattr_t mqtt_attr;
#endif
static int lock_init = 0;
static int started = 0;

int mqtt_server_gc(void) {
	int i = 0;
	for(i=0;i<mqtt_blacklist_nr;i++) {
		FREE(mqtt_blacklist[i]);
	}
	if(mqtt_blacklist_nr > 0) {
		FREE(mqtt_blacklist);
	}
	mqtt_blacklist_nr = 0;
	for(i=0;i<mqtt_whitelist_nr;i++) {
		FREE(mqtt_whitelist[i]);
	}
	if(mqtt_whitelist_nr > 0) {
		FREE(mqtt_whitelist);
	}
	mqtt_whitelist_nr = 0;

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

	if(server_poll_req != NULL) {
		client_close_cb(server_poll_req);
		server_poll_req = NULL;
	}
#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif

	started = 0;
	return 0;
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
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

static void client_close_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *client = custom_poll_data->data;
	char buffer[BUFFER_SIZE] = { 0 };

	if(client != NULL) {
		if(!uv_is_closing((uv_handle_t *)client->async_req)) {
			uv_close((uv_handle_t *)client->async_req, close_cb);
		}

		struct mqtt_subscription_t *subscription = NULL;
		while(client->subscriptions) {
			subscription = client->subscriptions;
			FREE(client->subscriptions->topic);
			client->subscriptions = client->subscriptions->next;
			FREE(subscription);
		}

		struct mqtt_message_t *message = NULL;
		while(client->retain) {
			message = client->retain;
			FREE(client->retain->topic);
			FREE(client->retain->payload);
			client->retain = client->retain->next;
			FREE(message);
		}

		iobuf_free(&client->send_iobuf);
		iobuf_free(&client->recv_iobuf);

		uv_timer_stop(client->timeout_req);
		uv_close((uv_handle_t *)client->timeout_req, close_cb);

		{
			char *topicfmt = "pilight/mqtt/%s", *topic = NULL;
			int len = snprintf(NULL, 0, topicfmt, client->id);
			if((topic = MALLOC(len+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			snprintf(topic, len+1, topicfmt, client->id);

#ifdef _WIN32
			uv_mutex_lock(&mqtt_lock);
#else
			pthread_mutex_lock(&mqtt_lock);
#endif
			int ret = 0;
			struct mqtt_client_t *clients = mqtt_clients;
			while(clients) {
				if(clients != client) {
					struct mqtt_subscription_t *subscription = clients->subscriptions;
					while(subscription) {
						if(client->haswill == 1) {
							if(mosquitto_topic_matches_sub(subscription->topic, client->will.topic, &ret) == 0) {
								if(ret == 1) {
									int x = 0, pass = 1;
									for(x=0;x<mqtt_blacklist_nr;x++) {
										if(mosquitto_topic_matches_sub(mqtt_blacklist[x], client->will.topic, &ret) == 0) {
											if(ret == 1) {
												pass = 0;
												break;
											}
										}
									}
									if(pass == 0) {
										for(x=0;x<mqtt_whitelist_nr;x++) {
											if(mosquitto_topic_matches_sub(mqtt_whitelist[x], client->will.topic, &ret) == 0) {
												if(ret == 1) {
													pass = 1;
													break;
												}
											}
										}
									}
									if(pass == 1) {
										mqtt_publish(clients, 0, subscription->qos, 0, client->will.topic, client->will.message);
									}
								}
							}
						}

						if(mosquitto_topic_matches_sub(subscription->topic, topic, &ret) == 0) {
							if(ret == 1) {
								int x = 0, pass = 1;
								for(x=0;x<mqtt_blacklist_nr;x++) {
									if(mosquitto_topic_matches_sub(mqtt_blacklist[x], topic, &ret) == 0) {
										if(ret == 1) {
											pass = 0;
											break;
										}
									}
								}
								if(pass == 0) {
									for(x=0;x<mqtt_whitelist_nr;x++) {
										if(mosquitto_topic_matches_sub(mqtt_whitelist[x], topic, &ret) == 0) {
											if(ret == 1) {
												pass = 1;
												break;
											}
										}
									}
								}
								if(pass == 1) {
									mqtt_publish(clients, 0, subscription->qos, 0, topic, "disconnected");
								}
							}
						}
					subscription = subscription->next;
					}
				}
				clients = clients->next;
			}
#ifdef _WIN32
			uv_mutex_unlock(&mqtt_lock);
#else
			pthread_mutex_unlock(&mqtt_lock);
#endif
			FREE(topic);
		}

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

		if(client->id != NULL) {
			FREE(client->id);
		}

		if(client->haswill == 1) {
			client->haswill = 0;
			FREE(client->will.topic);
			FREE(client->will.message);
		}
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

static void timeout(uv_timer_t *handle) {
	struct mqtt_client_t *client = handle->data;

	uv_timer_stop(client->timeout_req);

	uv_custom_close(client->poll_req);
}

static void client_write_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *client = custom_poll_data->data;

	if(client != NULL) {
		iobuf_append_remove(&client->send_iobuf, &custom_poll_data->send_iobuf);
	}

	if(custom_poll_data->send_iobuf.len > 0) {
		uv_custom_write(req);
	}

	uv_custom_read(req);
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

static ssize_t client_read_cb(uv_poll_t *req, ssize_t nread, const char *buf) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *client = custom_poll_data->data;
	struct iobuf_t *iobuf = NULL;

	struct mqtt_pkt_t *pkt = NULL;
	unsigned int pos = 0, len = 0, disconnect = 0;
	unsigned char *buf1 = NULL;
	int ret = 0;

	if(nread > 0) {
		iobuf = &client->recv_iobuf;
		iobuf_append(iobuf, buf, nread);

#ifdef PILIGHT_UNITTEST
		uv_timer_start(client->timeout_req, timeout, (client->keepalive*1.5)*100, 0);
#else
		uv_timer_start(client->timeout_req, timeout, (client->keepalive*1.5)*1000, 0);
#endif

		while(iobuf->len > 0 && (ret = mqtt_decode(&pkt, (unsigned char *)iobuf->buf, iobuf->len, &pos)) == 0) {
			iobuf_remove(iobuf, pos);
			pos = 0;

			// printf("-------------------\nserver\n");
			// mqtt_dump(pkt);
			// printf("\n");

			struct mqtt_pkt_t pkt1;
			memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

			switch(pkt->type) {
				case MQTT_CONNECT: {
					if(pkt->header.connect.cleansession == 0) {
						mqtt_pkt_connack(&pkt1, MQTT_CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION);
					} else {
						mqtt_pkt_connack(&pkt1, MQTT_CONNECTION_ACCEPTED);
						client->step = MQTT_CONNECTED;
						if((client->id = STRDUP(pkt->payload.connect.clientid)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						client->keepalive = pkt->payload.connect.keepalive;

						if(pkt->header.connect.willflag == 1) {
							client->haswill = 1;
							client->will.qos = pkt->header.connect.qos;
							client->will.retain = pkt->header.connect.willretain;
							if((client->will.topic = STRDUP(pkt->payload.connect.willtopic)) == NULL) {
								OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
							}
							if((client->will.message = STRDUP(pkt->payload.connect.willmessage)) == NULL) {
								OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
							}
						}

						char *topicfmt = "pilight/mqtt/%s", *topic = NULL;
						int len = snprintf(NULL, 0, topicfmt, client->id);
						if((topic = MALLOC(len+1)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						snprintf(topic, len+1, topicfmt, client->id);

#ifdef _WIN32
						uv_mutex_lock(&mqtt_lock);
#else
						pthread_mutex_lock(&mqtt_lock);
#endif
						struct mqtt_client_t *clients = mqtt_clients;
						while(clients) {
							if(clients != client) {
								struct mqtt_subscription_t *subscription = clients->subscriptions;
								while(subscription) {
									if(mosquitto_topic_matches_sub(subscription->topic, topic, &ret) == 0) {
										if(ret == 1) {
											mqtt_publish(clients, 0, subscription->qos, 0, topic, "connected");
										}
									}
									subscription = subscription->next;
								}
							}
							clients = clients->next;
						}
						FREE(topic);
#ifdef _WIN32
						uv_mutex_unlock(&mqtt_lock);
#else
						pthread_mutex_unlock(&mqtt_lock);
#endif
					}

					if(mqtt_encode(&pkt1, &buf1, &len) == 0) {
						iobuf_append(&custom_poll_data->send_iobuf, (void *)buf1, len);
						FREE(buf1);
					}
				} break;
				case MQTT_PUBLISH: {
#ifdef _WIN32
					uv_mutex_lock(&mqtt_lock);
#else
					pthread_mutex_lock(&mqtt_lock);
#endif
					if(pkt->retain == 1) {
						struct mqtt_message_t *message = client->retain;
						while(message) {
							if(strcmp(message->topic, pkt->payload.publish.topic) == 0) {
								FREE(message->payload);
								FREE(message->topic);
								break;
							}
							message = message->next;
						}

						if(message == NULL) {
							if((message = MALLOC(sizeof(struct mqtt_message_t))) == NULL) {
								OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
							}
							memset(message, 0, sizeof(struct mqtt_message_t));

							message->next = client->retain;
							client->retain = message;
						}

						if((message->payload = STRDUP(pkt->payload.publish.message)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						if((message->topic = STRDUP(pkt->payload.publish.topic)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						message->client = client;
					}

					struct mqtt_client_t *clients = mqtt_clients;
					while(clients) {
						struct mqtt_subscription_t *subscription = clients->subscriptions;
						while(subscription) {
							if(mosquitto_topic_matches_sub(subscription->topic, pkt->payload.publish.topic, &ret) == 0) {
								if(ret == 1) {
									int x = 0, pass = 1;
									for(x=0;x<mqtt_blacklist_nr;x++) {
										if(mosquitto_topic_matches_sub(mqtt_blacklist[x], pkt->payload.publish.topic, &ret) == 0) {
											if(ret == 1) {
												pass = 0;
												break;
											}
										}
									}
									if(pass == 0) {
										for(x=0;x<mqtt_whitelist_nr;x++) {
											if(mosquitto_topic_matches_sub(mqtt_whitelist[x], pkt->payload.publish.topic, &ret) == 0) {
												if(ret == 1) {
													pass = 1;
													break;
												}
											}
										}
									}
									if(pass == 1) {
										mqtt_publish(clients, 0, subscription->qos, 0, pkt->payload.publish.topic, pkt->payload.publish.message);
									}
								}
							}
							subscription = subscription->next;
						}
						clients = clients->next;
					}
#ifdef _WIN32
					uv_mutex_unlock(&mqtt_lock);
#else
					pthread_mutex_unlock(&mqtt_lock);
#endif
					if(pkt->qos == 1) {
						mqtt_puback(client, pkt->payload.publish.msgid);
					}
					if(pkt->qos == 2) {
						mqtt_pubrec(client, pkt->payload.publish.msgid);
					}
				} break;
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
				case MQTT_PUBREL: {
					mqtt_pubcomp(client, pkt->payload.pubrel.msgid);
				} break;
				case MQTT_SUBSCRIBE: {
					mqtt_suback(client, pkt->payload.subscribe.msgid, pkt->payload.subscribe.qos);
#ifdef _WIN32
					uv_mutex_lock(&mqtt_lock);
#else
					pthread_mutex_lock(&mqtt_lock);
#endif
					struct mqtt_subscription_t *subscription = client->subscriptions;
					while(subscription) {
						if(strcmp(subscription->topic, pkt->payload.subscribe.topic) == 0) {
							break;
						}
						subscription = subscription->next;
					}

					if(subscription == NULL) {
						if((subscription = MALLOC(sizeof(struct mqtt_subscription_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						memset(subscription, 0, sizeof(struct mqtt_subscription_t));

						subscription->next = client->subscriptions;
						client->subscriptions = subscription;
					}

					if((subscription->topic = STRDUP(pkt->payload.subscribe.topic)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					subscription->qos = pkt->payload.subscribe.qos;

					struct mqtt_client_t *clients = mqtt_clients;
					while(clients) {
						if(clients != client) {
							struct mqtt_message_t *message = clients->retain;
							while(message) {
								if(mosquitto_topic_matches_sub(pkt->payload.subscribe.topic, message->topic, &ret) == 0) {
									if(ret == 1) {
										int x = 0, pass = 1;
										for(x=0;x<mqtt_blacklist_nr;x++) {
											if(mosquitto_topic_matches_sub(mqtt_blacklist[x], message->topic, &ret) == 0) {
												if(ret == 1) {
													pass = 0;
													break;
												}
											}
										}
										if(pass == 0) {
											for(x=0;x<mqtt_whitelist_nr;x++) {
												if(mosquitto_topic_matches_sub(mqtt_whitelist[x], message->topic, &ret) == 0) {
													if(ret == 1) {
														pass = 1;
														break;
													}
												}
											}
										}
										if(pass == 1) {
											mqtt_publish(client, 0, pkt->payload.subscribe.qos, 0, message->topic, message->payload);
										}
									}
								}
								message = message->next;
							}
						}
						clients = clients->next;
					}
#ifdef _WIN32
					uv_mutex_unlock(&mqtt_lock);
#else
					pthread_mutex_unlock(&mqtt_lock);
#endif
				} break;
				case MQTT_PINGREQ: {
					mqtt_pingresp(client);
				} break;
				case MQTT_PINGRESP: {
				} break;
				case MQTT_DISCONNECT: {
					disconnect = 1;
					uv_custom_close(req);
				} break;
			}
			mqtt_free(pkt);
			FREE(pkt);
		}
	}

	if(ret == -1) {
		if(!uv_is_closing((uv_handle_t *)client->async_req)) {
			uv_close((uv_handle_t *)client->async_req, close_cb);
		}
		uv_custom_close(req);
	} else if(disconnect == 0) {
		uv_custom_write(req);
		uv_custom_read(req);
	}

	return nread;
}

static ssize_t server_read_cb(uv_poll_t *req, ssize_t nread, const char *buf) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct sockaddr_in servaddr;
	socklen_t socklen = sizeof(servaddr);
	char buffer[BUFFER_SIZE];
	int clientfd = 0, r = 0, fd = 0;

	memset(buffer, '\0', BUFFER_SIZE);

	if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		return 0;
		/*LCOV_EXCL_STOP*/
	}

	if((clientfd = accept(fd, (struct sockaddr *)&servaddr, (socklen_t *)&socklen)) < 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_NOTICE, "accept: %s", strerror(errno));
		return 0;
		/*LCOV_EXCL_STOP*/
	}

	memset(&buffer, '\0', INET_ADDRSTRLEN+1);
	uv_inet_ntop(AF_INET, (void *)&(servaddr.sin_addr), buffer, INET_ADDRSTRLEN+1);

	// if(whitelist_check(buffer) != 0) {
		// logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", buffer, ntohs(servaddr.sin_port));
// #ifdef _WIN32
		// closesocket(client);
// #else
		// close(client);
// #endif
		// return;
	// } else {
		logprintf(LOG_DEBUG, "new client, ip: %s, port: %d", buffer, ntohs(servaddr.sin_port));
		logprintf(LOG_DEBUG, "client fd: %d", clientfd);
	// }

#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(clientfd, FIONBIO, &on);
#else
	long arg = fcntl(clientfd, F_GETFL, NULL);
	fcntl(clientfd, F_SETFL, arg | O_NONBLOCK);
#endif

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	struct mqtt_client_t *client = NULL;

	if((client = MALLOC(sizeof(struct mqtt_client_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(client, 0, sizeof(struct mqtt_client_t));

	client->poll_req = poll_req;

	client->step = MQTT_CONNECT;
	client->fd = clientfd;
#ifdef PILIGHT_UNITTEST
	client->keepalive = 1;
#else
	client->keepalive = 60;
#endif

	int i = 0;
	for(i=0;i<1024;i++) {
		memset(&client->messages[i], 0, sizeof(struct mqtt_message_t));
	}

	if((client->timeout_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	client->timeout_req->data = client;

	iobuf_init(&client->recv_iobuf, 0);
	iobuf_init(&client->send_iobuf, 0);

	uv_timer_init(uv_default_loop(), client->timeout_req);
#ifdef PILIGHT_UNITTEST
	uv_timer_start(client->timeout_req, timeout, (client->keepalive*1.5)*100, 0);
#else
	uv_timer_start(client->timeout_req, timeout, (client->keepalive*1.5)*1000, 0);
#endif

	if((client->async_req = MALLOC(sizeof(uv_async_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	client->async_req->data = client;
	uv_async_init(uv_default_loop(), client->async_req, do_write);

	uv_custom_poll_init(&custom_poll_data, poll_req, client);

	custom_poll_data->read_cb = client_read_cb;
	custom_poll_data->write_cb = client_write_cb;
	custom_poll_data->close_cb = client_close_cb;

	r = uv_poll_init_socket(uv_default_loop(), poll_req, clientfd);
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
		return 0;
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

	uv_custom_read(req);
	uv_custom_read(poll_req);

	return 0;
}

#ifdef MQTT
int mqtt_server(int port) {
	if(started == 1) {
		return 0;
	}
	started = 1;

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

	if((listen(sockfd, TCP_BACKLOG)) < 0) {
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

	if((server_poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_custom_poll_init(&custom_poll_data, server_poll_req, (void *)NULL);
	custom_poll_data->is_ssl = 0;
	custom_poll_data->is_server = 1;
	custom_poll_data->read_cb = server_read_cb;

	r = uv_poll_init_socket(uv_default_loop(), server_poll_req, sockfd);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		FREE(server_poll_req);
		goto free;
		/*LCOV_EXCL_STOP*/
	}

	logprintf(LOG_DEBUG, "started MQTT broker on port %d", port);

	struct lua_state_t *state = plua_get_free_state();
	if(state != NULL) {
		char *out = NULL;
		while(config_setting_get_string(state->L, "mqtt-blacklist", mqtt_blacklist_nr, &out) == 0) {
			if((mqtt_blacklist = REALLOC(mqtt_blacklist, (mqtt_blacklist_nr+1)*sizeof(char *))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			mqtt_blacklist[mqtt_blacklist_nr] = out;
			mqtt_blacklist_nr++;
		}

		out = NULL;
		while(config_setting_get_string(state->L, "mqtt-whitelist", mqtt_whitelist_nr, &out) == 0) {
			if((mqtt_whitelist = REALLOC(mqtt_whitelist, (mqtt_whitelist_nr+1)*sizeof(char *))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			mqtt_whitelist[mqtt_whitelist_nr] = out;
			mqtt_whitelist_nr++;
		}
		plua_clear_state(state);
	}

	uv_custom_read(server_poll_req);

	return 0;

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
}
#endif
