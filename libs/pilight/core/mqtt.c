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

/*
 * FIXME:
 * - QoS 1 & 2 persistent session queue
 * - Unsubscribe
 */

#define CLIENT_SIDE 0
#define SERVER_SIDE 1

typedef struct mqtt_messages_t {
	uv_poll_t *poll_req;
	char *message;
	char *topic;
	int msgid;
	int step;
	int qos;

	uv_timer_t *timer_req;

	struct mqtt_messages_t *next;
} mqtt_messages_t;

typedef struct mqtt_subscriptions_t {
	char *topic;
	int qos;
} mqtt_subscriptions_t;

typedef struct mqtt_topic_t {
	char *topic;
	char *message;

	int retain;

	struct mqtt_topic_t *next;
} mqtt_topic_t;


#ifdef _WIN32
	static uv_mutex_t mqtt_lock;
#else
	static pthread_mutex_t mqtt_lock;
	static pthread_mutexattr_t mqtt_attr;
#endif
static int lock_init = 0;
static int started = 0;

static uv_poll_t *poll_server_req = NULL;
static struct mqtt_client_t *mqtt_clients = NULL;
static struct mqtt_topic_t *mqtt_topics = NULL;
static struct mqtt_messages_t *mqtt_messages = NULL;

static char **mqtt_blacklist = NULL;
static char **mqtt_whitelist = NULL;
static int mqtt_blacklist_nr = 0;
static int mqtt_whitelist_nr = 0;

static void poll_close_cb(uv_poll_t *req);

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

void mqtt_gc(void) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	started = 0;

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

	{
		struct mqtt_topic_t *node = NULL;
		while(mqtt_topics) {
			node = mqtt_topics;
			if(node->topic != NULL) {
				FREE(node->topic);
			}
			if(node->message != NULL) {
				FREE(node->message);
			}
			mqtt_topics = mqtt_topics->next;
			FREE(node);
		}
	}

	{
		struct mqtt_messages_t *node = NULL;
		while(mqtt_messages) {
			node = mqtt_messages;
			if(node->topic != NULL) {
				FREE(node->topic);
			}
			if(node->message != NULL) {
				FREE(node->message);
			}
			uv_timer_stop(node->timer_req);
			mqtt_messages = mqtt_messages->next;
			FREE(node);
		}
	}

	{
		struct mqtt_client_t *node = NULL;
		while(mqtt_clients) {
			node = mqtt_clients->next;
			uv_timer_stop(mqtt_clients->timer_req);
			poll_close_cb(mqtt_clients->poll_req);
			mqtt_clients = node;
		}
	}

	if(poll_server_req != NULL) {
		poll_close_cb(poll_server_req);
		poll_server_req = NULL;
	}

#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
	return;
}

void mqtt_free(struct mqtt_pkt_t *pkt) {
	switch(pkt->type) {
		case MQTT_CONNECT: {
			if(pkt->header.connect.protocol != NULL) {
				FREE(pkt->header.connect.protocol);
			}
			if(pkt->payload.connect.username != NULL) {
				FREE(pkt->payload.connect.username);
			}
			if(pkt->payload.connect.password != NULL) {
				FREE(pkt->payload.connect.password);
			}
			if(pkt->payload.connect.clientid != NULL) {
				FREE(pkt->payload.connect.clientid);
			}
			if(pkt->payload.connect.willtopic != NULL) {
				FREE(pkt->payload.connect.willtopic);
			}
			if(pkt->payload.connect.willmessage != NULL) {
				FREE(pkt->payload.connect.willmessage);
			}
		} break;
		case MQTT_PUBLISH: {
			if(pkt->payload.publish.topic != NULL) {
				FREE(pkt->payload.publish.topic);
			}
			if(pkt->payload.publish.message != NULL) {
				FREE(pkt->payload.publish.message);
			}
		} break;
		case MQTT_SUBSCRIBE: {
			if(pkt->payload.subscribe.topic != NULL) {
				FREE(pkt->payload.subscribe.topic);
			}
		} break;
	}
}

int mosquitto_sub_topic_check(const char *str) {
	char c = '\0';
	int len = 0;

	while(str && str[0]) {
		if(str[0] == '+') {
			if((c != '\0' && c != '/') || (str[1] != '\0' && str[1] != '/')) {
				return -1;
			}
		} else if(str[0] == '#') {
			if((c != '\0' && c != '/') || str[1] != '\0') {
				return -1;
			}
		}
		len++;
		c = str[0];
		str = &str[1];
	}

	if(len > 65535) {
		return -1;
	}

	return 0;
}

/* Does a topic match a subscription? */
int mosquitto_topic_matches_sub(const char *sub, const char *topic, int *result) {
	int slen, tlen;
	int spos, tpos;
	int multilevel_wildcard = 0;

	if(!sub || !topic || !result) {
		return -1;
	}

	slen = strlen(sub);
	tlen = strlen(topic);

	if(slen && tlen) {
		if((sub[0] == '$' && topic[0] != '$') || (topic[0] == '$' && sub[0] != '$')) {
			*result = 0;
			return 0;
		}
	}

	spos = 0;
	tpos = 0;

	while(spos < slen && tpos < tlen) {
		if(sub[spos] == topic[tpos]) {
			if(tpos == tlen-1) {
				/* Check for e.g. foo matching foo/# */
				if(spos == slen-3
						&& sub[spos+1] == '/'
						&& sub[spos+2] == '#') {
					*result = 1;
					multilevel_wildcard = 1;
					return 0;
				}
			}
			spos++;
			tpos++;
			if(spos == slen && tpos == tlen){
				*result = 1;
				return 0;
			}else if(tpos == tlen && spos == slen-1 && sub[spos] == '+'){
				spos++;
				*result = 1;
				return 0;
			}
		} else {
			if(sub[spos] == '+') {
				spos++;
				while(tpos < tlen && topic[tpos] != '/') {
					tpos++;
				}
				if(tpos == tlen && spos == slen){
					*result = 1;
					return 0;
				}
			} else if(sub[spos] == '#') {
				multilevel_wildcard = 1;
				if(spos+1 != slen) {
					*result = 0;
					return 0;
				}else{
					*result = 1;
					return 0;
				}
			}else{
				*result = 0;
				return 0;
			}
		}
	}
	if(multilevel_wildcard == 0 && (tpos < tlen || spos < slen)) {
		*result = 0;
	}

	return 0;
}

