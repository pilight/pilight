/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _MQTT_H_
#define _MQTT_H_

#include <stdint.h>

#include "../../libuv/uv.h"
#include "eventpool.h"

struct mqtt_client_t;

typedef struct mqtt_message_t {
	char *payload;
	char *topic;
	int msgid;
	int step;
	int qos;

	struct mqtt_client_t *client;

	uv_timer_t *timer_req;

	struct mqtt_message_t *next;
} mqtt_message_t;

typedef struct mqtt_pkt_t {
	uint8_t type;
	uint8_t dub;
	uint8_t qos;
	uint8_t retain;
	uint8_t reserved;

	union {
		struct {
			char *protocol;
			int version;
			uint8_t username;
			uint8_t password;
			uint8_t willretain;
			uint8_t qos;
			uint8_t willflag;
			uint8_t cleansession;
			uint8_t keepalive;
			uint8_t reserved;
		} connect;
		struct {
			uint8_t reserved;
			uint8_t session;
		} connack;
	} header;
	union {
		struct {
			uint16_t keepalive;
			char *clientid;
			char *willtopic;
			char *willmessage;
			char *username;
			char *password;
		} connect;
		struct {
			uint8_t code;
		} connack;
		struct {
			char *topic;
			uint16_t msgid;
			char *message;
		} publish;
		struct {
			uint16_t msgid;
		} puback;
		struct {
			uint16_t msgid;
		} pubrec;
		struct {
			uint16_t msgid;
		} pubrel;
		struct {
			uint16_t msgid;
		} pubcomp;
		struct {
			uint16_t msgid;
			char *message;
			char *topic;
			uint8_t qos;
		} subscribe;
		struct {
			uint16_t msgid;
			uint8_t qos;
		} suback;
	} payload;
} mqtt_pkt_t;

typedef struct mqtt_subscription_t {
	char *topic;
	int qos;

	struct mqtt_subscription_t *next;
} mqtt_subscription_t;

typedef struct mqtt_client_t {
	uv_poll_t *poll_req;
	uv_async_t *async_req;
	uv_timer_t *ping_req;
	uv_timer_t *timeout_req;

	char *id;

	struct iobuf_t recv_iobuf;
	struct iobuf_t send_iobuf;
	struct mqtt_message_t messages[1024];
	struct mqtt_message_t *retain;
	struct mqtt_subscription_t *subscriptions;

	struct {
		char *message;
		char *topic;
		int qos;
		int retain;
	} will;

	int haswill;
	int msgid;
	int keepalive;

	int step;

	char *ip;
	int port;
	int fd;

	void *userdata;

	void (*callback)(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata);

	struct mqtt_client_t *next;
} mqtt_client_t;

enum mqtt_connection_t {
	MQTT_CONNECTION_ACCEPTED,
	MQTT_CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION,
	MQTT_CONNECTION_REFUSED_IDENTIFIER_REJECTED,
	MQTT_CONNECTION_REFUSED_SERVER_UNAVAILABLE,
	MQTT_CONNECTION_REFUSED_BAD_USERNAME_OR_PASSWORD,
	MQTT_CONNECTION_REFUSED_NOT_AUTHORIZED
};

#define MQTT_CONNECT       1
#define MQTT_CONNACK       2
#define MQTT_PUBLISH       3
#define MQTT_PUBACK        4
#define MQTT_PUBREC        5
#define MQTT_PUBREL        6
#define MQTT_PUBCOMP       7
#define MQTT_SUBSCRIBE     8
#define MQTT_SUBACK        9
#define MQTT_UNSUBSCRIBE   10
#define MQTT_UNSUBACK      11
#define MQTT_PINGREQ       12
#define MQTT_PINGRESP      13
#define MQTT_DISCONNECT 	 14
#define MQTT_CONNECTED     15
#define MQTT_DISCONNECTED  16

#define MQTT_QOS1						101
#define MQTT_QOS2						102
#define MQTT_DUB						103
#define MQTT_RETAIN					104

int mqtt_encode(struct mqtt_pkt_t *pkt, unsigned char **buf, unsigned int *len);
int mqtt_decode(struct mqtt_pkt_t **pkt, unsigned char *buf, unsigned int len, unsigned int *pos);

int mqtt_pkt_connect(struct mqtt_pkt_t *pkt, int dub, int qos, int retain, char *clientid, char *protocol, int version, char *username, char *password, int willretain, int willflag, int cleansession, int keepalive, char *topic, char *message);
int mqtt_pkt_connack(struct mqtt_pkt_t *pkt, enum mqtt_connection_t status);
int mqtt_pkt_publish(struct mqtt_pkt_t *pkt, int dub, int qos, int retain, char *topic, int msgid, char *message);
int mqtt_pkt_puback(struct mqtt_pkt_t *pkt, int msgid);
int mqtt_pkt_pubrec(struct mqtt_pkt_t *pkt, int msgid);
int mqtt_pkt_pubrel(struct mqtt_pkt_t *pkt, int msgid);
int mqtt_pkt_pubcomp(struct mqtt_pkt_t *pkt, int msgid);
int mqtt_pkt_subscribe(struct mqtt_pkt_t *pkt, char *topic, int msgid, int qos);
int mqtt_pkt_suback(struct mqtt_pkt_t *pkt, int msgid, int qos);
int mqtt_pkt_pingreq(struct mqtt_pkt_t *pkt);
int mqtt_pkt_pingresp(struct mqtt_pkt_t *pkt);
int mqtt_pkt_disconnect(struct mqtt_pkt_t *pkt);

int mosquitto_topic_matches_sub(const char *sub, const char *topic, int *result);
int mosquitto_sub_topic_check(const char *str);

int mqtt_subscribe(struct mqtt_client_t *client, char *topic, int qos);
int mqtt_publish(struct mqtt_client_t *client, int dub, int qos, int retain, char *topic, char *message);
int mqtt_suback(struct mqtt_client_t *client, int msgid, int qos);
int mqtt_puback(struct mqtt_client_t *client, int msgid);
int mqtt_pubrel(struct mqtt_client_t *client, int msgid);
int mqtt_pubrec(struct mqtt_client_t *client, int msgid);
int mqtt_pubcomp(struct mqtt_client_t *client, int msgid);
int mqtt_ping(struct mqtt_client_t *client);
int mqtt_pingresp(struct mqtt_client_t *client);
int mqtt_disconnect(struct mqtt_client_t *client);
#ifdef PILIGHT_UNITTEST
void mqtt_activate(void);
#endif

void mqtt_free(struct mqtt_pkt_t *pkt);
void mqtt_dump(struct mqtt_pkt_t *pkt);

int mqtt_client(char *server, int port, char *clientid, char *willtopic, char *willmsg, void (*callback)(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata), void *userdata);
#ifdef MQTT
int mqtt_server(int port);
#endif

int mqtt_server_gc(void);
int mqtt_client_gc(void);

#endif