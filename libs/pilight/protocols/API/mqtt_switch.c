/*
	Copyright (C) 2019 CurlyMo & Niek

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <regex.h>
#include <sys/stat.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../../libuv/uv.h"
#include "../../core/threads.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/datetime.h" // Full path because we also have a datetime protocol
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "mqtt_switch.h"
#include "../../hardware/mqtt.h"

#define INTERVAL	1

#define MIN_DELAY 1000
#define MAX_DELAY 10000

typedef struct settings_t {
	double id;
	char *pub_clientid;
	char *pub_host;
	int pub_port;
	char *pub_user;
	char *pub_pass;
	char *pub_topic;
	char *sub_clientid;
	char *sub_host;
	int sub_port;
	char *sub_user;
	char *sub_pass;
	char *sub_topic;
	int qos;
	int keepalive;
	int fd;
	int reqtype;
	uv_timer_t *send_timer_req;
	int send_delay;
	uv_timer_t *receive_timer_req;
	int receive_delay;
	struct settings_t *next;
} settings_t;

static pthread_mutex_t lock;
static pthread_mutexattr_t attr;

static struct settings_t *settings;
struct mqtt_pub_client_t *mqtt_client;

static unsigned short loop = 1;
static unsigned short threads = 0;

static void send_callback(int, char *, char *, void *userdata);
static void receive_callback(int, char *, char *, void *userdata);

static void createMessage(int id, int state) {
	mqtt_switch->message = json_mkobject();
	json_append_member(mqtt_switch->message, "id", json_mknumber(id, 0));
	if(state == 1) {
		json_append_member(mqtt_switch->message, "state", json_mkstring("on"));
	} else {
		json_append_member(mqtt_switch->message, "state", json_mkstring("off"));
	}
}

static void receive_register_wait(uv_timer_t *req) {
	struct settings_t *wnode = req->data;
	if(wnode->receive_timer_req != NULL) {
		uv_timer_stop(wnode->receive_timer_req);
	}
	mqtt_sub(wnode->sub_clientid, wnode->sub_host, wnode->sub_port, wnode->sub_user, wnode->sub_pass, wnode->sub_topic, wnode->qos, 0, receive_callback, wnode);
}

static void receive_callback(int status, char *message, char *topic, void *userdata) {
	struct settings_t *wnode = userdata;
	
	switch(status) {
		case 0: { //connect/subscribe succes
			if(wnode->receive_timer_req != NULL) {
				uv_timer_stop(wnode->receive_timer_req);
				wnode->receive_delay = MIN_DELAY;
			}
		} break;

		case 1: { // message received
			wnode->receive_delay = MIN_DELAY;
			logprintf(LOG_DEBUG, "mqtt_switch: received message \"%s\" for \"%s\"", message, wnode->sub_clientid);
			if(strcmp(message, "on") == 0 || strcmp(message, "off") == 0) {
				struct JsonNode *node = json_mkobject();
				JsonNode *code = json_mkobject();
				json_append_member(code, "id", json_mknumber(wnode->id, 0));
				json_append_member(code, "state", json_mkstring(message));
				json_append_member(node, "message", code);
				json_append_member(node, "origin", json_mkstring("receiver"));
				json_append_member(node, "protocol", json_mkstring(mqtt_switch->id));
				if(pilight.broadcast != NULL) {
					pilight.broadcast(mqtt_switch->id, node, PROTOCOL);
				}
				json_delete(node);
				node = NULL;
			} else {
				logprintf(LOG_NOTICE, "mqtt_switch: client \"%s\" expected \"on\" or \"off\", but received \"%s\"", wnode->sub_clientid, message);
			}
		} break;
		
		case -3: {
			logprintf(LOG_ERR, "mqtt module not installed");
			exit(EXIT_FAILURE);
		} break;

		case -2: { // Connection lost
			logprintf(LOG_NOTICE, "mqtt_switch: connection for \"%s\" lost, trying to register again in %d seconds", wnode->sub_clientid, wnode->receive_delay / 1000);
			wnode->receive_delay *= 2;
			if(wnode->receive_delay > MAX_DELAY) {
				wnode->receive_delay = MAX_DELAY;
			}
		}
 	}
 }

 static void send_callback(int status, char *message, char *topic, void *userdata) {
	struct settings_t *wnode = userdata;

	switch(status) {
		case 0: { //connect succes
		} break;

		case -3: { //hardware module not ready
			logprintf(LOG_ERR, "mqtt_switch: mqtt module not ready");
			exit(EXIT_FAILURE);
		} break;

		case -2: { //connection lost, will automatcally try to reconnect whit next publish attempt
			logprintf(LOG_NOTICE, "mqtt_switch: connection for client \"%s\" lost, message not published!", wnode->pub_clientid);
		}
	}
}

static int createCode(JsonNode *code) { //publish state changes
	struct settings_t *wtmp = settings;
	char *message = "off";
	int id = -1;
	int state = -1;
	double itmp = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);

	if(json_find_number(code, "off", &itmp) == 0)
		state = 0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state = 1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "mqtt_switch: insufficient number of arguments");
		return EXIT_FAILURE;

		} else {

		while(wtmp) {
			if((int)round(wtmp->id) == id) {

				if (state == 1) {
					message = MALLOC(4);
					if(!message) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(message, "on");
				}
				logprintf(LOG_DEBUG, "mqtt_switch: \"%s\" publishing message \"%s\" for topic \"%s\"", wtmp->pub_clientid, message, wtmp->pub_topic);
				mqtt_pub(wtmp->pub_clientid, wtmp->pub_host, wtmp->pub_port, wtmp->pub_user, wtmp->pub_pass, wtmp->pub_topic, wtmp->qos, message, 0, send_callback, wtmp);
			}
			wtmp = wtmp->next;
		}
		createMessage(id, state);
	}

	return EXIT_SUCCESS;
}

static void threadGC(void) {
	loop = 0;
	protocol_thread_stop(mqtt_switch);
	while(threads > 0) {
		usleep(10);
	}
	protocol_thread_free(mqtt_switch);

	struct settings_t *wtmp = NULL;
	while(settings) {
		wtmp = settings;
		settings = settings->next;
		FREE(wtmp);
	}
	if(settings != NULL) {
		FREE(settings);
	}
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	json_free(output);
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	double itmp = 0;

	struct settings_t *wnode = MALLOC(sizeof(struct settings_t));

	wnode->pub_host = NULL, wnode->pub_user = NULL, wnode->pub_pass = NULL;
	wnode->sub_host = NULL, wnode->sub_user = NULL, wnode->sub_pass = NULL;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		if(json_find_number(jchild, "id", &itmp) == 0) {
			wnode->id = itmp;
		}
	}
	json_find_string(json, "pub_host", &wnode->pub_host);
	itmp = 0;
	json_find_number(json, "pub_port", &itmp);
	wnode->pub_port = (int)round(itmp);	
	json_find_string(json, "pub_user", &wnode->pub_user);
	json_find_string(json, "pub_pass", &wnode->pub_pass);
	json_find_string(json, "pub_topic", &wnode->pub_topic);

	json_find_string(json, "sub_host", &wnode->sub_host);
	itmp = 0;
	json_find_number(json, "sub_port", &itmp);
	wnode->sub_port = (int)round(itmp);	
	json_find_string(json, "sub_user", &wnode->sub_user);
	json_find_string(json, "sub_pass", &wnode->sub_pass);
	json_find_string(json, "sub_topic", &wnode->sub_topic);

	json_find_number(json, "qos", &itmp);
	wnode->qos = (int)round(itmp);
	wnode->next = settings;
	settings = wnode;

	if((wnode->sub_clientid = MALLOC(20)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	sprintf(wnode->sub_clientid, "%s_%i_sub", mqtt_switch->id, (int)round(wnode->id));

	if((wnode->pub_clientid = MALLOC(20)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	sprintf(wnode->pub_clientid, "%s_%i_pub", mqtt_switch->id, (int)round(wnode->id));
	wnode->receive_delay = MIN_DELAY;
	wnode->receive_timer_req = NULL;
	if((wnode->receive_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	wnode->receive_timer_req->data = wnode;
	uv_timer_init(uv_default_loop(), wnode->receive_timer_req);

	logprintf(LOG_DEBUG, "mqtt_switch: registering clientid \"%s\" for suscribe in 1 second", wnode->sub_clientid);
	uv_timer_start(wnode->receive_timer_req, (void (*)(uv_timer_t *))receive_register_wait, 1000, 0);//give hardware module time to initialize before trying to subscribe

	return NULL;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void mqttSwitchInit(void) {
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &attr);

	protocol_register(&mqtt_switch);
	protocol_set_id(mqtt_switch, "mqtt_switch");
	protocol_device_add(mqtt_switch, "mqtt_switch", "Switch with two way mqtt support");
	mqtt_switch->devtype = SWITCH;
	mqtt_switch->hwtype = API;
	mqtt_switch->multipleId = 0;
	mqtt_switch->masterOnly = 0;
	
	options_add(&mqtt_switch->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,})$");
	options_add(&mqtt_switch->options, "ph", "pub_host", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "pp", "pub_port", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);
	options_add(&mqtt_switch->options, "pu", "pub_user", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "ps", "pub_password", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "pt", "pub_topic", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "sh", "sub_host", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "sp", "sub_port", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);
	options_add(&mqtt_switch->options, "su", "sub_user", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "ss", "sub_password", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "st", "sub_topic", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&mqtt_switch->options, "q", "qos", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, NULL, NULL);

	options_add(&mqtt_switch->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&mqtt_switch->options, "0", "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	mqtt_switch->createCode=&createCode;
	mqtt_switch->initDev=&initDev;
	mqtt_switch->threadGC=&threadGC;
	mqtt_switch->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "mqtt_switch";
	module->version = "1.0";
	module->reqversion = "8.1.3";
	module->reqcommit = "";
}

void init(void) {
	mqttSwitchInit();
}
#endif