void mqtt_client_remove(uv_poll_t *req, int disconnect) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif

	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct mqtt_client_t *currP, *prevP;
	struct mqtt_client_t *node = mqtt_clients;
	int ret = 0, x = 0, ret1 = 0;

	prevP = NULL;

	for(currP = mqtt_clients; currP != NULL; prevP = currP, currP = currP->next) {
		if(currP->poll_req == req) {
			if(prevP == NULL) {
				mqtt_clients = currP->next;
			} else {
				prevP->next = currP->next;
			}

			uv_timer_stop(currP->timer_req);

			node = mqtt_clients;
			while(node) {
				for(x=0;x<node->nrsubs;x++) {
					if((currP->haswill == 1 && disconnect == 0 &&
							mosquitto_topic_matches_sub(node->subs[x]->topic, currP->will.topic, &ret) == 0) ||
							mosquitto_topic_matches_sub(node->subs[x]->topic, "pilight/mqtt/disconnected", &ret1) == 0
						) {
						if((currP->haswill == 1 && disconnect == 0 && ret == 1) || ret1 == 1) {
							struct mqtt_pkt_t pkt1;
							unsigned int len = 0;
							unsigned char *buf = NULL;

							memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

							if(currP->haswill == 1 && disconnect == 0 && ret == 1) {
								mqtt_pkt_publish(&pkt1, 0, currP->will.qos, 0, currP->will.topic, node->msgid++, currP->will.message);
							}
							if(ret1 == 1) {
								mqtt_pkt_publish(&pkt1, 0, 0, 0, "pilight/mqtt/disconnected", node->msgid++, currP->id);
							}

							if(mqtt_encode(&pkt1, &buf, &len) == 0) {
								custom_poll_data = node->poll_req->data;

								iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
								node->flush = 1;
								if(node->msgid > 1024) {
									node->msgid = 0;
								}
								FREE(buf);
							}
							mqtt_free(&pkt1);
						}
					}
				}
				node = node->next;
			}

			int i = 0;
			for(i=0;i<currP->nrsubs;i++) {
				FREE(currP->subs[i]->topic);
				FREE(currP->subs[i]);
			}
			if(currP->nrsubs > 0) {
				FREE(currP->subs);
			}
			if(currP->id != NULL) {
				FREE(currP->id);
			}
			if(currP->will.message != NULL) {
				FREE(currP->will.message);
			}
			if(currP->will.topic != NULL) {
				FREE(currP->will.topic);
			}

			if(currP->poll_req->data != NULL) {
				struct uv_custom_poll_t *custom_poll_data = currP->poll_req->data;
				int r = 0, fd = 0;
				if((r = uv_fileno((uv_handle_t *)currP->poll_req, (uv_os_fd_t *)&fd)) != 0) {
					/*LCOV_EXCL_START*/
					logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
					/*LCOV_EXCL_STOP*/
				}
				if(custom_poll_data != NULL) {
					uv_custom_poll_free(custom_poll_data);
					currP->poll_req->data = NULL;
				}
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

/*LCOV_EXCL_START*/
void mqtt_dump(struct mqtt_pkt_t *pkt) {
	printf("-------------------\n");
	printf("type: %d\n", pkt->type);
	if(pkt->type == MQTT_PUBLISH ||
		 pkt->type == MQTT_SUBACK) {
		printf("dub: %d\n", pkt->dub);
		printf("qos: %d\n", pkt->qos);
		printf("retain: %d\n", pkt->retain);
	}
	if(pkt->type == MQTT_CONNECT ||
	   pkt->type == MQTT_SUBSCRIBE ||
	   pkt->type == MQTT_CONNACK ||
		 pkt->type == MQTT_PUBACK ||
		 pkt->type == MQTT_PUBREC ||
		 pkt->type == MQTT_PUBREL ||
	   pkt->type == MQTT_PINGREQ ||
	   pkt->type == MQTT_PINGRESP ||
		 pkt->type == MQTT_DISCONNECT) {
		printf("reserved: %d\n", pkt->reserved);
	}
	printf("\n");
	switch(pkt->type) {
		case MQTT_CONNECT: {
			printf("header\n");
			printf("\tprotocol name: %s\n", pkt->header.connect.protocol);
			printf("\tversion: %d\n", pkt->header.connect.version);
			printf("\tusername: %d\n", pkt->header.connect.username);
			printf("\tpassword: %d\n", pkt->header.connect.password);
			printf("\twill retain: %d\n", pkt->header.connect.willretain);
			printf("\tqos: %d\n", pkt->header.connect.qos);
			printf("\twill flag: %d\n", pkt->header.connect.willflag);
			printf("\tclean session: %d\n", pkt->header.connect.cleansession);
			printf("\treserved: %d\n", pkt->header.connect.reserved);

			printf("\n");
			printf("payload\n");
			printf("\tkeepalive: %d\n", pkt->payload.connect.keepalive);
			printf("\tclientid: %s\n", pkt->payload.connect.clientid);
			if(pkt->payload.connect.willtopic != NULL) {
				printf("\twill topic: %s\n", pkt->payload.connect.willtopic);
			}
			if(pkt->payload.connect.willmessage != NULL) {
				printf("\twill message: %s\n", pkt->payload.connect.willmessage);
			}
			if(pkt->header.connect.username == 1) {
				printf("\tusername: %s\n", pkt->payload.connect.username);
			}
			if(pkt->header.connect.password == 1) {
				printf("\tpassword: %s\n", pkt->payload.connect.password);
			}
		} break;
		case MQTT_CONNACK: {
			printf("header\n");
			printf("\treserved: %d\n", pkt->header.connack.reserved);
			printf("\tsession: %d\n", pkt->header.connack.session);

			printf("payload\n");
			printf("\tcode: %d\n", pkt->payload.connack.code);
		} break;
		case MQTT_PUBLISH: {
			printf("payload\n");
			if(pkt->qos > 0) {
				printf("\tmsgid: %d\n", pkt->payload.publish.msgid);
			}
			printf("\ttopic: %s\n", pkt->payload.publish.topic);
			printf("\tmessage: %s\n", pkt->payload.publish.message);
		} break;
		case MQTT_PUBACK: {
			printf("payload\n");
			printf("\tmsgid: %d\n", pkt->payload.puback.msgid);
		} break;
		case MQTT_PUBREC: {
			printf("payload\n");
			printf("\tmsgid: %d\n", pkt->payload.pubrec.msgid);
		} break;
		case MQTT_PUBREL: {
			printf("payload\n");
			printf("\tmsgid: %d\n", pkt->payload.pubrel.msgid);
		} break;
		case MQTT_PUBCOMP: {
			printf("payload\n");
			printf("\tmsgid: %d\n", pkt->payload.pubcomp.msgid);
		} break;
		case MQTT_SUBSCRIBE: {
			printf("payload\n");
			printf("\tmsgid: %d\n", pkt->payload.subscribe.msgid);
			printf("\ttopic: %s\n", pkt->payload.subscribe.topic);
			printf("\tqos: %d\n", pkt->payload.subscribe.qos);
		} break;
		case MQTT_SUBACK: {
			printf("payload\n");
			printf("\tmsgid: %d\n", pkt->payload.suback.msgid);
			printf("\tqos: %d\n", pkt->payload.suback.qos);
		} break;
	}
	printf("-------------------\n");
}
/*LCOV_EXCL_STOP*/

static void read_value(unsigned char *buf, unsigned int len, char **val) {
	if((*val = MALLOC(len+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memcpy(*val, buf, len);
	(*val)[len] = '\0';
}

static void write_value(unsigned char **buf, unsigned int *len, char *val, int write_length) {
	int l = strlen(val), i = 0;

	if(write_length == 1) {
		(*buf)[(*len)++] = (l & 0xFF00) >> 8;
		(*buf)[(*len)++] = (l & 0xFF);
	}
	for(i=0;i<l;i++) {
		(*buf)[(*len)++] = val[i];
	}
}

int mqtt_encode(struct mqtt_pkt_t *pkt, unsigned char **buf, unsigned int *len) {
	*len = 0;
	unsigned int msglen = 0;

	switch(pkt->type) {
		case MQTT_CONNECT: {
			msglen += 4;
			msglen += strlen(pkt->header.connect.protocol)+2;
			msglen += strlen(pkt->payload.connect.clientid)+2;
			if(pkt->payload.connect.willtopic != NULL) {
				msglen += strlen(pkt->payload.connect.willtopic)+2;
			}
			if(pkt->payload.connect.willmessage != NULL) {
				msglen += strlen(pkt->payload.connect.willmessage)+2;
			}
			if(pkt->header.connect.username == 1) {
				msglen += strlen(pkt->payload.connect.username)+2;
			}
			if(pkt->header.connect.password == 1) {
				msglen += strlen(pkt->payload.connect.password)+2;
			}
		} break;
		case MQTT_CONNACK: {
			msglen += 2;
		} break;
		case MQTT_PUBLISH: {
			msglen += strlen(pkt->payload.publish.topic)+2;
			if(pkt->qos > 0) {
				msglen += 2;
			}
			msglen += strlen(pkt->payload.publish.message);
		} break;
		case MQTT_PUBACK:
		case MQTT_PUBREC:
		case MQTT_PUBREL:
		case MQTT_PUBCOMP: {
			msglen += 2;
		} break;
		case MQTT_SUBSCRIBE: {
			msglen += 2;
			msglen += strlen(pkt->payload.subscribe.topic)+2;
			msglen += 1;
		} break;
		case MQTT_SUBACK: {
			msglen += 3;
		} break;
	}

	if(((*buf) = REALLOC((*buf), msglen+4)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset((*buf), 0, msglen+4);

	(*buf)[*len] = (pkt->type & 0x0F) << 4;

	if(pkt->type == MQTT_PUBLISH ||
		 pkt->type == MQTT_SUBACK) {
		(*buf)[*len] |= (pkt->dub & 0x01) << 3;
		(*buf)[*len] |= (pkt->qos & 0x03) << 1;
		(*buf)[*len] |= pkt->retain & 0x01;
	}
	(*len)++;

	unsigned int foo = msglen, msgbytes = 0;
	do {
		msgbytes++;
		unsigned char d = msglen % 128;
		msglen /= 128;

		/* if there are more digits to encode, set the top bit of this digit */
		if(msglen > 0) {
			d |= 0x80;
		}

		(*buf)[(*len)++] = d;
	} while (msglen > 0);

	switch(pkt->type) {
		case MQTT_CONNECT: {
			write_value(buf, len, pkt->header.connect.protocol, 1);
			(*buf)[(*len)++] = pkt->header.connect.version & 0x0F;
			(*buf)[(*len)] = (pkt->header.connect.username & 0x01) << 7;
			(*buf)[(*len)] |= (pkt->header.connect.password & 0x01) << 6;
			(*buf)[(*len)] |= (pkt->header.connect.willretain & 0x01) << 5;
			(*buf)[(*len)] |= (pkt->header.connect.qos & 0x03) << 3;
			(*buf)[(*len)] |= (pkt->header.connect.willflag & 0x01) << 2;
			(*buf)[(*len)] |= (pkt->header.connect.cleansession & 0x01) << 1;
			(*buf)[(*len)++] |= (pkt->header.connect.reserved & 0x01);

			(*buf)[(*len)++] = (pkt->payload.connect.keepalive & 0xFF00) >> 8;
			(*buf)[(*len)++] = pkt->payload.connect.keepalive & 0xFF;

			write_value(buf, len, pkt->payload.connect.clientid, 1);
			if(pkt->payload.connect.willtopic != NULL) {
				write_value(buf, len, pkt->payload.connect.willtopic, 1);
			}
			if(pkt->payload.connect.willmessage != NULL) {
				write_value(buf, len, pkt->payload.connect.willmessage, 1);
			}

			if(pkt->header.connect.username == 1) {
				write_value(buf, len, pkt->payload.connect.username, 1);
			}
			if(pkt->header.connect.password == 1) {
				write_value(buf, len, pkt->payload.connect.password, 1);
			}
		} break;
		case MQTT_CONNACK: {
			(*buf)[(*len)] = (pkt->header.connect.reserved & 0x7F) << 1;
			(*buf)[(*len)++] = (pkt->header.connack.session & 0x01);
			(*buf)[(*len)++] = pkt->payload.connack.code & 0xFF;
		} break;
		case MQTT_PUBLISH: {
			write_value(buf, len, pkt->payload.publish.topic, 1);
			if(pkt->qos > 0) {
				(*buf)[(*len)++] = (pkt->payload.publish.msgid & 0xFF00) >> 8;
				(*buf)[(*len)++] = pkt->payload.publish.msgid & 0xFF;
			}
			write_value(buf, len, pkt->payload.publish.message, 0);
		} break;
		case MQTT_PUBACK: {
			(*buf)[(*len)++] = (pkt->payload.puback.msgid & 0xFF00) >> 8;
			(*buf)[(*len)++] = pkt->payload.puback.msgid & 0xFF;
		} break;
		case MQTT_PUBREC: {
			(*buf)[(*len)++] = (pkt->payload.pubrec.msgid & 0xFF00) >> 8;
			(*buf)[(*len)++] = pkt->payload.pubrec.msgid & 0xFF;
		} break;
		case MQTT_PUBREL: {
			(*buf)[(*len)++] = (pkt->payload.pubrel.msgid & 0xFF00) >> 8;
			(*buf)[(*len)++] = pkt->payload.pubrel.msgid & 0xFF;
		} break;
		case MQTT_PUBCOMP: {
			(*buf)[(*len)++] = (pkt->payload.pubcomp.msgid & 0xFF00) >> 8;
			(*buf)[(*len)++] = pkt->payload.pubcomp.msgid & 0xFF;
		} break;
		case MQTT_SUBSCRIBE: {
			(*buf)[(*len)++] = (pkt->payload.subscribe.msgid & 0xFF00) >> 8;
			(*buf)[(*len)++] = pkt->payload.subscribe.msgid & 0xFF;
			write_value(buf, len, pkt->payload.subscribe.topic, 1);
			(*buf)[(*len)++] = pkt->payload.subscribe.qos & 0x03;
		} break;
		case MQTT_SUBACK: {
			(*buf)[(*len)++] = (pkt->payload.suback.msgid & 0xFF00) >> 8;
			(*buf)[(*len)++] = pkt->payload.suback.msgid & 0xFF;
			(*buf)[(*len)++] = pkt->payload.suback.qos & 0x03;
		} break;
	}

	assert(*len-(1+msgbytes) == foo);

	return 0;
}

int mqtt_decode(struct mqtt_pkt_t **pkt, unsigned char *buf, unsigned int len, unsigned int *pos) {
	unsigned int length = 0, startpos = 0;

	if(len < 2) {
		/*
		 * Packet too short for minimal MQTT
		 */
		return -1;
	}

	if(((*pkt) = MALLOC(sizeof(struct mqtt_pkt_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset((*pkt), 0, sizeof(struct mqtt_pkt_t));

	(*pkt)->type = (buf[(*pos)] & 0xF0) >> 4;
	if((*pkt)->type == MQTT_PUBLISH ||
		 (*pkt)->type == MQTT_PUBCOMP ||
		 (*pkt)->type == MQTT_SUBACK) {
		(*pkt)->dub = (buf[(*pos)] & 0x08) >> 3;
		(*pkt)->qos = (buf[(*pos)] & 0x06) >> 1;
		(*pkt)->retain = (buf[(*pos)++] & 0x01);
	}
	if((*pkt)->type == MQTT_SUBSCRIBE ||
		 (*pkt)->type == MQTT_CONNECT ||
		 (*pkt)->type == MQTT_CONNACK ||
		 (*pkt)->type == MQTT_PUBACK ||
		 (*pkt)->type == MQTT_PUBREC ||
		 (*pkt)->type == MQTT_PUBREL ||
		 (*pkt)->type == MQTT_PINGREQ ||
		 (*pkt)->type == MQTT_PINGRESP ||
		 (*pkt)->type == MQTT_DISCONNECT) {
		(*pkt)->reserved = buf[(*pos)++] & 0x0F;
	}

	unsigned int multiplier = 1, msglength = 0;
	do {
		msglength += (buf[(*pos)] & 0x7F) * multiplier;
		multiplier *= 128;
	} while(buf[(*pos)++] >= 128);

	startpos = (*pos);

	if(len < msglength+((*pos)-1)) {
		/*
		 * Packet too short for full message
		 */
		mqtt_free((*pkt));
		FREE((*pkt));
		return 1;
	}

	switch((*pkt)->type) {
		case MQTT_CONNECT: {
			{
				length = (buf[(*pos)] << 8) | buf[(*pos)+1]; (*pos)+=2;
				read_value(&buf[(*pos)], length, &(*pkt)->header.connect.protocol);
				(*pos)+=length;
			}

			(*pkt)->header.connect.version = buf[(*pos)++];
			(*pkt)->header.connect.username = (buf[(*pos)] >> 7) & 0x01;
			(*pkt)->header.connect.password = (buf[(*pos)] >> 6) & 0x01;
			(*pkt)->header.connect.willretain = (buf[(*pos)] >> 5) & 0x01;
			(*pkt)->header.connect.qos = (buf[(*pos)] >> 3) & 0x03;
			(*pkt)->header.connect.willflag = (buf[(*pos)] >> 2) & 0x01;
			(*pkt)->header.connect.cleansession = (buf[(*pos)] >> 1) & 0x01;
			(*pkt)->header.connect.reserved = (buf[(*pos)++]) & 0x01;

			(*pkt)->payload.connect.keepalive = buf[(*pos)] << 8 | buf[(*pos)+1]; (*pos)+=2;

			{
				length = (buf[(*pos)]) | buf[(*pos)+1]; (*pos)+=2;
				read_value(&buf[(*pos)], length, &(*pkt)->payload.connect.clientid);
				(*pos)+=length;
			}

			{
				if((*pos) < msglength) {
					length = (buf[(*pos)]) | buf[(*pos)+1]; (*pos)+=2;
					read_value(&buf[(*pos)], length, &(*pkt)->payload.connect.willtopic);
					(*pos)+=length;
				}
			}

			{
				if((*pos) < msglength) {
					length = (buf[(*pos)]) | buf[(*pos)+1]; (*pos)+=2;
					read_value(&buf[(*pos)], length, &(*pkt)->payload.connect.willmessage);
					(*pos)+=length;
				}
			}

			{
				if((*pkt)->header.connect.username == 1) {
					length = (buf[(*pos)]) | buf[(*pos)+1]; (*pos)+=2;
					read_value(&buf[(*pos)], length, &(*pkt)->payload.connect.username);
					(*pos)+=length;
				}
			}

			{
				if((*pkt)->header.connect.password == 1) {
					length = (buf[(*pos)]) | buf[(*pos)+1]; (*pos)+=2;
					read_value(&buf[(*pos)], length, &(*pkt)->payload.connect.password);
					(*pos)+=length;
				}
			}
		} break;
		case MQTT_CONNACK: {
			(*pkt)->header.connack.reserved = buf[(*pos)] >> 1;
			(*pkt)->header.connack.session = buf[(*pos)++] & 0x01;
			(*pkt)->payload.connack.code = buf[(*pos)++];
		} break;
		case MQTT_PUBLISH: {
			{
				length = (buf[(*pos)]) | buf[(*pos)+1]; (*pos)+=2;
				if((int)length < 0) {
					logprintf(LOG_ERR, "received incomplete message");
					mqtt_free((*pkt));
					FREE((*pkt));
					return -1;
				}
				read_value(&buf[(*pos)], length, &(*pkt)->payload.publish.topic);
				(*pos)+=length;
			}

			if((*pkt)->qos > 0) {
				(*pkt)->payload.publish.msgid = (buf[(*pos)] << 8) | buf[(*pos)+1]; (*pos)+=2;
			}

			{
				length = msglength-((*pos)-startpos);
				if((int)length < 0) {
					logprintf(LOG_ERR, "received incomplete message");
					mqtt_free((*pkt));
					FREE((*pkt));
					return -1;
				}
				read_value(&buf[(*pos)], length, &(*pkt)->payload.publish.message);
				(*pos)+=length;
			}
		} break;
		case MQTT_PUBACK: {
			(*pkt)->payload.puback.msgid = (buf[(*pos)] << 8) | buf[(*pos)+1]; (*pos)+=2;
		} break;
		case MQTT_PUBREC: {
			(*pkt)->payload.pubrec.msgid = (buf[(*pos)] << 8) | buf[(*pos)+1]; (*pos)+=2;
		} break;
		case MQTT_PUBREL: {
			(*pkt)->payload.pubrel.msgid = (buf[(*pos)] << 8) | buf[(*pos)+1]; (*pos)+=2;
		} break;
		case MQTT_PUBCOMP: {
			(*pkt)->payload.pubcomp.msgid = (buf[(*pos)] << 8) | buf[(*pos)+1]; (*pos)+=2;
		} break;
		case MQTT_SUBSCRIBE: {
			(*pkt)->payload.subscribe.msgid = buf[(*pos)] | buf[(*pos)+1]; (*pos)+=2;

			{
				length = (buf[(*pos)]) | buf[(*pos)+1]; (*pos)+=2;
				read_value(&buf[(*pos)], length, &(*pkt)->payload.subscribe.topic);
				(*pos)+=length;
			}

			(*pkt)->payload.subscribe.qos = buf[(*pos)++];
		} break;
		case MQTT_SUBACK: {
			(*pkt)->payload.suback.msgid = buf[(*pos)] | buf[(*pos)+1]; (*pos)+=2;
			(*pkt)->payload.suback.qos = buf[(*pos)++];
		} break;
	}

	return 0;
}

int mqtt_pkt_connect(struct mqtt_pkt_t *pkt, int dub, int qos, int retain, char *clientid, char *protocol, int version, char *username, char *password, int willretain, int willflag, int cleansession, int keepalive, char *topic, char *message) {
	if(qos < 0 || qos > 2) {
		return -1;
	}
	if(protocol == NULL || clientid == NULL) {
		return -1;
	}

	pkt->type = MQTT_CONNECT;
	pkt->dub = dub;
	pkt->qos = qos;
	pkt->retain = retain;

	if((pkt->header.connect.protocol = STRDUP(protocol)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	pkt->header.connect.version = version;
	pkt->header.connect.username = (username != NULL);
	pkt->header.connect.password = (password != NULL);
	pkt->header.connect.willretain = willretain;
	pkt->header.connect.qos = qos;
	pkt->header.connect.willflag = willflag;
	pkt->header.connect.cleansession = cleansession;
	pkt->header.connect.reserved = 0;

	pkt->payload.connect.keepalive = keepalive;

	if((pkt->payload.connect.clientid = STRDUP(clientid)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	if(topic != NULL) {
		if((pkt->payload.connect.willtopic = STRDUP(topic)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
	}

	if(message != NULL) {
		if((pkt->payload.connect.willmessage = STRDUP(message)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
	}

	if(username != NULL) {
		if((pkt->payload.connect.username = STRDUP(username)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
	}

	if(password != NULL) {
		if((pkt->payload.connect.password = STRDUP(password)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
	}
	return 0;
}

int mqtt_pkt_connack(struct mqtt_pkt_t *pkt, enum mqtt_connection_t status) {
	pkt->type = MQTT_CONNACK;
	pkt->reserved = 0;

	pkt->payload.connack.code = (int)status;

	return 0;
}

int mqtt_pkt_publish(struct mqtt_pkt_t *pkt, int dub, int qos, int retain, char *topic, int msgid, char *message) {
	if(qos < 0 || qos > 2) {
		return -1;
	}
	if(topic == NULL || message == NULL) {
		return -1;
	}

	pkt->type = MQTT_PUBLISH;
	pkt->dub = dub;
	pkt->qos = qos;
	pkt->retain = retain;

	if((pkt->payload.publish.topic = STRDUP(topic)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	pkt->payload.publish.msgid = msgid;

	if((pkt->payload.publish.message = STRDUP(message)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	return 0;
}

static int mqtt_pkt_pubxxx(struct mqtt_pkt_t *pkt, int type, int msgid) {
	pkt->type = type;

	switch(type) {
		case MQTT_PUBACK: {
			pkt->payload.puback.msgid = msgid;
		} break;
		case MQTT_PUBREC: {
			pkt->payload.pubrec.msgid = msgid;
		} break;
		case MQTT_PUBREL: {
			pkt->payload.pubrel.msgid = msgid;
		} break;
		case MQTT_PUBCOMP: {
			pkt->payload.pubcomp.msgid = msgid;
		} break;
	}

	return 0;
}

int mqtt_pkt_puback(struct mqtt_pkt_t *pkt, int msgid) {
	return mqtt_pkt_pubxxx(pkt, MQTT_PUBACK, msgid);
}

int mqtt_pkt_pubrec(struct mqtt_pkt_t *pkt, int msgid) {
	return mqtt_pkt_pubxxx(pkt, MQTT_PUBREC, msgid);
}

int mqtt_pkt_pubrel(struct mqtt_pkt_t *pkt, int msgid) {
	return mqtt_pkt_pubxxx(pkt, MQTT_PUBREL, msgid);
}

int mqtt_pkt_pubcomp(struct mqtt_pkt_t *pkt, int msgid) {
	return mqtt_pkt_pubxxx(pkt, MQTT_PUBCOMP, msgid);
}

int mqtt_pkt_subscribe(struct mqtt_pkt_t *pkt, char *topic, int msgid, int qos) {
	if(qos < 0 || qos > 2) {
		return -1;
	}
	if(topic == NULL) {
		return -1;
	}

	pkt->type = MQTT_SUBSCRIBE;

	if((pkt->payload.subscribe.topic = STRDUP(topic)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	pkt->payload.subscribe.msgid = msgid;
	pkt->payload.subscribe.qos = qos;

	return 0;
}

int mqtt_pkt_suback(struct mqtt_pkt_t *pkt, int msgid, int qos) {
	if(qos < 0 || qos > 2) {
		return -1;
	}

	pkt->type = MQTT_SUBACK;

	pkt->payload.suback.msgid = msgid;
	pkt->payload.suback.qos = qos;

	return 0;
}

int mqtt_pkt_pingreq(struct mqtt_pkt_t *pkt) {
	pkt->type = MQTT_PINGREQ;

	return 0;
}

int mqtt_pkt_pingresp(struct mqtt_pkt_t *pkt) {
	pkt->type = MQTT_PINGRESP;

	return 0;
}

int mqtt_pkt_disconnect(struct mqtt_pkt_t *pkt) {
	pkt->type = MQTT_DISCONNECT;

	return 0;
}

static void mqtt_client_timeout(uv_timer_t *handle) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	struct mqtt_client_t *client = handle->data;
	uv_poll_t *poll_req = client->poll_req;

	if(client->timer_req != NULL) {
		uv_timer_stop(client->timer_req);
	}

	uv_custom_close(poll_req);
#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

static void mqtt_message_resend(uv_timer_t *handle) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	struct mqtt_messages_t *message = handle->data;
	uv_poll_t *poll_req = message->poll_req;
	struct uv_custom_poll_t *custom_poll_data = poll_req->data;

	struct mqtt_pkt_t pkt1;
	unsigned int len = 0;
	unsigned char *buf = NULL;
	mqtt_pkt_publish(&pkt1, 1, message->qos, 0, message->topic, message->msgid, message->message);

	if(mqtt_encode(&pkt1, &buf, &len) == 0) {
		iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
		uv_custom_write(poll_req);
		FREE(buf);
	}
	mqtt_free(&pkt1);

#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

void mqtt_client_register(uv_poll_t *req, struct mqtt_pkt_t *pkt) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif

	struct mqtt_client_t *clients = mqtt_clients;
	struct mqtt_client_t *node = NULL;
	int known = 0;
	while(clients) {
		if(clients->side == SERVER_SIDE) {
			if(clients->poll_req == req) {
				node = clients;
				known = 1;
			/*
			 * Clear state when not in persistent session
			 */
			} else if(clients->id != NULL && strcmp(clients->id, pkt->payload.connect.clientid) == 0) {
				node = clients;
				known = 2;
			}
		}
		clients = clients->next;
	}

	int r = 0, fd = 0;
	if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
		/*LCOV_EXCL_STOP*/
	}

	if(known == 2 && fd != node->fd) {
		uv_timer_stop(node->timer_req);

		uv_custom_close(node->poll_req);

#ifdef _WIN32
		uv_mutex_unlock(&mqtt_lock);
#else
		pthread_mutex_unlock(&mqtt_lock);
#endif
		return;
	}

	if((node->id = STRDUP(pkt->payload.connect.clientid)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	node->keepalive = pkt->payload.connect.keepalive;

	if(pkt->type == MQTT_CONNECT) {
		if(pkt->header.connect.willflag == 1) {
			if((node->will.message = STRDUP(pkt->payload.connect.willmessage)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			if((node->will.topic = STRDUP(pkt->payload.connect.willtopic)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			node->will.retain = pkt->header.connect.willretain;
			node->will.qos = pkt->header.connect.qos;
			node->haswill = 1;
		}
	}

	if(known == 1) {
		if(node->keepalive > 0) {
			uv_update_time(uv_default_loop());
#ifdef PILIGHT_UNITTEST
			uv_timer_start(node->timer_req, mqtt_client_timeout, (node->keepalive*1.5)*100, 0);
#else
			uv_timer_start(node->timer_req, mqtt_client_timeout, (node->keepalive*1.5)*1000, 0);
#endif
		}
	}

	clients = mqtt_clients;

	struct uv_custom_poll_t *custom_poll_data = NULL;
	int ret = 0, x = 0;
	while(clients) {
		for(x=0;x<clients->nrsubs;x++) {
			if(mosquitto_topic_matches_sub(clients->subs[x]->topic, "pilight/mqtt/connected", &ret) == 0) {
				if(ret == 1) {
					struct mqtt_pkt_t pkt1;
					unsigned int len = 0;
					unsigned char *buf = NULL;

					memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

					mqtt_pkt_publish(&pkt1, 0, 0, 0, "pilight/mqtt/connected", clients->msgid++, node->id);

					if(mqtt_encode(&pkt1, &buf, &len) == 0) {
						custom_poll_data = clients->poll_req->data;

						iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
						clients->flush = 1;
						if(clients->msgid > 1024) {
							clients->msgid = 0;
						}
						FREE(buf);
					}
					mqtt_free(&pkt1);
				}
			}
		}
		clients = clients->next;
	}

#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

void mqtt_client_add(struct mqtt_client_t **node, uv_poll_t *req, int side) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif

	if(((*node) = MALLOC(sizeof(struct mqtt_client_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset((*node), 0, sizeof(struct mqtt_client_t));

	(*node)->poll_req = req;
	(*node)->side = side;

	if(((*node)->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	(*node)->timer_req->data = (*node);

	uv_timer_init(uv_default_loop(), (*node)->timer_req);

	uv_update_time(uv_default_loop());
#ifdef PILIGHT_UNITTEST
	uv_timer_start((*node)->timer_req, mqtt_client_timeout, 150, 0);
#else
	uv_timer_start((*node)->timer_req, mqtt_client_timeout, 3000, 0);
#endif

	(*node)->next = mqtt_clients;
	mqtt_clients = (*node);

#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

static void mqtt_subscribtion_add(uv_poll_t *req, char *topic, int qos) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif

	struct mqtt_topic_t *node1 = mqtt_topics;
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct mqtt_client_t *node = mqtt_clients;
	int i = 0, match = 0, ret = 0;

	while(node) {
		if(node->poll_req == req) {
			break;
		}
		node = node->next;
	}
	if(node != NULL) {
		for(i=0;i<node->nrsubs;i++) {
			if(strcmp(node->subs[i]->topic, topic) == 0) {
				match = 1;
				break;
			}
		}
		if(match == 0) {
			if((node->subs = REALLOC(node->subs, sizeof(struct mqtt_subscriptions_t *)*(node->nrsubs+1))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			if((node->subs[node->nrsubs] = MALLOC(sizeof(struct mqtt_subscriptions_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			memset(node->subs[node->nrsubs], 0, sizeof(struct mqtt_subscriptions_t));
			if((node->subs[node->nrsubs]->topic = STRDUP(topic)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			node->subs[node->nrsubs]->qos = qos;
			node->nrsubs++;
		}
	}

	while(node1) {
		if(node1->retain == 1) {
			ret = 0;
			if(mosquitto_topic_matches_sub(topic, node1->topic, &ret) == 0) {
				if(ret == 1) {
					struct mqtt_pkt_t pkt1;
					unsigned int len = 0;
					unsigned char *buf = NULL;

					memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

					mqtt_pkt_publish(&pkt1, 0, qos, 0, node1->topic, node->msgid++, node1->message);

					if(mqtt_encode(&pkt1, &buf, &len) == 0) {
						custom_poll_data = node->poll_req->data;

						iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
						node->flush = 1;
						if(node->msgid > 1024) {
							node->msgid = 0;
						}
						node->flush = 1;
						FREE(buf);
					}
					mqtt_free(&pkt1);
				}
			}
		}
		node1 = node1->next;
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

	if(custom_poll_data->started == 1) {
		client->step = MQTT_DISCONNECTED;
		client->callback(client, NULL, client->userdata);
	} else {
		client->callback(NULL, NULL, client->userdata);
	}

	mqtt_client_remove(client->poll_req, 1);
}

static void poll_close_cb(uv_poll_t *req) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = req->data;
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
		shutdown(fd, SHUT_RDWR);
		close(fd);
#endif
	}

	mqtt_client_remove(req, 0);

	if(!uv_is_closing((uv_handle_t *)req)) {
		uv_close((uv_handle_t *)req, close_cb);
	}

	if(req->data != NULL) {
		uv_custom_poll_free(custom_poll_data);
		req->data = NULL;
	}
}

static void mqtt_topic_add(int retain, char *topic, char *message) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif

	struct mqtt_topic_t *node = mqtt_topics;
	while(node) {
		if(strcmp(node->topic, topic) == 0) {
			break;
		}
		node = node->next;
	}
	if(node == NULL) {
		if((node = MALLOC(sizeof(struct mqtt_topic_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(node, 0, sizeof(struct mqtt_topic_t));

		node->next = mqtt_topics;
		mqtt_topics = node;
	}

	if(node->topic != NULL) {
		FREE(node->topic);
	}

	if(node->message != NULL) {
		FREE(node->message);
	}

	if((node->topic = STRDUP(topic)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	if((node->message = STRDUP(message)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	node->retain = retain;

#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

static void mqtt_process_publish(uv_poll_t *req, struct mqtt_pkt_t *pkt) {
#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct mqtt_client_t *node = mqtt_clients;
	int ret = 0, x = 0, match = 0, pass = 1;

	if(pkt->type == MQTT_PUBLISH) {
		for(x=0;x<mqtt_blacklist_nr;x++) {
			if(mosquitto_topic_matches_sub(mqtt_blacklist[x], pkt->payload.publish.topic, &ret) == 0) {
				if(ret == 1) {
					pass = 0;
				}
			}
		}
		if(pass == 0) {
			for(x=0;x<mqtt_whitelist_nr;x++) {
				if(mosquitto_topic_matches_sub(mqtt_whitelist[x], pkt->payload.publish.topic, &ret) == 0) {
					if(ret == 1) {
						pass = 1;
					}
				}
			}
		}
		if(pass == 0) {
#ifdef _WIN32
			uv_mutex_unlock(&mqtt_lock);
#else
			pthread_mutex_unlock(&mqtt_lock);
#endif
			return;
		}

		while(node) {
			if(node->side == SERVER_SIDE && (req == NULL || node->poll_req != req)) {
				for(x=0;x<node->nrsubs;x++) {
					if(mosquitto_topic_matches_sub(node->subs[x]->topic, pkt->payload.publish.topic, &ret) == 0) {
						if(ret == 1) {
							struct mqtt_pkt_t pkt1;
							unsigned int len = 0;
							unsigned char *buf = NULL;
							int msgid = node->msgid++;
							int retry = 1;

							while(retry) {
								retry = 0;
								struct mqtt_messages_t *messages = mqtt_messages;
								while(messages) {
									if(messages->poll_req == node->poll_req &&
										messages->qos == 2) {
										if(msgid == messages->msgid) {
											msgid = node->msgid++;
											retry = 1;
											break;
										}
									}
									messages = messages->next;
								}
							}

							memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

							if(pkt->type == MQTT_PUBLISH) {
								if(node->subs[x]->qos > 0) {
									struct mqtt_messages_t *message = MALLOC(sizeof(struct mqtt_messages_t));
									if(message == NULL) {
										OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
									}
									memset(message, 0, sizeof(struct mqtt_messages_t));
									message->qos = node->subs[x]->qos;
									message->poll_req = node->poll_req;
									message->msgid = msgid;
									if((message->message = STRDUP(pkt->payload.publish.message)) == NULL) {
										OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
									}
									if((message->topic = STRDUP(pkt->payload.publish.topic)) == NULL) {
										OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
									}

									if((message->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
										OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
									}

									uv_timer_init(uv_default_loop(), message->timer_req);
									if(node->keepalive > 0) {
										uv_update_time(uv_default_loop());
#ifdef PILIGHT_UNITTEST
										uv_timer_start(message->timer_req, mqtt_message_resend, 100, 100);
#else
										uv_timer_start(message->timer_req, mqtt_message_resend, 3000, 3000);
#endif
									}
									message->timer_req->data = message;

									if(node->subs[x]->qos == 1) {
										message->step = MQTT_PUBACK;
									} else if(node->subs[x]->qos == 2) {
										message->step = MQTT_PUBREC;
									}
									message->next = mqtt_messages;
									mqtt_messages = message;
								}

								if(node->msgid > 1024) {
									node->msgid = 0;
								}

								mqtt_pkt_publish(&pkt1, 0, node->subs[x]->qos, 0, pkt->payload.publish.topic, msgid, pkt->payload.publish.message);

								if(mqtt_encode(&pkt1, &buf, &len) == 0) {
									custom_poll_data = node->poll_req->data;

									iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
									node->flush = 1;
									FREE(buf);
								}
								mqtt_free(&pkt1);
							}
						}
					}
				}
			}
			node = node->next;
		}
	}
	if(pkt->type == MQTT_PUBACK) {
		struct mqtt_messages_t *currP = NULL, *prevP = NULL;

		for(currP = mqtt_messages; currP != NULL; prevP = currP, currP = currP->next) {
			if((req == NULL || currP->poll_req == req) &&
				 currP->msgid == pkt->payload.puback.msgid &&
				 currP->qos == 1 &&
				 currP->step == MQTT_PUBACK) {

				if(prevP == NULL) {
					mqtt_messages = currP->next;
				} else {
					prevP->next = currP->next;
				}

				match = 1;
				uv_timer_stop(currP->timer_req);

				FREE(currP->message);
				FREE(currP->topic);

				FREE(currP);
				break;
			}
		}
	}
	if(pkt->type == MQTT_PUBREC) {
		struct mqtt_messages_t *messages = mqtt_messages;
		while(messages) {
			if((req == NULL || messages->poll_req == req) &&
				 messages->msgid == pkt->payload.pubrec.msgid &&
				 messages->qos == 2 &&
				 messages->step == MQTT_PUBREC) {

				match = 1;
				messages->step = MQTT_PUBCOMP;

				struct mqtt_pkt_t pkt1;
				unsigned int len = 0;
				unsigned char *buf = NULL;

				memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

				mqtt_pkt_pubrel(&pkt1, messages->msgid);

				if(mqtt_encode(&pkt1, &buf, &len) == 0) {
					custom_poll_data = messages->poll_req->data;

					iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
					while(node) {
						if(node->poll_req == messages->poll_req) {
							node->flush = 1;
							break;
						}
						node = node->next;
					}
					FREE(buf);
				}
			}
			messages = messages->next;
		}
	}

	if(pkt->type == MQTT_PUBCOMP) {
		struct mqtt_messages_t *currP = NULL, *prevP = NULL;

		for(currP = mqtt_messages; currP != NULL; prevP = currP, currP = currP->next) {
			if((req == NULL || currP->poll_req == req) &&
				 currP->msgid == pkt->payload.pubcomp.msgid &&
				 currP->qos == 2 &&
				 currP->step == MQTT_PUBCOMP) {

				if(prevP == NULL) {
					mqtt_messages = currP->next;
				} else {
					prevP->next = currP->next;
				}

				match = 1;
				uv_timer_stop(currP->timer_req);

				FREE(currP->message);
				FREE(currP->topic);

				struct mqtt_pkt_t pkt1;
				unsigned int len = 0;
				unsigned char *buf = NULL;

				memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

				mqtt_pkt_pubcomp(&pkt1, currP->msgid);

				if(mqtt_encode(&pkt1, &buf, &len) == 0) {
					custom_poll_data = currP->poll_req->data;

					iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
					while(node) {
						if(node->poll_req == currP->poll_req) {
							node->flush = 1;
							break;
						}
						node = node->next;
					}
					FREE(buf);
				}

				FREE(currP);
				break;
			}
		}
	}
	/*
	 * Also respond to messages when they are
	 * unknown to us.
	 */
	if(match == 0) {
		if(pkt->type == MQTT_PUBREC) {
			struct mqtt_pkt_t pkt1;
			unsigned int len = 0;
			unsigned char *buf = NULL;

			memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

			mqtt_pkt_pubrel(&pkt1, pkt->payload.pubrec.msgid);

			if(mqtt_encode(&pkt1, &buf, &len) == 0) {
				custom_poll_data = req->data;

				iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
				while(node) {
					if(node->poll_req == req) {
						node->flush = 1;
						break;
					}
					node = node->next;
				}
				FREE(buf);
			}
		}
		if(pkt->type == MQTT_PUBCOMP) {
			struct mqtt_pkt_t pkt1;
			unsigned int len = 0;
			unsigned char *buf = NULL;

			memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));

			mqtt_pkt_pubcomp(&pkt1, pkt->payload.pubcomp.msgid);

			if(mqtt_encode(&pkt1, &buf, &len) == 0) {
				custom_poll_data = req->data;

				iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
				while(node) {
					if(node->poll_req == req) {
						node->flush = 1;
						break;
					}
					node = node->next;
				}
				FREE(buf);
			}
		}
	}

#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif
}

int mqtt_subscribe(struct mqtt_client_t *client, char *topic, int qos) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;

		mqtt_pkt_subscribe(&pkt, topic, 0, qos);
		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		mqtt_free(&pkt);
		uv_custom_write(poll_req);
	}
	return 0;
}

int mqtt_publish(struct mqtt_client_t *client, int dub, int qos, int retain, char *topic, char *message) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;


		mqtt_pkt_publish(&pkt, dub, qos, retain, topic, client->msgid++, message);
		if(client->msgid > 1024) {
			client->msgid = 0;
		}

		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		mqtt_free(&pkt);
		uv_custom_write(poll_req);
	}
	return 0;
}

int mqtt_puback(struct mqtt_client_t *client, int msgid) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;


		mqtt_pkt_puback(&pkt, msgid);
		if(client->msgid > 1024) {
			client->msgid = 0;
		}

		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		mqtt_free(&pkt);
		uv_custom_write(poll_req);
	}
	return 0;
}

int mqtt_pubrec(struct mqtt_client_t *client, int msgid) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;


		mqtt_pkt_pubrec(&pkt, msgid);
		if(client->msgid > 1024) {
			client->msgid = 0;
		}

		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		mqtt_free(&pkt);
		uv_custom_write(poll_req);
	}
	return 0;
}

int mqtt_pubrel(struct mqtt_client_t *client, int msgid) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;

		mqtt_pkt_pubrel(&pkt, msgid);
		if(client->msgid > 1024) {
			client->msgid = 0;
		}

		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		mqtt_free(&pkt);
		uv_custom_write(poll_req);
	}
	return 0;
}

int mqtt_pubcomp(struct mqtt_client_t *client, int msgid) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;

		mqtt_pkt_pubcomp(&pkt, msgid);
		if(client->msgid > 1024) {
			client->msgid = 0;
		}

		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		mqtt_free(&pkt);
		uv_custom_write(poll_req);
	}
	return 0;
}

int mqtt_ping(struct mqtt_client_t *client) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;

		mqtt_pkt_pingreq(&pkt);
		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		uv_custom_write(poll_req);
	}
	return 0;
}

int mqtt_disconnect(struct mqtt_client_t *client) {
	if(client->step != MQTT_CONNECTED) {
		return -1;
	} else {
		uv_poll_t *poll_req = client->poll_req;
		struct uv_custom_poll_t *custom_poll_data = poll_req->data;

		struct mqtt_pkt_t pkt;
		unsigned char *buf = NULL;
		unsigned int len = 0;

		mqtt_pkt_disconnect(&pkt);
		if(mqtt_encode(&pkt, &buf, &len) == 0) {
			iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
			FREE(buf);
		}
		uv_custom_write(poll_req);
	}
	return 0;
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
		if(client->side == CLIENT_SIDE) {
			if(client->step == MQTT_CONNECT) {
				struct mqtt_pkt_t pkt;
				unsigned char *buf = NULL;
				unsigned int len = 0;

				memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

#ifdef PILIGHT_UNITTEST
				mqtt_pkt_connect(&pkt, 0, client->will.qos, 0, client->id, "mqtt", 4, NULL, NULL, client->will.retain, (client->will.topic != NULL && client->will.message != NULL), 1, 1, client->will.topic, client->will.message);
#else
				mqtt_pkt_connect(&pkt, 0, client->will.qos, 0, client->id, "mqtt", 4, NULL, NULL, client->will.retain, (client->will.topic != NULL && client->will.message != NULL), 1, 60, client->will.topic, client->will.message);
#endif
				if(mqtt_encode(&pkt, &buf, &len) == 0) {
					iobuf_append(&custom_poll_data->send_iobuf, (void *)buf, len);
					FREE(buf);
				}
				mqtt_free(&pkt);
				uv_custom_write(req);
				client->step = MQTT_CONNACK;
			}
		}
	}
	uv_custom_read(req);
}

static void client_read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	struct uv_custom_poll_t *custom_poll_data = req->data;
	struct mqtt_client_t *client = custom_poll_data->data;

	struct mqtt_pkt_t *pkt = NULL;
	unsigned int pos = 0, len = 0;
	unsigned char *buf1 = NULL;
	int ret = 0;

	if(*nread > 0) {
		buf[*nread] = '\0';

		while(*nread > 0 && (ret = mqtt_decode(&pkt, (unsigned char *)buf, *nread, &pos)) == 0) {
			memmove(&buf[0], &buf[pos], *nread-pos);
			*nread -= pos;
			pos = 0;

			struct mqtt_pkt_t pkt1;
			memset(&pkt1, 0, sizeof(struct mqtt_pkt_t));
			len = 0;

			int fd = -1, r = 0;

			if((r = uv_fileno((uv_handle_t *)req, (uv_os_fd_t *)&fd)) != 0) {
				/*LCOV_EXCL_START*/
				logprintf(LOG_ERR, "uv_fileno: %s", uv_strerror(r));
				/*LCOV_EXCL_STOP*/
			}
			// mqtt_dump(pkt);

			if(client == NULL) {
				switch(pkt->type) {
					case MQTT_CONNECT: {
						if(pkt->header.connect.cleansession == 0) {
							mqtt_pkt_connack(&pkt1, MQTT_CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION);
							mqtt_client_remove(req, 1);
						} else {
							mqtt_client_register(req, pkt);
							mqtt_pkt_connack(&pkt1, MQTT_CONNECTION_ACCEPTED);
						}
						if(mqtt_encode(&pkt1, &buf1, &len) == 0) {
							iobuf_append(&custom_poll_data->send_iobuf, (void *)buf1, len);
							FREE(buf1);
						}
					} break;
					case MQTT_DISCONNECT: {
						mqtt_client_remove(req, 1);
						mqtt_free(pkt);
						FREE(pkt);
						return;
					} break;
					case MQTT_SUBSCRIBE: {
						mqtt_pkt_suback(&pkt1, pkt->payload.subscribe.msgid, pkt->payload.subscribe.qos);
						if(mqtt_encode(&pkt1, &buf1, &len) == 0) {
							iobuf_append(&custom_poll_data->send_iobuf, (void *)buf1, len);
							FREE(buf1);
						}
						mqtt_subscribtion_add(req, pkt->payload.subscribe.topic, pkt->payload.subscribe.qos);
					} break;
					case MQTT_PINGREQ: {
						mqtt_pkt_pingresp(&pkt1);
						if(mqtt_encode(&pkt1, &buf1, &len) == 0) {
							iobuf_append(&custom_poll_data->send_iobuf, (void *)buf1, len);
							FREE(buf1);
						}
					} break;
					case MQTT_PUBACK: {
						mqtt_process_publish(req, pkt);
					} break;
					case MQTT_PUBREC: {
						mqtt_process_publish(req, pkt);
					} break;
					case MQTT_PUBCOMP: {
						mqtt_process_publish(req, pkt);
					} break;
					case MQTT_PUBLISH: {
						mqtt_topic_add(pkt->retain, pkt->payload.publish.topic, pkt->payload.publish.message);

						mqtt_process_publish(req, pkt);

						if(pkt->qos == 1) {
							mqtt_pkt_puback(&pkt1, pkt->payload.publish.msgid);
						}
					} break;
				}
			}

			if(client != NULL && client->side == CLIENT_SIDE) {
				switch(pkt->type) {
					case MQTT_CONNACK: {
						if(client->step == MQTT_CONNACK) {
							uv_timer_stop(client->timer_req);
							client->step = MQTT_CONNECTED;
							client->callback(client, pkt, client->userdata);
						} else {
							// error
						}
					} break;
					default: {
						client->callback(client, pkt, client->userdata);
					} break;
				}
			}
			mqtt_free(pkt);
			FREE(pkt);
		}
	}

#ifdef _WIN32
	uv_mutex_lock(&mqtt_lock);
#else
	pthread_mutex_lock(&mqtt_lock);
#endif
	struct mqtt_client_t *node = mqtt_clients;
	while(node) {
		if(node->side == SERVER_SIDE) {
			if(node->flush == 1) {
				uv_custom_write(node->poll_req);
				node->flush = 0;
			}
			if(node->poll_req == req) {
				if(node->keepalive > 0) {
					uv_update_time(uv_default_loop());
#ifdef PILIGHT_UNITTEST
					uv_timer_start(node->timer_req , mqtt_client_timeout, (node->keepalive*1.5)*100, 0);
#else
					uv_timer_start(node->timer_req , mqtt_client_timeout, (node->keepalive*1.5)*1000, 0);
#endif
				}
			}
		}
		node = node->next;
	}

#ifdef _WIN32
	uv_mutex_unlock(&mqtt_lock);
#else
	pthread_mutex_unlock(&mqtt_lock);
#endif

	uv_custom_write(req);
	uv_custom_read(req);
}

static void server_read_cb(uv_poll_t *req, ssize_t *nread, char *buf) {
	/*
	 * Make sure we execute in the main thread
	 */
	const uv_thread_t pth_cur_id = uv_thread_self();
	assert(uv_thread_equal(&pth_main_id, &pth_cur_id));

	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct sockaddr_in servaddr;
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
		logprintf(LOG_DEBUG, "client fd: %d", client);
	// }

#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(client, FIONBIO, &on);
#else
	long arg = fcntl(client, F_GETFL, NULL);
	fcntl(client, F_SETFL, arg | O_NONBLOCK);
#endif

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_custom_poll_init(&custom_poll_data, poll_req, NULL);

	custom_poll_data->read_cb = client_read_cb;
	custom_poll_data->write_cb = client_write_cb;
	custom_poll_data->close_cb = poll_close_cb;

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

	struct mqtt_client_t *node = NULL;
	mqtt_client_add(&node, poll_req, SERVER_SIDE);
	node->fd = client;

	uv_custom_read(req);
	uv_custom_read(poll_req);
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

	if((poll_server_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_custom_poll_init(&custom_poll_data, poll_server_req, (void *)NULL);
	custom_poll_data->is_ssl = 0;
	custom_poll_data->is_server = 1;
	custom_poll_data->read_cb = server_read_cb;

	r = uv_poll_init_socket(uv_default_loop(), poll_server_req, sockfd);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		FREE(poll_server_req);
		goto free;
		/*LCOV_EXCL_STOP*/
	}

	uv_custom_read(poll_server_req);

	logprintf(LOG_DEBUG, "started MQTT broker on port %d", port);

	struct lua_state_t *state = plua_get_free_state();
	if(state != NULL) {
		char *out = NULL;
		while(config_setting_get_string(state->L, "mqtt-blacklist", mqtt_blacklist_nr, &out) == 0) {
			if((mqtt_blacklist = REALLOC(mqtt_blacklist, (mqtt_blacklist_nr+1)*sizeof(char *))) == NULL) {
				OUT_OF_MEMORY
			}
			mqtt_blacklist[mqtt_blacklist_nr] = out;
			mqtt_blacklist_nr++;
		}

		out = NULL;
		while(config_setting_get_string(state->L, "mqtt-whitelist", mqtt_whitelist_nr, &out) == 0) {
			if((mqtt_whitelist = REALLOC(mqtt_whitelist, (mqtt_whitelist_nr+1)*sizeof(char *))) == NULL) {
				OUT_OF_MEMORY
			}
			mqtt_whitelist[mqtt_whitelist_nr] = out;
			mqtt_whitelist_nr++;
		}
		plua_clear_state(state);
	}

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

int mqtt_client(char *ip, int port, char *clientid, char *willtopic, char *willmsg, void (*callback)(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata), void *userdata) {
	if(willtopic != NULL && willmsg == NULL) {
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
	struct uv_custom_poll_t *custom_poll_data = NULL;
	struct sockaddr_in addr4;
	int sockfd = 0, r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	memset(&addr4, '\0', sizeof(struct sockaddr_in));
	r = uv_ip4_addr(ip, port, &addr4);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_ip4_addr: %s", uv_strerror(r));
		goto free;
		/*LCOV_EXCL_END*/
	}

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "socket: %s", strerror(errno));
		goto free;
		/*LCOV_EXCL_STOP*/
	}

#ifdef _WIN32
	unsigned long on = 1;
	ioctlsocket(sockfd, FIONBIO, &on);
#else
	long arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg | O_NONBLOCK);
#endif

	if(connect(sockfd, (struct sockaddr *)&addr4, sizeof(addr4)) < 0) {
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

	uv_poll_t *poll_req = NULL;
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	mqtt_client_add(&client, poll_req, CLIENT_SIDE);

	if((client->id = STRDUP(clientid)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

#ifdef PILIGHT_UNITTEST
	client->keepalive = 1;
#else
	client->keepalive = 60;
#endif

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

	client->step = MQTT_CONNECT;
	client->side = CLIENT_SIDE;
	client->callback = callback;
	client->userdata = userdata;
	client->fd = sockfd;

	uv_custom_poll_init(&custom_poll_data, client->poll_req, client);

	custom_poll_data->is_ssl = 0;
	custom_poll_data->write_cb = client_write_cb;
	custom_poll_data->read_cb = client_read_cb;
	custom_poll_data->close_cb = client_close_cb;

	r = uv_poll_init_socket(uv_default_loop(), client->poll_req, sockfd);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_poll_init_socket: %s", uv_strerror(r));
		FREE(client->poll_req);
		goto free;
		/*LCOV_EXCL_STOP*/
	}

	uv_custom_write(client->poll_req);

	return 0;

/*LCOV_EXCL_START*/
free:
	if(client != NULL) {
		if(client->timer_req != NULL) {
			uv_timer_stop(client->timer_req);
		}
	}
	if(custom_poll_data != NULL) {
		uv_custom_poll_free(custom_poll_data);
	}
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
