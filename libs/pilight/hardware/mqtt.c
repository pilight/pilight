/*
	Copyright (C) 2019 CurlyMo & Niek

 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <libgen.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <pthread.h>

#ifdef _WIN32
	#define STRICT
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <termios.h>
#endif

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
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <poll.h>
	#include <arpa/inet.h>
	#include <unistd.h>
#endif

#include "../core/pilight.h"
#include "../core/json.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/dso.h"
#include "../config/hardware.h"
#include "../config/config.h"

#include <mbedtls/ssl.h>
#include <mbedtls/error.h>
#include "../core/eventpool.h"
#include "../../libuv/uv.h"
#include "../core/socket.h"
#include "../core/network.h"
#include "../core/webserver.h"
#include "../core/threads.h"
#include "../core/ssl.h"
#include "mqtt.h"

#define PROTOCOL_NAME "MQTT" 
#define PROTOCOL_LEVEL 4	// version 3.1.1
#define CLEAN_SESSION (1U<<1)

#define UNSUPPORTED	0
#define AUTHPLAIN	1

#define MIN_DELAY		1000
#define MAX_DELAY		10000
#define LISTENER_CLIENTID	"mqtt_listener"

#define Connection_Accepted 0

/*
Connection results:

Connection_Accepted 0
Connection_Refused_unacceptable_protocol_version 1
Connection_Refused_identifier_rejected 2
Connection_Refused_server_unavailable 3
Connection_Refused_bad_username_or_password 4
Connection_Refused_not_authorized 5
*/

#ifdef _WIN32
	static uv_mutex_t mqtt_lock;
#else
	static pthread_mutex_t mqtt_lock;
	static pthread_mutexattr_t mqtt_attr;
#endif

static char *default_host = NULL, *default_user = NULL, *default_pass = NULL, *listener_topic = NULL;
static int default_port = 0, initialized = 0, listener_delay = MIN_DELAY;
static uv_timer_t *listener_timer_req = NULL;


static int mqtt_lock_init = 0;

struct mqtt_clients_t *mqtt_clients = NULL;

static void listener_callback(int status, char *message, char *topic, void *userdata);
static void mqtt_client_close(uv_poll_t *req);
static void timeout(uv_timer_t *req);

static unsigned char lsb(unsigned short val) {
	return (unsigned char)(val & 0x00ff);
}
static unsigned char msb(unsigned short val) {
	return (unsigned char)((val & 0xff00) >> 8) ;
}

static unsigned char get_packet_type(unsigned short val) {
	return (unsigned char)val >> 4;
}

static unsigned char set_packet_type(unsigned short val) {
	return (unsigned char)val << 4;
}

static void free_mqtt_client(struct mqtt_client_t *mqtt_client) {
	if (mqtt_client != NULL) {
		if(mqtt_client->packet != NULL) {
			FREE(mqtt_client->packet);
		}
		if(mqtt_client->host != NULL) {
			FREE(mqtt_client->host);
		}
		if(mqtt_client->user != NULL) {
			FREE(mqtt_client->user);
		}
		if(mqtt_client->pass != NULL) {
			FREE(mqtt_client->pass);
		}
		if(mqtt_client->clientid != NULL) {
			FREE(mqtt_client->clientid);
		}
		if(mqtt_client->message != NULL) {
			FREE(mqtt_client->message);
		}
		FREE(mqtt_client);
	}
}

static void mqtt_client_add(uv_poll_t *req, struct uv_custom_poll_t *data) {

#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	int fd = -1, r = 0;

	if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));/*LCOV_EXCL_LINE*/
	}

	struct mqtt_clients_t *node = MALLOC(sizeof(struct mqtt_clients_t));
	if(node == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	node->req = req;
	node->data = data;
	node->fd = fd;

	node->next = mqtt_clients;
	mqtt_clients = node;
#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

static void mqtt_client_remove(uv_poll_t *req) {

#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *mqtt_client = custom_poll_data->data;
	logprintf(LOG_DEBUG, "mqtt: removing client \"%s\"", mqtt_client->clientid);

	struct mqtt_clients_t *currP, *prevP;
	prevP = NULL;

	for(currP = mqtt_clients;currP != NULL;prevP = currP, currP = currP->next) {
		if(currP->req == req) {
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
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));
	FREE(handle);
}

static void mqtt_client_close(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *mqtt_client = custom_poll_data->data;
	if (mqtt_client != NULL && mqtt_client->timer_req != NULL) {
		uv_timer_stop(mqtt_client->timer_req);
	}

	if(mqtt_client->fd > -1) {
#ifdef _WIN32
		shutdown(mqtt_client->fd, SD_BOTH);
		closesocket(mqtt_client->fd);
#else
		shutdown(mqtt_client->fd, SHUT_RDWR);
		close(mqtt_client->fd);
#endif
	}
	
	if(mqtt_client->keepalive == 0 || mqtt_client->status == -1) {
		logprintf(LOG_NOTICE, "mqtt: connection failed for client \"%s\"", mqtt_client->clientid);
	}

	mqtt_client_remove(req);

	if(!uv_is_closing((uv_handle_t *)req)) {
		uv_poll_stop(req);
		uv_close((uv_handle_t *)req, close_cb);
	}

	if(mqtt_client != NULL) {
		free_mqtt_client(mqtt_client);
		custom_poll_data->data = NULL;
	}

	if(custom_poll_data != NULL) {
		uv_custom_poll_free(custom_poll_data);
		req->data = NULL;
	}
}

static void poll_close_cb(uv_poll_t *req) {

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *mqtt_client = custom_poll_data->data;
	if(mqtt_client->timer_req != NULL) {
		uv_timer_stop(mqtt_client->timer_req);
	}
	uv_poll_stop(mqtt_client->poll_req);
	mqtt_client_close(req);
}

static void timeout(uv_timer_t *req) {
	struct mqtt_client_t *mqtt_client = req->data;
	logprintf(LOG_NOTICE, "mqtt: timeout for clientid \"%s\"", mqtt_client->clientid);
	if(mqtt_client->timer_req != NULL) {
		uv_timer_stop(mqtt_client->timer_req);
	}
	uv_poll_stop(mqtt_client->poll_req);
	mqtt_client_close(mqtt_client->poll_req);
}

static void push_data(uv_poll_t *req, int step) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *mqtt_client = custom_poll_data->data;

	iobuf_append(&custom_poll_data->send_iobuf, (void *)mqtt_client->packet, mqtt_client->packet_len);

	if(mqtt_client->packet != NULL) {
		FREE(mqtt_client->packet);
	}
	mqtt_client->packet_len = 0;
	mqtt_client->step = step;
	mqtt_client->reading = 1;

	uv_custom_write(req);
	uv_custom_read(req);
}

