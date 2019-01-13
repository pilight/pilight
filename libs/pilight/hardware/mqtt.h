/*
	Copyright (C) 2019 CurlyMo & Niek

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _HARDWARE_MQTT_H_
#define _HARDWARE_MQTT_H_

//MQTT request types:

#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define PUBACK 4
#define PUBREC 5
#define PUBREL 6
#define PUBCOMP 7
#define SUBSCRIBE 8
#define SUBACK 9
#define UNSUBSCRIBE 10
#define UNSUBACK 11
#define PINGREQ 12
#define PINGRESP 13
#define DISCONNECT 14
#define GETMESSAGE 99

//MQTT processing step
#define MQTT_STEP_NONE					0
#define MQTT_STEP_SEND_CONNECT			1
#define MQTT_STEP_RECV_CONNACK			2
#define MQTT_STEP_SEND_PUBLISH			3
#define MQTT_STEP_RECV_PUBACK			4
#define MQTT_STEP_SEND_SUBSCRIBE		8
#define MQTT_STEP_RECV_SUBACK			9
#define MQTT_STEP_SEND_UNSUBSCRIBE		10
#define MQTT_STEP_RECV_UNSUBACK			11
#define MQTT_STEP_SEND_PINGREQ			12
#define MQTT_STEP_RECV_PINGRESP			13
#define MQTT_STEP_SEND_DISCONNECT		14

#define MQTT_STEP_RECV_PUBLISH			103 //for receiving messages
#define MQTT_STEP_SEND_PUBACK			104 //for receiving messages

#define MQTT_STEP_END					100

typedef struct mqtt_client_t {
	char *clientid;
	char *host;
	int is_ssl;
	int port;
	char *user;
	char *pass;
	char *topic;
	int qos;
	int keepalive;
	int fd;
	int reqtype;
	int step;
	char *packet;
	int packet_len;
	int sent_packetid;
	int recvd_packetid;
	int connected;
	int subscribed;
	int update_pending;
	int authtype;
	int reading;
	int sending;
	int status;
	char *message;
	int msg_pending;
	uv_timer_t *timer_req;
	uv_poll_t *poll_req;
	int wait;
	void *userdata;
	size_t bytes_read;
	void (*callback)(int, char *, char *, void *userdata);
	int called;

} mqtt_client_t;

typedef struct mqtt_clients_t {
	uv_poll_t *req;
	int fd;
	struct uv_custom_poll_t *data;
	struct mqtt_clients_t *next;
} mqtt_clients_t;

struct hardware_t *mqtt;
void mqttInit(void);

void mqtt_process(int, char *, char *, int, char *, char *, char *, int, char *, int, void (*)(int, char *, char *, void *userdata), void *userdata);

void mqtt_sub(char *, char *, int, char *, char *, char *, int, int, void (*)(int, char *, char *, void *userdata), void *userdata);
void mqtt_pub(char *, char *, int, char *, char *, char *, int, char *, int, void (*)(int, char *, char *, void *userdata), void *userdata);
#endif
