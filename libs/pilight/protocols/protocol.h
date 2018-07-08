/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#ifndef _WIN32
	#ifdef __mips__
		#ifndef __USE_UNIX98
			#define __USE_UNIX98
		#endif
	#endif
	#include <pthread.h>
#endif

#include "defines.h"
#include "../core/options.h"
#include "../core/json.h"
#include "../hardware/hardware.h"

typedef enum {
	PROCESS = -1,
	RAW = 0,
	SWITCH,
	DIMMER,
	WEATHER,
	RELAY,
	SCREEN,
	CONTACT,
	PENDINGSW,
	DATETIME,
	XBMC,
	LIRC,
	WEBCAM,
	MOTION,
	DUSK,
	PING,
	LABEL,
	ALARM
} devtype_t;

typedef struct protocol_devices_t {
	char *id;
	char *desc;
	struct protocol_devices_t *next;
} protocol_devices_t;

typedef struct protocol_threads_t {
	// pthread_mutex_t mutex;
	// pthread_cond_t cond;
	// pthread_mutexattr_t attr;
	JsonNode *param;
	struct protocol_threads_t *next;
} protocol_threads_t;

typedef struct protocol_t {
	char *id;
	int rawlen;
	int minrawlen;
	int maxrawlen;
	int mingaplen;
	int maxgaplen;
	short txrpt;
	short rxrpt;
	short multipleId;
	short config;
	short masterOnly;
	struct options_t *options;

	int repeats;
	unsigned long first;
	unsigned long second;

	int *raw;

	hwtype_t hwtype;
	devtype_t devtype;
	struct protocol_devices_t *devices;
	struct protocol_threads_t *threads;

	union {
		void (*parseCode)(char **message);
		void (*parseCommand)(struct JsonNode *code, char **message);
	};
	int (*validate)(void);
	int (*createCode)(struct JsonNode *code, char **message);
	int (*checkValues)(struct JsonNode *code);
	void (*initDev)(struct JsonNode *device);
	void (*printHelp)(void);
	void (*gc)(void);
	// void (*threadGC)(void);
} protocol_t;

typedef struct protocols_t {
	struct protocol_t *listener;
	char *name;
	struct protocols_t *next;
} protocols_;

extern struct protocols_t *protocols;

void protocol_init(void);
struct protocol_threads_t *protocol_thread_init(protocol_t *proto, struct JsonNode *param);
int protocol_thread_wait(struct protocol_threads_t *node, int interval, int *nrloops);
void protocol_thread_free(protocol_t *proto);
void protocol_thread_stop(protocol_t *proto);
void protocol_set_id(protocol_t *proto, const char *id);
void protocol_plslen_add(protocol_t *proto, int plslen);
void protocol_register(protocol_t **proto);
void protocol_device_add(protocol_t *proto, const char *id, const char *desc);
int protocol_device_exists(protocol_t *proto, const char *id);
int protocol_gc(void);

int protocol_get_type(char *, int *);
int protocol_get_state(char *, char **);
int protocol_is_state(char *, char *);
int protocol_get_value(char *, char *, char **);
int protocol_get_number_setting(char *, char *, int *);
int protocol_get_string_setting(char *, char *, char **);
int protocol_has_parameter(char *, char *);

#endif