static void read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *mqtt_client = custom_poll_data->data;

	switch(mqtt_client->step) {

		case MQTT_STEP_RECV_CONNACK: {
			if( (get_packet_type(buf[0]) == CONNACK) && ((sizeof(buf) -2) == buf[1]) && (buf[3] == Connection_Accepted) ) {
				logprintf(LOG_DEBUG, "mqtt: client \"%s\" connected", mqtt_client->clientid);
				mqtt_client->connected = 1;
				mqtt_client->status = 0;
				*nread = 0;
				
				switch (mqtt_client->reqtype) {
					case PUBLISH: {
						mqtt_client->step = MQTT_STEP_SEND_PUBLISH;	
						uv_custom_write(req);
					} break;
					case SUBSCRIBE: {
						mqtt_client->step = MQTT_STEP_SEND_SUBSCRIBE;
						uv_custom_write(req);
					} break;
					default: {
						logprintf(LOG_NOTICE, "MQTT Invalid request type %i, %i or %i required", mqtt_client->reqtype, PUBLISH, SUBSCRIBE);
						mqtt_client->status = -1;
						if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
							mqtt_client->called = 1;
							mqtt_client->callback(-1, "Invalid request type", mqtt_client->topic, mqtt_client->userdata);
						}
					}
				}

			} else {
				logprintf(LOG_NOTICE, "failed to connect to MQTT broker with error: %d", buf[3]);
				mqtt_client->status = -1;
				if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
					mqtt_client->called = 1;
					mqtt_client->callback(-1, "Failed to connect to broker", mqtt_client->topic, mqtt_client->userdata);
				}
			}
		} break;

		case MQTT_STEP_RECV_SUBACK: {
			if((get_packet_type(buf[0]) == SUBACK) && (buf[1]) != 128 &&(buf[2] == msb(mqtt_client->sent_packetid)) && (buf[3] == lsb(mqtt_client->sent_packetid)) ) {

				if (mqtt_client->timer_req != NULL) {
						uv_timer_stop(mqtt_client->timer_req);
				}
				
				char **topics = NULL;
				int n = explode(mqtt_client->topic, ";", &topics);
				for(int i=0;i<n;i++) {
					logprintf(LOG_DEBUG, "mqtt: client \"%s\" subscribed to topic \"%s\"", mqtt_client->clientid, topics[i]);
				}
				mqtt_client->status = 0;
				mqtt_client->subscribed = 1;
				*nread = 0;

				if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
					mqtt_client->called = 1;
					mqtt_client->callback(0, "Subscription succeeded", mqtt_client->topic, mqtt_client->userdata);
				}
			}
			else {
				logprintf(LOG_NOTICE, "mqtt: subscription for clientïd \"%s\" failed with code %i", mqtt_client->clientid, buf[1]);
				mqtt_client->status = -1;
				if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
					mqtt_client->called = 1;
					mqtt_client->callback(-1, "Subscription failed", mqtt_client->topic, mqtt_client->userdata);
				}

			}
		mqtt_client->step = MQTT_STEP_RECV_PUBLISH;	

		 } break;

 		case MQTT_STEP_RECV_PUBLISH: {
			if( get_packet_type(buf[0]) == PUBLISH) {
				int qos = buf[0] >> 1 & 3;
				unsigned int topiclen = (buf[2]<<8) + buf[3];
				if (topiclen > strlen(mqtt_client->topic)) {
					if((mqtt_client->topic = REALLOC(mqtt_client->topic, topiclen + 1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
				}
				memset(mqtt_client->topic, 0, topiclen + 1);
				memcpy(mqtt_client->topic, buf + 4, topiclen);

				unsigned long msgpos = 4 + topiclen;

				if (qos > 0) {//if qos is 1 or 2, get the packet id
					unsigned short received_packetid = (buf[4 + topiclen] << 8) + buf[4 + topiclen+1];
					mqtt_client->recvd_packetid = received_packetid;
					msgpos += 2;
				}

				unsigned int msglen = buf[1] + 2 - msgpos;

				if(mqtt_client->message == NULL || msglen > strlen(mqtt_client->message)) {
					if((mqtt_client->message = REALLOC(mqtt_client->message, msglen + 1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
				}

				memset(mqtt_client->message, 0, msglen + 1);
				memcpy(mqtt_client->message, buf + msgpos, msglen);
				logprintf(LOG_DEBUG, "mqtt: received message \"%s\" with topic \"%s\" with qos%i", mqtt_client->message, mqtt_client->topic, qos);

				if(qos > 0) {
					*nread = 0;
					mqtt_client->step = MQTT_STEP_SEND_PUBACK;
					uv_custom_write(req);
				} else {
					//callback unless message has zero length
					if(mqtt_client->message != NULL && strlen(mqtt_client->message) > 0) {
						if(mqtt_client->callback != NULL) {
							mqtt_client->callback(1, mqtt_client->message, mqtt_client->topic, mqtt_client->userdata);
						}
						//publish zero length message to remove latest retained non zero message
						unsigned char fixed_header[2];
						unsigned char topic[strlen(mqtt_client->topic) + 2];
						topic[0] = 0;
						topic[1] = lsb((unsigned short)strlen(mqtt_client->topic));
						memcpy((char *)&topic[2], mqtt_client->topic, strlen(mqtt_client->topic));
						fixed_header[0] = set_packet_type(PUBLISH)| 1;//retained
						fixed_header[1] = sizeof(topic);
						int packet_len = 2 + sizeof(topic);
						if((mqtt_client->packet = REALLOC(mqtt_client->packet, packet_len)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						memset(mqtt_client->packet, 0, packet_len);
						memcpy(mqtt_client->packet, fixed_header, sizeof(fixed_header));
						memcpy(mqtt_client->packet + sizeof(fixed_header), topic, sizeof(topic));
						mqtt_client->packet_len = packet_len;

						push_data(req, MQTT_STEP_RECV_PUBLISH);
					}
					*nread = 0;
				}
			} else {
				logprintf(LOG_NOTICE, "mqtt: unexpected packet type %i", get_packet_type(buf[0]));
				if(mqtt_client->callback != NULL) {
					mqtt_client->callback(-1, "Unexpected packet type", mqtt_client->topic, mqtt_client->userdata);
				}
			}
		} break;
		
		case MQTT_STEP_RECV_PUBACK: {
			if((get_packet_type(buf[0]) == PUBACK) && (buf[1]) == 2 && (buf[2] == msb(mqtt_client->sent_packetid) && (buf[3] == lsb(mqtt_client->sent_packetid))) ) {
				if (mqtt_client->timer_req != NULL) {
						uv_timer_stop(mqtt_client->timer_req);
				}
			
				logprintf(LOG_DEBUG, "mqtt: published message \"%s\" for topic \"%s\" with qoS%i", mqtt_client->message, mqtt_client->topic, mqtt_client->qos);
				mqtt_client->status = 0;
				if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
					mqtt_client->called = 1;
					mqtt_client->callback(0, "Published with qos1", NULL, mqtt_client->userdata);
				}
				*nread = 0;
			}
			else {
				logprintf(LOG_NOTICE, "mqtt: failed to publish message \"%s\" for topic \"%s\" with qoS%i", mqtt_client->message, mqtt_client->topic, mqtt_client->qos);
				mqtt_client->status = -1;
				if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
					mqtt_client->called = 1;
					mqtt_client->callback(-1, "Publish failed", NULL, mqtt_client->userdata);
				}
			}
		} break;
		
	}
}

static void write_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *mqtt_client = custom_poll_data->data;
	unsigned char fixed_header[2];

	switch(mqtt_client->step) {

		case MQTT_STEP_SEND_CONNECT: {
			logprintf(LOG_DEBUG, "mqtt: client \"%s\" trying to connect", mqtt_client->clientid);
			unsigned char var_header[10];

			int have_credentials = 0;

			var_header[0] = 0;
			var_header[1] = strlen(PROTOCOL_NAME);
			memcpy(&var_header[2], PROTOCOL_NAME, strlen(PROTOCOL_NAME));
			var_header[6] = PROTOCOL_LEVEL;
			var_header[7] = CLEAN_SESSION;
			var_header[8] = msb(mqtt_client->keepalive);
			var_header[9] = lsb(mqtt_client->keepalive);	

			int payload_size = strlen(mqtt_client->clientid) + 2;

			if(mqtt_client->user != NULL && mqtt_client->pass != NULL) {
				have_credentials = 1;
				var_header[7] += 192; //set user and password flags
				payload_size += strlen(mqtt_client->user) + strlen(mqtt_client->pass) + 4;
			}
			unsigned char payload[payload_size];
			payload[0] = 0;
			payload[1] = strlen(mqtt_client->clientid);
			memcpy(&payload[2],mqtt_client->clientid, strlen(mqtt_client->clientid));

			if (have_credentials) {
				payload[strlen(mqtt_client->clientid) + 2] = 0;
				payload[strlen(mqtt_client->clientid) + 3] = strlen(mqtt_client->user);
				memcpy(&payload[strlen(mqtt_client->clientid) + 4], mqtt_client->user, strlen(mqtt_client->user));

				payload[strlen(mqtt_client->clientid) + strlen(mqtt_client->user) + 4] = 0;
				payload[strlen(mqtt_client->clientid) + strlen(mqtt_client->user) + 5] = strlen(mqtt_client->user);
				memcpy(&payload[strlen(mqtt_client->clientid) + strlen(mqtt_client->user) + 6], mqtt_client->pass, strlen(mqtt_client->pass));
			}

			fixed_header[0] = set_packet_type(CONNECT);
			fixed_header[1] = sizeof(var_header) + sizeof(payload);

			int packet_len = 2 + sizeof(var_header) + sizeof(payload);
			if((mqtt_client->packet = REALLOC(mqtt_client->packet, packet_len)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			memset(mqtt_client->packet, 0, packet_len);
			memcpy(mqtt_client->packet, fixed_header, sizeof(fixed_header));
			memcpy(mqtt_client->packet + sizeof(fixed_header),var_header,sizeof(var_header));
			memcpy(mqtt_client->packet + sizeof(fixed_header)+sizeof(var_header),payload,sizeof(payload));

			// send Connect message
			mqtt_client->packet_len = packet_len;
			push_data(req, MQTT_STEP_RECV_CONNACK);
		} break;

		case MQTT_STEP_SEND_SUBSCRIBE: {
			unsigned char var_header[2];
			mqtt_client->sent_packetid++;
			var_header[0] = msb(mqtt_client->sent_packetid);
			var_header[1] = lsb(mqtt_client->sent_packetid);
			char **topics = NULL;
			int n = explode(mqtt_client->topic, ";", &topics);
			int topics_size = strlen(mqtt_client->topic) + (n * 3) - (n - 1);
			int packet_len = 4 + topics_size;
			mqtt_client->packet_len = packet_len;

			if((mqtt_client->packet = REALLOC(mqtt_client->packet, packet_len)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

 			memset(mqtt_client->packet, 0, packet_len);
			memcpy(mqtt_client->packet + 2, var_header, 2);
			fixed_header[0] = set_packet_type(SUBSCRIBE) + 2;
			fixed_header[1] = 2 + topics_size;
			memcpy(mqtt_client->packet, fixed_header, 2);
			int topic_offset = 4;
			for(int i=0;i<n;i++) {
				logprintf(LOG_DEBUG, "mqtt: client \"%s\" adding topic: \"%s\"", mqtt_client->clientid, topics[i]);
				unsigned char topic[strlen(topics[i]) + 3];
				topic[0] = 0;
				topic[1] = lsb(strlen(topics[i]));
				memcpy((char *)&topic[2], topics[i], strlen(topics[i]));
				topic[sizeof(topic)-1] = mqtt_client->qos;
				memcpy(mqtt_client->packet + topic_offset, topic, sizeof(topic));
				topic_offset += sizeof(topic);
			}

			push_data(req, MQTT_STEP_RECV_SUBACK);

		} break;

		case MQTT_STEP_SEND_PUBACK: {
			unsigned char var_header[2];
			var_header[0] = msb(mqtt_client->recvd_packetid);
			var_header[1] = lsb(mqtt_client->recvd_packetid);

			fixed_header[0] = set_packet_type(PUBACK);
			fixed_header[1] = sizeof(var_header);

			int packet_len = sizeof(fixed_header) + sizeof(var_header);
			if((mqtt_client->packet = REALLOC(mqtt_client->packet, packet_len)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			memset(mqtt_client->packet, 0, packet_len);

			memcpy(mqtt_client->packet, fixed_header, 2);
			memcpy(mqtt_client->packet + 2, var_header, 2);
			mqtt_client->packet_len = packet_len;

			push_data(req, MQTT_STEP_RECV_PUBLISH);

			//callback unless message has zero length
			if(mqtt_client->callback != NULL && mqtt_client->message != NULL && strlen(mqtt_client->message) > 0) {
				mqtt_client->callback(1, mqtt_client->message, mqtt_client->topic, mqtt_client->userdata);
			}
			return;

		} break;
		
		case MQTT_STEP_SEND_PUBLISH: {
			if(mqtt_client->qos == 0) {
				unsigned char topic[strlen(mqtt_client->topic) + 2];
				topic[0] = 0;
				topic[1] = lsb((unsigned short)strlen(mqtt_client->topic));
				memcpy((char *)&topic[2], mqtt_client->topic, strlen(mqtt_client->topic));
					
				fixed_header[0] = set_packet_type(PUBLISH);//not retained
				fixed_header[1] = sizeof(topic) + strlen(mqtt_client->message);

				int packet_len = 2 + sizeof(topic) + strlen(mqtt_client->message);
				if((mqtt_client->packet = REALLOC(mqtt_client->packet, packet_len)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}

				memset(mqtt_client->packet, 0, packet_len);
				memcpy(mqtt_client->packet, fixed_header, sizeof(fixed_header));
				memcpy(mqtt_client->packet + sizeof(fixed_header), topic, sizeof(topic));
				memcpy(mqtt_client->packet + sizeof(fixed_header) + sizeof(topic), mqtt_client->message, strlen(mqtt_client->message));
				mqtt_client->packet_len = packet_len;

				push_data(req, MQTT_STEP_END);
				if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
					mqtt_client->called = 1;
					mqtt_client->callback(0, "Published with qos 0", mqtt_client->topic, mqtt_client->userdata);
				}


			} else {
				unsigned char topic[strlen(mqtt_client->topic) + 4];// 2 extra for message size for packet ID
				topic[0] = 0;
				topic[1] = lsb(strlen(mqtt_client->topic));
				memcpy((char *)&topic[2], mqtt_client->topic, strlen(mqtt_client->topic));
				
				mqtt_client->sent_packetid++;

				topic[sizeof(topic) -2] = msb(mqtt_client->sent_packetid);
				topic[sizeof(topic) -1] = lsb(mqtt_client->sent_packetid);
				
				fixed_header[0] = set_packet_type(PUBLISH)|(mqtt_client->qos << 1)| 1;//retained
				fixed_header[1] = sizeof(topic) + strlen(mqtt_client->message);
				int packet_len = 2 + sizeof(topic) + strlen(mqtt_client->message);
				
				if((mqtt_client->packet = REALLOC(mqtt_client->packet, packet_len)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				
				memset(mqtt_client->packet, 0, packet_len);
				memcpy(mqtt_client->packet, fixed_header, sizeof(fixed_header));
				memcpy(mqtt_client->packet + sizeof(fixed_header), topic, sizeof(topic));
				memcpy(mqtt_client->packet + sizeof(fixed_header) + sizeof(topic), mqtt_client->message, strlen(mqtt_client->message));
				mqtt_client->packet_len = packet_len;

				push_data(req, MQTT_STEP_RECV_PUBACK);
			}
		} break;

		case MQTT_STEP_END: {
			return;
		}
	}
}

void mqtt_process(int reqtype, char *clientid, char *host, int port, char *user, char *pass, char *topic, int qos, char *message, int keepalive, void (*callback)(int, char *, char *, void *userdata), void *userdata){

	struct mqtt_clients_t *clients = mqtt_clients;
	struct mqtt_client_t *mqtt_client = MALLOC (sizeof(mqtt_client_t));
	struct mqtt_client_t *client = MALLOC (sizeof(mqtt_client_t));

	if(!mqtt_client) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(mqtt_client, 0, sizeof(struct mqtt_client_t));

#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif

	//check if not already registered

	int registered = 0;
	while(clients) {
		client = clients->data->data;
		 if(strcmp(client->clientid, clientid) == 0) { // registered
			 registered = 1;
			 break;
		 }
		clients = clients->next;
	}

	#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
	if(initialized == 0) {
		logprintf(LOG_ERR, "mqtt: not initialized");
		exit(EXIT_FAILURE);
		if(callback != NULL) {
			callback(-3, "hardware module not initialized", mqtt_client->topic, userdata);
		}
		return;
	}

	if(registered == 1 && reqtype == SUBSCRIBE) { 
		if(callback != NULL) {
			callback(-1, "already registered", mqtt_client->topic, userdata);
		}
		return;
		
	} else {
		if((mqtt_client->clientid = MALLOC(strlen(clientid)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(mqtt_client->clientid, clientid);

		if(host != NULL && strlen(host) > 0) {
			if((mqtt_client->host = MALLOC(strlen(host) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(mqtt_client->host, host);

		} else if (default_host != NULL && strlen(default_host) > 0) {
			if((mqtt_client->host = MALLOC(strlen(default_host) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(mqtt_client->host, default_host);
		}

		mqtt_client->user = NULL;
		if(user != NULL && strlen(user) > 0) {
			if((mqtt_client->user = MALLOC(strlen(user)+ 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
		strcpy(mqtt_client->user, user);

		} else if (default_user != NULL && strlen(default_user) > 0) {
			if((mqtt_client->user = MALLOC(strlen(default_user) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(mqtt_client->user, default_user);
		}

		mqtt_client->pass = NULL;
		if(pass != NULL && strlen(pass) > 0) {
			if((mqtt_client->pass = MALLOC(strlen(pass)+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(mqtt_client->pass, pass);

		} else if (default_pass != NULL && strlen(default_pass) > 0) {
			if((mqtt_client->pass = MALLOC(strlen(default_pass) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
		strcpy(mqtt_client->pass, default_pass);
		}

		if((mqtt_client->user == NULL && mqtt_client->pass != NULL)|| (mqtt_client->user != NULL && mqtt_client->pass == NULL)) {
			logprintf(LOG_NOTICE, "mqtt: user and password must either both be defined or both be left blank");
			exit(EXIT_FAILURE);
		}

		if(port >0) {
			mqtt_client->port = (int)round(port);	
		} else if(default_port > 0) {
			mqtt_client->port = (int)round(default_port);
		} else {
			logprintf(LOG_DEBUG, "mqtt: port is not specified in configuration");
			exit(EXIT_FAILURE);
		}

		if((mqtt_client->topic = MALLOC(strlen(topic)+1)) == NULL){
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(mqtt_client->topic, topic);

		if(message != NULL) {
			if((mqtt_client->message = MALLOC(strlen(message)+1)) == NULL){
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(mqtt_client->message, message);
		} 

		mqtt_client->qos = qos; //determines qos for publish and maximum qos for received publish
		mqtt_client->keepalive = keepalive;

		if(port == 8883) {
			if(ssl_client_init_status() == -1) {
				logprintf(LOG_ERR, "mqtt_send ssl requires a properly initialized SSL library");
			exit(EXIT_FAILURE);
			}
			mqtt_client->is_ssl = 1;
		} else {
			mqtt_client->is_ssl = 0;
		}
		mqtt_client->status = -1;
		mqtt_client->authtype = UNSUPPORTED;
		mqtt_client->reqtype = -1;
		mqtt_client->userdata = userdata;
		mqtt_client->callback = callback;
		mqtt_client->connected = 0;
		mqtt_client->subscribed = 0;
		mqtt_client->called = 0;

		struct uv_custom_poll_t *custom_poll_data = NULL;
		struct sockaddr_in addr4;
		struct sockaddr_in6 addr6;
		char *ip = NULL;
		int r = 0;

			if(mqtt_lock_init == 0) {
				mqtt_lock_init = 1;
		#ifdef _WIN32
				uv_mutex_init(&mqtt_lock);
		#else
				pthread_mutexattr_init(&mqtt_attr);
				pthread_mutexattr_settype(&mqtt_attr, PTHREAD_MUTEX_RECURSIVE);
				pthread_mutex_init(&mqtt_lock, &mqtt_attr);
		#endif
			}

		#ifdef _WIN32
			WSADATA wsa;

			if(WSAStartup(0x202, &wsa) != 0) {
				logprintf(LOG_ERR, "WSAStartup");
				exit(EXIT_FAILURE);
			}
		#endif

			memset(&addr4, 0, sizeof(addr4));
			memset(&addr6, 0, sizeof(addr6));
			int inet = host2ip(mqtt_client->host, &ip);
			switch(inet) {
				case AF_INET: {
					memset(&addr4, '\0', sizeof(struct sockaddr_in));
					r = uv_ip4_addr(ip, mqtt_client->port, &addr4);
					if(r != 0) {
						/*LCOV_EXCL_START*/
						logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
						goto freeuv;
						/*LCOV_EXCL_END*/
					}
				} break;
				case AF_INET6: {
					memset(&addr6, '\0', sizeof(struct sockaddr_in6));
					r = uv_ip6_addr(ip, mqtt_client->port, &addr6);
					if(r != 0) {
						/*LCOV_EXCL_START*/
						logprintf(LOG_ERR, "uv_ip6_addr: %s", uv_strerror(r));
						goto freeuv;
						/*LCOV_EXCL_END*/
					}
				} break;
				default: {
					/*LCOV_EXCL_START*/
					logprintf(LOG_ERR, "host2ip");
					goto freeuv;
					/*LCOV_EXCL_END*/
				} break;
			}
			FREE(ip);

			/*
			 * Partly bypass libuv in case of ssl connections
			 */
			if((mqtt_client->fd = socket(inet, SOCK_STREAM, 0)) < 0){
				/*LCOV_EXCL_START*/
				logprintf(LOG_ERR, "socket: %s", strerror(errno));
				goto freeuv;
				/*LCOV_EXCL_STOP*/
			}

		#ifdef _WIN32
			unsigned long on = 1;
			ioctlsocket(mqtt_client->fd, FIONBIO, &on);
		#else
			long arg = fcntl(mqtt_client->fd, F_GETFL, NULL);
			fcntl(mqtt_client->fd, F_SETFL, arg | O_NONBLOCK);
		#endif

			switch(inet) {
				case AF_INET: {
					r = connect(mqtt_client->fd, (struct sockaddr *)&addr4, sizeof(addr4));
				} break;
				case AF_INET6: {
					r = connect(mqtt_client->fd, (struct sockaddr *)&addr6, sizeof(addr6));
				} break;
				default: {
				} break;
			}

			if(r < 0) {
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
		logprintf(LOG_DEBUG, "mqtt: client \"%s\" fd: %i", mqtt_client->clientid, mqtt_client->fd);

		mqtt_client->poll_req = NULL;
		mqtt_client->timer_req = NULL;
		if((mqtt_client->poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		if((mqtt_client->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		uv_custom_poll_init(&custom_poll_data, mqtt_client->poll_req, (void *)mqtt_client);
		custom_poll_data->is_ssl = mqtt_client->is_ssl;
		custom_poll_data->write_cb = write_cb;
		custom_poll_data->read_cb = read_cb;
		custom_poll_data->close_cb = poll_close_cb;
		if((custom_poll_data->host = STRDUP(mqtt_client->host)) == NULL) {
			OUT_OF_MEMORY
		}
		mqtt_client->update_pending = 0;
		mqtt_client->timer_req->data = mqtt_client;

		r = uv_poll_init_socket(uv_default_loop(), mqtt_client->poll_req, mqtt_client->fd);
		if(r != 0) {
			/*LCOV_EXCL_START*/
			logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
			FREE(mqtt_client->poll_req);
			goto freeuv;
			/*LCOV_EXCL_STOP*/
		}

		uv_timer_init(uv_default_loop(), mqtt_client->timer_req);
		uv_timer_start(mqtt_client->timer_req, (void (*)(uv_timer_t *))timeout, 3000, 0);
		mqtt_client_add(mqtt_client->poll_req, custom_poll_data);

		mqtt_client->reqtype = reqtype;
		mqtt_client->step = MQTT_STEP_SEND_CONNECT;
		uv_custom_write(mqtt_client->poll_req);

		return;

	freeuv:
		if(mqtt_client->poll_req != NULL) {
			FREE(mqtt_client->poll_req);
		}
		if(mqtt_client->callback != NULL && mqtt_client->called == 0) {
			mqtt_client->called = 1;
			mqtt_client->callback(-2, "Socket connection failed", mqtt_client->topic, userdata);
		}
		FREE(mqtt_client->host);
		FREE(mqtt_client->user);
		FREE(mqtt_client->pass);
		FREE(mqtt_client->clientid);
		FREE(mqtt_client->topic);

		if(mqtt_client->fd > 0) {
	#ifdef _WIN32
			closesocket(mqtt_client->fd);
	#else
			close(mqtt_client->fd);
	#endif
		}
		FREE(mqtt_client);
		return;
	}
}

//The listener is a passive function. It is activated via a callback if a message is received with a matching topic.
//If the connection would fail, the client is notified also via a callback 

static void listener_wait(uv_timer_t *req) {
	if(listener_timer_req != NULL) {
		uv_timer_stop(listener_timer_req);
	}
	mqtt_sub(LISTENER_CLIENTID, default_host, default_port, default_user, default_pass, listener_topic, 1, 0, listener_callback, NULL);
}

static void listener_callback(int status, char *message, char *topic, void *userdata) {
	switch(status) {
		case 0: { //connect/subscribe succes
			if(listener_timer_req != NULL) {
				uv_timer_stop(listener_timer_req);
				listener_delay = MIN_DELAY;
			}
		} break;

		case 1: { // message received
			listener_delay = MIN_DELAY;
			char *sep = "^"; //separator for multi value label- and weather messages
			logprintf(LOG_DEBUG, "mqtt_listener: received message \"%s\" with topic \"%s\" for \"%s\"", message, topic, LISTENER_CLIENTID);
			char **array = NULL;
			int n = explode(topic, """/""", &array); //array[n-1] is devicename, array[n-2] is action 
			struct devices_t *devices = NULL;
			if(devices_get(array[n - 1], &devices) == 0) {
				struct protocols_t *protocols = devices->protocols;
				struct protocol_t *protocol = NULL;
				while(protocols) {
					protocol = protocols->listener;
					devtype_t devtype = protocol->devtype;
					if(pilight.control != NULL) {
						struct JsonNode *jvalues = json_mkobject();

						logprintf(LOG_DEBUG, "mqtt_listener: trying to %s %s device \"%s\" to \"%s\"", array[n - 2], protocol->id, array[n - 1], message);

						switch(devtype) {
							case SWITCH: {
								if(strcmp(array[n-2], "switch") == 0) {
									if(strcmp(message, "on") == 0 || strcmp(message, "off") == 0) {
										pilight.control(devices, message, NULL, PROTOCOL);
										break;
									} else {
										logprintf(LOG_DEBUG, "mqtt_listener: cannot %s device \"%s\" to \"%s\"", array[n - 2], array[n - 1], message);
										break;
									}
								} else {
									logprintf(LOG_DEBUG, "mqtt_listener: action \"%s\" invalid for device \"%s\"", array[n - 2], array[n - 1]);
								}
							} break;

							case DIMMER: {
								if(strcmp(array[n-2], "switch") == 0) {
									if(strcmp(message, "on") == 0 || strcmp(message, "off") == 0) {
										pilight.control(devices, message, NULL, PROTOCOL);
									} else {
										logprintf(LOG_DEBUG, "mqtt_listener: cannot %s device \"%s\" to \"%s\"", array[n - 2], array[n - 1], message);
									}
									break;
								} else if(strcmp(array[n-2], "dim") == 0) {
									if(isNumeric(message) == 0 && atoi(message) >= 0) {
										json_append_member(jvalues, "dimlevel", json_mknumber(atoi(message), 0));
										pilight.control(devices, NULL, json_first_child(jvalues), PROTOCOL);
										break;
									} else {
										logprintf(LOG_DEBUG, "mqtt_listener: cannot %s device \"%s\" to \"%s\"", array[n - 2], array[n - 1], message);
									}
								} else {
									logprintf(LOG_DEBUG, "mqtt_listener: command %s invalid for device \"%s\"", array[n - 2], array[n - 1]);
								}
							} break;

							case SCREEN: {
								if(strcmp(array[n-2], "switch") == 0) {
									if(strcmp(message, "up") == 0 || strcmp(message, "down") == 0) {
										pilight.control(devices, message, NULL, PROTOCOL);
									} else {
										logprintf(LOG_DEBUG, "mqtt_listener: cannot %s device \"%s\" to \"%s\"", array[n - 2], array[n - 1], message);
									}
									break;
								} else {
									logprintf(LOG_DEBUG, "mqtt_listener: command %s invalid for device \"%s\"", array[n - 2], array[n - 1]);
								}
							} break;

							case LABEL: {
								if(strcmp(array[n-2], "label") == 0) {
									char *stmp = strdup(message);
									char *label = strsep(&stmp, sep);
									char *color = strsep(&stmp, sep);
									char *bgcolor = strsep(&stmp, sep);
									char *blink = strsep(&stmp, sep);

									if(label != NULL && strlen(label) > 0) {
										json_append_member(jvalues, "label", json_mkstring(label));
										logprintf(LOG_DEBUG, "mqtt_listener: trying to set label to \"%s\"", label);
									}

									if(color != NULL && strlen(color) > 0) {
										json_append_member(jvalues, "color", json_mkstring(color));
										logprintf(LOG_DEBUG, "mqtt_listener: trying to set color to \"%s\"", color);
									}

									if(bgcolor != NULL && strlen(bgcolor) > 0) {
										json_append_member(jvalues, "bgcolor", json_mkstring(bgcolor));
										logprintf(LOG_DEBUG, "mqtt_listener: trying to set bgcolor to \"%s\"", bgcolor);
									}

									if(blink != NULL && strlen(blink) > 0) {
										json_append_member(jvalues, "blink", json_mkstring(blink));
										logprintf(LOG_DEBUG, "mqtt_listener: trying to set blink to \"%s\"", blink);
									}

									pilight.control(devices, NULL, json_first_child(jvalues), PROTOCOL);

								} else {
									logprintf(LOG_DEBUG, "mqtt_listener: cannot %s device \"%s\" to \"%s\"", array[n - 2], array[n - 1], message);
									break;
								}
							} break;

							case WEATHER: {
								if(strcmp(array[n-2], "weather") == 0) {
									char *stmp = strdup(message);
									char *temperature = strsep(&stmp, sep);
									char *humidity = strsep(&stmp, sep);
									char *battery = strsep(&stmp, sep);
									if(temperature != NULL && strlen(temperature) > 0) {
										if(isNumeric(temperature) == 0) {
											json_append_member(jvalues, "temperature", json_mknumber(atof(temperature), 2));
										} else {
											logprintf(LOG_DEBUG, "mqtt_listener: cannot set temperature of device \"%s\" to \"%s\"", array[n - 1], temperature);
										}
									}

									if(humidity != NULL && strlen(humidity) > 0) {
										if(isNumeric(humidity) == 0 && atof(humidity) > 0 && atof(humidity) < 100) {
											json_append_member(jvalues, "humidity", json_mknumber(atof(humidity), 2));
										} else {
											logprintf(LOG_DEBUG, "mqtt_listener: cannot set humidity of device \"%s\" to \"%s\"", array[n - 1], humidity);
										}
									}

									if(battery != NULL && strlen(battery) > 0) {
										if(isNumeric(battery) == 0 && (atoi(battery) == 0 || atoi(battery) == 1)) {
											json_append_member(jvalues, "battery", json_mknumber(atoi(battery), 0));
										} else {
											logprintf(LOG_DEBUG, "mqtt_listener: cannot set battery of device \"%s\" to \"%s\"", array[n - 1], battery);
										}
									}
									
									pilight.control(devices, NULL, json_first_child(jvalues), PROTOCOL);

								} else {
									logprintf(LOG_DEBUG, "mqtt_listener: cannot %s device \"%s\" to \"%s\"", array[n - 2], array[n - 1], message);
									break;
								}
							} break;

							default: {
								logprintf(LOG_DEBUG, "mqtt_listener: device type \"%i\" is not supported", devtype);
							}
						}
					}
					protocols = protocols->next;
				}

			} else {
				logprintf(LOG_DEBUG, "mqtt_listener: device \"%s\" doesn't exist in config", array[n - 1]);
			}

		} break;

		case -3: { //hardware module not ready
			logprintf(LOG_NOTICE, "mqtt_listener: client \"%s\" tried to register, but it seems that mqtt module was not configured", LISTENER_CLIENTID);
			exit(EXIT_FAILURE);
			uv_timer_start(listener_timer_req, (void (*)(uv_timer_t *))listener_wait, MIN_DELAY, 0);
		} break;

		case -2: { // Connection lost
			logprintf(LOG_NOTICE, "mqtt_listener: connection for \"%s\"lost, will try to register again in %d seconds", LISTENER_CLIENTID, listener_delay / 1000);
			uv_timer_start(listener_timer_req, (void (*)(uv_timer_t *))listener_wait, listener_delay, 0);
			listener_delay *= 2;
			if(listener_delay > MAX_DELAY) {
				listener_delay = MAX_DELAY;
			}
		}
	}
}

static unsigned short int mqttHwInit(void) {
	initialized = 1;
	if(listener_topic != NULL && strlen(listener_topic) > 0) {
		if((listener_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		listener_timer_req->data = NULL; //no data needed
		uv_timer_init(uv_default_loop(), listener_timer_req);

		logprintf(LOG_DEBUG, "mqtt_listener: registering clientid \"%s\" for receive", LISTENER_CLIENTID);
		mqtt_sub(LISTENER_CLIENTID, default_host, default_port, default_user, default_pass, listener_topic, 1, 0, listener_callback, NULL); //no need for userdata, because this client is part of the main module
	}

	return EXIT_SUCCESS;
}

static unsigned short mqttSettings(struct JsonNode *json) {

	if(strcmp(json->key, "host") == 0) {
		default_host = NULL;
		if(json->tag == JSON_STRING) {
			if((default_host = MALLOC(strlen(json->string_) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(default_host, json->string_);
		} else {
			return EXIT_FAILURE;
		}
	}

	if(strcmp(json->key, "user") == 0) {
		default_user = NULL;
		if(json->tag == JSON_STRING) {
			if((default_user = MALLOC(strlen(json->string_) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(default_user, json->string_);
		} else {
			return EXIT_FAILURE;
		}
	}

	if(strcmp(json->key, "pass") == 0) {
		default_pass = NULL;
		if(json->tag == JSON_STRING) {
			if((default_pass = MALLOC(strlen(json->string_) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(default_pass, json->string_);
		} else {
			return EXIT_FAILURE;
		}
	}

	if(strcmp(json->key, "port") == 0) {
		if(json->tag == JSON_NUMBER) {
			default_port = (int)round(json->number_);
		} else {
			return EXIT_FAILURE;
		}
	}

	if(strcmp(json->key, "listener_topic") == 0) {
		if(json->tag == JSON_STRING) {
			if((listener_topic = MALLOC(strlen(json->string_) + 1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(listener_topic, json->string_);
		} else {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

void mqtt_send(char *clientid, char *message, void (*callback)(int, char *, char *, void *), void *userdata) {
	struct mqtt_clients_t *clients = mqtt_clients;
	struct mqtt_client_t *mqtt_client = MALLOC (sizeof(mqtt_client_t));

	if(!mqtt_client) {
		logprintf(LOG_ERR, "out of memory 1");
		exit(EXIT_FAILURE);
	}
	memset(mqtt_client, 0, sizeof(struct mqtt_client_t));

#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif

	//get client
	int registered = 0;
	while(clients) {
		mqtt_client = clients->data->data;
		if(strcmp(mqtt_client->clientid, clientid) == 0) { // registered
			registered = 1;
			break;
		}
		clients = clients->next;
	}

	if( registered == 1) {
		mqtt_client->callback = callback;
		mqtt_client->userdata = userdata;
		mqtt_client->called = 0;
		mqtt_client->message = MALLOC(strlen(message)+1);
		if(!mqtt_client->message) {
			logprintf(LOG_ERR, "out of memory 4");
			exit(EXIT_FAILURE);
		}
		strcpy(mqtt_client->message, message);
		mqtt_client->reqtype = PUBLISH;
		mqtt_client->step = MQTT_STEP_SEND_PUBLISH;
		uv_custom_write(mqtt_client->poll_req);
	} else {
		logprintf(LOG_DEBUG, "mqtt: client \"%s\" not registered", mqtt_client->clientid);
		if(mqtt_client->callback != NULL) {
			mqtt_client->callback(-1, "Not registered", mqtt_client->topic, userdata);
		}
	}
}

void mqtt_sub(char *clientid, char *host, int port, char *user, char *pass, char *topic, int qos, int keepalive, void (*callback)(int, char *, char *, void *userdata), void *userdata){
	mqtt_process(SUBSCRIBE, clientid, host, port, user, pass, topic, qos, NULL, keepalive, callback, userdata);
}

void mqtt_pub(char *clientid, char *host, int port, char *user, char *pass, char *topic, int qos, char *message, int keepalive, void (*callback)(int, char *, char *, void *userdata), void *userdata){

	if(message == NULL) {
		logprintf(LOG_NOTICE, "mqtt: client \"%s\" missing a message to be published", clientid);
		callback(-1, "No message provided", topic, userdata);
		return;
	}
	mqtt_process(PUBLISH, clientid, host, port, user, pass, topic, qos, message, keepalive, callback, userdata);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void mqttInit(void) {
	hardware_register(&mqtt);
	hardware_set_id(mqtt, "mqtt");
	options_add(&mqtt->options, "h", "host", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&mqtt->options, "p", "port", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&mqtt->options, "u", "user", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&mqtt->options, "s", "pass", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&mqtt->options, "t", "listener_topic", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	mqtt->init=&mqttHwInit;
	mqtt->settings=&mqttSettings;

	}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "mqtt";
	module->version = "0.1";
	module->reqversion = "8.1.3";
	module->reqcommit = NULL;
}

void init(void) {
	mqttInit();
}
#endif
