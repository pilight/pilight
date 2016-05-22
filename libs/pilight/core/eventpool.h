/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EVENTPOOL_H_
#define _EVENTPOOL_H_

#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "eventpool_structs.h"

enum eventpool_threads_t {
	EVENTPOOL_NO_THREADS,
	EVENTPOOL_THREADED
} eventpool_threads_t;

#define EVENTPOOL_STAGE_SOCKET 						0
#define EVENTPOOL_STAGE_CONNECT 					1
#define EVENTPOOL_STAGE_CONNECTING				2
#define EVENTPOOL_STAGE_CONNECTED					3
#define EVENTPOOL_STAGE_BIND							4
#define EVENTPOOL_STAGE_LISTEN						5
#define EVENTPOOL_STAGE_LISTENING					6
#define EVENTPOOL_STAGE_READ							7
#define EVENTPOOL_STAGE_WRITE							8
#define EVENTPOOL_STAGE_DISCONNECTED			9
#define EVENTPOOL_STAGE_DISCONNECT				10

#define EVENTPOOL_TYPE_SOCKET_CLIENT			0
#define EVENTPOOL_TYPE_SOCKET_SERVER			1
#define EVENTPOOL_TYPE_SOCKET_SSL_CLIENT	2
#define EVENTPOOL_TYPE_SOCKET_SSL_SERVER	3
#define EVENTPOOL_TYPE_IO									4

#define EV_SOCKET_SUCCESS						0
#define EV_SOCKET_FAILED						1
#define EV_CONNECT_SUCCESS					2
#define EV_CONNECT_FAILED						3
#define EV_BIND_SUCCESS							4
#define EV_BIND_FAILED							5
#define EV_LISTEN_SUCCESS						6
#define EV_LISTEN_FAILED						7
#define EV_READ											8
#define EV_WRITE										9
#define EV_HIGHPRI									10
#define EV_IDLE											11
#define EV_DISCONNECTED							12
// #define EV_POLL											13

#define REASON_SEND_CODE							0
#define REASON_CODE_SENT							1
#define REASON_CODE_RECEIVED					2
#define REASON_RECEIVED_PULSETRAIN		3
#define REASON_BROADCAST							4
#define REASON_BROADCAST_CORE					5
#define REASON_FORWARD								6
#define REASON_CONFIG_UPDATE					7
#define REASON_SOCKET_RECEIVED				8
#define REASON_SOCKET_DISCONNECTED		9
#define REASON_SOCKET_CONNECTED				10
#define REASON_SOCKET_SEND						11
#define REASON_SSDP_RECEIVED					12
#define REASON_SSDP_RECEIVED_FREE			13
#define REASON_SSDP_DISCONNECTED			14
#define REASON_SSDP_CONNECTED					15
#define REASON_WEBSERVER_CONNECTED		16
#define REASON_DEVICE_ADDED						17
#define REASON_ADHOC_MODE							18
#define REASON_ADHOC_CONNECTED				19
#define REASON_ADHOC_CONFIG_RECEIVED	20
#define REASON_ADHOC_DATA_RECEIVED		21
#define REASON_ADHOC_UPDATE_RECEIVED	22
#define REASON_ADHOC_DISCONNECTED			23
#define REASON_SEND_BEGIN							24
#define REASON_SEND_END								25
#define REASON_LOG										26
#define REASON_END										27

typedef struct eventpool_listener_t {
	void *(*func)(void *);
	void *userdata;
	int reason;
	struct eventpool_listener_t *next;
} eventpool_listener_t;

// IO buffers interface
typedef struct eventpool_iobuf_t {
  char *buf;
  size_t len;
  size_t size;
	pthread_mutex_t lock;
} iobuf_t;

typedef struct eventpool_fd_t {
	char *name;
	int fd;
	int idx;
	int active;

	int type;
	int stage;
	int error;
	int remove;
	int doread;
	int dowrite;
	int doflush;
	int dohighpri;
	void *userdata;

	int timer;
	int steps;
	char *buffer;
	size_t len;

	union {
		struct {
			char *server;
			char ip[INET_ADDRSTRLEN+1];
			unsigned int port;
			unsigned short domain;
			unsigned short type;
			unsigned short protocol;
			struct sockaddr_in addr;
		} socket;
	} data;

	int (*callback)(struct eventpool_fd_t *, int);
	int (*send)(struct eventpool_fd_t *);

  struct eventpool_iobuf_t recv_iobuf;
  struct eventpool_iobuf_t send_iobuf;

	struct eventpool_fd_t *next;
} eventpool_fd_t;

int eventpool_nrlisteners(int);
void eventpool_callback(int, void *(*func)(void *));
void eventpool_trigger(int, void *(*)(void *), void *);

int eventpool_fd_select(int, struct eventpool_fd_t **);
void eventpool_fd_enable_write(struct eventpool_fd_t *);
void eventpool_fd_enable_read(struct eventpool_fd_t *);
void eventpool_fd_enable_highpri(struct eventpool_fd_t *);
void eventpool_fd_disable_write(struct eventpool_fd_t *);
void eventpool_fd_disable_read(struct eventpool_fd_t *);
void eventpool_fd_disable_highpri(struct eventpool_fd_t *);
void eventpool_socket_reconnect(struct eventpool_fd_t *);
int eventpool_fd_write(int, char *, unsigned long);
void eventpool_fd_remove(struct eventpool_fd_t *);
struct eventpool_fd_t *eventpool_socket_add(char *, char *, unsigned int, int, int, int, int, int (*)(struct eventpool_fd_t *, int), int (*)(struct eventpool_fd_t *), void *);
struct eventpool_fd_t *eventpool_fd_add(char *, int, int (*)(struct eventpool_fd_t *, int), int (*)(struct eventpool_fd_t *), void *);
void *eventpool_process(void *);
void eventpool_init(enum eventpool_threads_t);
enum eventpool_threads_t eventpool_threaded(void);
int eventpool_gc(void);

#endif
