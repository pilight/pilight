/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <windows.h>
	#include <ws2tcpip.h>
	#include <winsvc.h>
	#include <shellapi.h>
	#define MSG_NOSIGNAL 0
#else
	#include <unistd.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>

#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/eventpool.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/proc.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/json.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/ntp.h"
#include "libs/pilight/core/ssl.h"
#include "libs/pilight/config/settings.h"

#include "libs/pilight/core/mail.h"

#ifdef EVENTS
	#include "libs/pilight/events/events.h"
	#include "libs/pilight/events/action.h"
	#include "libs/pilight/events/function.h"
	#include "libs/pilight/events/operator.h"
#endif

#ifdef WEBSERVER
	#ifdef WEBSERVER_HTTPS
		#include <mbedtls/md5.h>
	#endif
	#include "libs/pilight/core/webserver.h"
#endif

#include "libs/pilight/hardware/hardware.h"
#include "libs/pilight/protocols/protocol.h"

#include <wiringx.h>

/*
 * Daemon unit test
 * - Client connect + client close
 * - Client connect + daemon stop
 * - Client connect + early disconnect
 * - http to socket port
 
 */

#ifndef _WIN32
static char *pid_file;
#endif
 
static uv_signal_t **signal_req = NULL;
static int signals[5] = { SIGINT, SIGQUIT, SIGTERM, SIGABRT, SIGTSTP };
static uv_timer_t *timer_abort_req = NULL;
static uv_timer_t *timer_stats_req = NULL;

#ifdef WEBSERVER
static int webserver_enable = WEBSERVER_ENABLE;
static int webserver_http_port = WEBSERVER_HTTP_PORT;
static int webserver_https_port = WEBSERVER_HTTPS_PORT;
static char *webserver_root = NULL;
#endif

static int active = 1;
static int nodaemon = 0;
static int main_loop = 1;
static int verbosity = LOG_INFO;
static int sockfd = 0;
static char *master_server = NULL;
static unsigned short master_port = 0;

struct timestamp_t {
	struct timespec first;
	struct timespec second;
}	timestamp_t;

struct timestamp_t timestamp;

typedef struct clients_t {
	char uuid[UUID_LENGTH];
	int id;
	int receiver;
	int config;
	int core;
	int stats;
	int forward;
	char media[8];
	double cpu;
	struct clients_t *next;
} clients_t;

static struct clients_t *clients = NULL;

static void *reason_code_sent_free(void *param) {
	struct reason_code_sent_t *data = param;
	FREE(data);
	return NULL;
}

// static void *reason_send_code_free(void *param) {
	// struct reason_send_code_t *data = param;
	// FREE(data);
	// return NULL;
// }

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_forward_free(void *param) {
	FREE(param);
	return NULL;
}

static void *reason_socket_send_free(void *param) {
	struct reason_socket_send_t *data = param;
	FREE(data->buffer);
	FREE(data);
	return NULL;
}

static void *reason_broadcast_free(void *param) {
	char *code = param;
	FREE(code);
	return NULL;
}

static void *reason_socket_received_free(void *param) {
	struct reason_socket_received_t *data = param;
	FREE(data->buffer);
	FREE(data);
	return NULL;
}

static void *reason_broadcast_core_free(void *param) {
	char *code = param;
	FREE(code);
	return NULL;
}

static void *broadcast(int reason, void *param, void *userdata) {	
	char /**threadid = NULL, */ *protocol = NULL, *origin = NULL;
	char *message = NULL, *uuid = NULL, *settings = NULL, *conf = NULL;
	char internal[1025];
	int repeats = 0, type = 0, len = 0, len1 = 0;
	int broadcasted = 0, free_conf = 1, komma = 0;

	if((conf = MALLOC(1025)) == NULL) {
		OUT_OF_MEMORY
	}

	switch(reason) {
		case REASON_CODE_RECEIVED: {
			struct reason_code_received_t *data = param;
			protocol = data->protocol;
			origin = data->origin;
			message = data->message;
			uuid = data->uuid;
			repeats = data->repeat;
			len = snprintf(conf, 1024, "{\"message\":%s,\"origin\":\"%s\",\"protocol\":\"%s\",\"uuid\":\"%s\",\"repeats\":%d}",
				message, origin, protocol, uuid, repeats);
		} break;
		case REASON_CODE_SENT: {
			struct reason_code_sent_t *data = param;
			type = data->type;
			if(strlen(data->protocol) > 0) {
				protocol = data->protocol;
			}
			if((message = STRDUP(data->message)) == NULL) {
				OUT_OF_MEMORY
			}
			if(strlen(data->uuid) > 0) {
				uuid = data->uuid;
			}
			repeats = data->repeat;
			/*if(strlen(data->settings) > 0) {
				settings = data->settings;
			}*/
			origin = data->origin;
			/* The settings objects inside the broadcast queue is only of interest for the
			 internal pilight functions. For the outside world we only communicate the
			 message part of the queue so we remove the settings */
			int len2 = strlen(message);
			if(message[0] != '{') {
				len1 += snprintf(&internal[len1], 1024-len1, "{");
				len += snprintf(&conf[len], 1024-len, "{");
			}
			if(message[len2-1] == '}') {
				message[len2-1] = '\0';
			}

			if(message != NULL) {
				len1 += snprintf(&internal[len1], 1024-len1, "%s", message);
				len += snprintf(&conf[len], 1024-len, "%s", message);
				komma = 1;
			}
			if(protocol != NULL) {
				if(komma == 1) {
					len1 += snprintf(&internal[len1], 1024-len1, ",");
					len += snprintf(&conf[len], 1024-len, ",");
				}
				len1 += snprintf(&internal[len1], 1024-len1, "\"protocol\":\"%s\"", protocol);
				len += snprintf(&conf[len], 1024-len, "\"protocol\":\"%s\"", protocol);
				komma = 1;
			}
			if(uuid != NULL) {
				if(komma == 1) {
					len1 += snprintf(&internal[len1], 1024-len1, ",");
					len += snprintf(&conf[len], 1024-len, ",");
				}
				len1 += snprintf(&internal[len1], 1024-len1, "\"uuid\":\"%s\"", uuid);
				len += snprintf(&conf[len], 1024-len, "\"uuid\":\"%s\"", uuid);
				komma = 1;
			}
			if(repeats > 0) {
				if(komma == 1) {
					len1 += snprintf(&internal[len1], 1024-len1, ",");
					len += snprintf(&conf[len], 1024-len, ",");
				}
				len1 += snprintf(&internal[len1], 1024-len1, "\"repeats\":%d", repeats);
				len += snprintf(&conf[len], 1024-len, "\"repeats\":%d}", repeats);
				komma = 1;
			} else {
				len += snprintf(&conf[len], 1024-len, "}");
			}

			if(settings != NULL) {
				if(komma == 1) {
					len1 += snprintf(&internal[len1], 1024-len1, ",");
					len += snprintf(&conf[len], 1024-len, ",");
				}
				len1 += snprintf(&internal[len1], 1024-len1, "\"settings\":%s}", settings);
				komma = 1;
			} else {
				len1 += snprintf(&internal[len1], 1024-len1, "}");
			}
			FREE(message);
		} break;
		default: {
			return NULL;
		} break;
	}

	broadcasted = 0;

	if(origin != NULL && strcmp(origin, "core") == 0) {
		struct clients_t *tmp_clients = clients;
		while(tmp_clients) {
			if((type < 0 && tmp_clients->core == 1) ||
				 (type >= 0 && tmp_clients->config == 1) ||
				 (type == PROCESS && tmp_clients->stats == 1)) {
				socket_write(tmp_clients->id, conf);
				broadcasted = 1;
			}
			tmp_clients = tmp_clients->next;
		}
		if(pilight.runmode == ADHOC && sockfd > 0) {
			char ret[1024];
			len = strlen(conf);
			strcpy(ret, conf);
			strcpy(&ret[len-1], ",\"action\":\"update\"}");
			socket_write(sockfd, ret);
			broadcasted = 1;
		}
		logprintf(LOG_DEBUG, "broadcasted: %s", conf);
		free_conf = 0;
		eventpool_trigger(REASON_BROADCAST_CORE, reason_broadcast_core_free, conf);
	} else {
		broadcasted = 0;

		/* Write the message to all receivers */
		struct clients_t *tmp_clients = clients;
		while(tmp_clients) {
			if(tmp_clients->receiver == 1 && tmp_clients->forward == 0) {
					// if(strcmp(conf, "{}") != 0 && nrchilds > 1) {
						socket_write(tmp_clients->id, conf);
						broadcasted = 1;
					// }
			}
			tmp_clients = tmp_clients->next;
		}

		if(pilight.runmode == ADHOC && sockfd > 0) {
			char ret[1024];
			strcpy(ret, conf);
			strcpy(&ret[len-1], ",\"action\":\"update\"}");
			socket_write(sockfd, ret);
			broadcasted = 1;
		}
		if((broadcasted == 1 || nodaemon == 1) /*&& (strcmp(c, "{}") != 0 && nrchilds > 1)*/) {
			logprintf(LOG_DEBUG, "broadcasted: %s", conf);
			free_conf = 0;
			eventpool_trigger(REASON_BROADCAST, reason_broadcast_free, conf);
		}
	}

	if(free_conf == 1) {
		FREE(conf);
	}
	
	return NULL;
}

static void receiver_create_message(struct protocol_t *protocol, char *message) {
	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(data->message, message);
	strcpy(data->origin, "receiver");
	data->protocol = protocol->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = protocol->repeats;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
}

static void receive_parse_code(int *raw, int rawlen, int plslen, int hwtype) {
	struct protocol_t *protocol = NULL;
	struct protocols_t *pnode = protocols;
	struct timeval tv;
	char message[255];

	while(pnode != NULL && main_loop == 1) {
		protocol = pnode->listener;

		if((protocol->hwtype == hwtype || protocol->hwtype == -1 || hwtype == -1) &&
			 (protocol->parseCode != NULL && protocol->validate != NULL)) {

			if(rawlen < MAXPULSESTREAMLENGTH) {
				protocol->raw = raw;
			}
			protocol->rawlen = rawlen;

			if(protocol->validate() == 0) {
				logprintf(LOG_DEBUG, "possible %s protocol", protocol->id);
				gettimeofday(&tv, NULL);
				if(protocol->first > 0) {
					protocol->first = protocol->second;
				}
				protocol->second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;
				if(protocol->first == 0) {
					protocol->first = protocol->second;
				}

				/* Reset # of repeats after a certain delay */
				if(((int)protocol->second-(int)protocol->first) > 500000) {
					protocol->repeats = 0;
				}

				protocol->repeats++;
				if(protocol->parseCode != NULL) {
					logprintf(LOG_DEBUG, "recevied pulse length of %d", plslen);
					logprintf(LOG_DEBUG, "caught minimum # of repeats %d of %s", protocol->repeats, protocol->id);
					logprintf(LOG_DEBUG, "called %s parseRaw()", protocol->id);
					memset(message, 0, 255);
					// protocol->parseCode(message);
					receiver_create_message(protocol, message);
				}
			}
		}
		pnode = pnode->next;
	}

	return;
}

static void *receivePulseTrain(int reason, void *param, void *userdata) {
	struct reason_received_pulsetrain_t *data = param;
	struct hardware_t *hw = NULL;
	int plslen = 0;

	if(data->hardware != NULL && data->pulses != NULL && data->length > 0) {
		if(hardware_select_struct(ORIGIN_MASTER, data->hardware, &hw) == 0) {
			plslen = data->pulses[data->length-1]/PULSE_DIV;
			if(data->length > 0) {
				receive_parse_code(data->pulses, data->length, plslen, hw->hwtype);
			}
		}
	}

	return (void *)NULL;
}

static void *code_sent_success(int reason, void *param, void *userdata) {
	struct reason_code_sent_success_t *data = param;

	logprintf(LOG_DEBUG, "successfully sent code %s from %s", data->message, data->uuid);

	return NULL;
}

static void *code_sent_fail(int reason, void *param, void *userdata) {
	struct reason_code_sent_fail_t *data = param;

	logprintf(LOG_DEBUG, "failed to sent code %s from %s", data->message, data->uuid);

	return NULL;
}

	// int origin;
	// char message[1025];
	// int rawlen;
	// int txrpt;
	// int pulses[MAXPULSESTREAMLENGTH+1];
	// char protocol[256];
	// int hwtype;
	// char settings[1025];
	// char uuid[UUID_LENGTH+1];

void *send_code(int reason, void *param, void *userdata) {
	struct reason_send_code_t *data = param;

	struct reason_code_sent_t *data1 = NULL;
	struct hardware_t *hw = NULL;
	int i = 0;

	/* Make sure the pilight sender gets
	   the highest priority available */
#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = 80;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched);
#endif

	if(strlen(data->message) > 0 && data->protocol != NULL) {
		if(strcmp(data->message, "{}") != 0) {
			if(data1 == NULL) {
				if((data1 = MALLOC(sizeof(struct reason_code_sent_t))) == NULL) {
					OUT_OF_MEMORY
				}
				memset(&data1->origin, '\0', 256);
			}
			strncpy(data1->protocol, data->protocol, 255);
			strncpy(data1->message, data->message, 1024);
			if(strlen(data->uuid) > 0) {
				strncpy(data1->uuid, data->uuid, UUID_LENGTH);
			} else {
				memset(&data1->uuid, '\0', UUID_LENGTH+1);
			}
			data1->repeat = 1;
		}
	}
	/*if(strlen(data->settings) > 0) {
		if(data1 == NULL) {
			if((data1 = MALLOC(sizeof(struct reason_code_sent_t))) == NULL) {
				OUT_OF_MEMORY
			}
			memset(&data1->origin, '\0', 256);
		}
		if(strcmp(data->settings, "{}") != 0) {
			strncpy(data1->settings, data->settings, 1024);
		} else {
			memset(&data1->settings, '\0', 1025);
		}
	}*/

	struct JsonNode *jrespond = NULL;
	struct JsonNode *jchilds = NULL;
	struct hardware_t *hardware = NULL;
	if(hardware_select(ORIGIN_MASTER, NULL, &jrespond) == 0) {
		jchilds = json_first_child(jrespond);
		while(jchilds) {
			if(hardware_select_struct(ORIGIN_MASTER, jchilds->key, &hardware) == 0) {
				if(hardware->hwtype == data->hwtype) {
					hw = hardware;
					break;
				}
			}

			jchilds = jchilds->next;
		}
	}

	eventpool_trigger(REASON_SEND_BEGIN, NULL, NULL);
	if(hw != NULL) {
		if((hw->comtype == COMOOK || hw->comtype == COMPLSTRAIN) && hw->sendOOK != NULL) {
			logprintf(LOG_DEBUG, "**** RAW CODE ****");
			if(log_level_get() >= LOG_DEBUG) {
				for(i=0;i<data->rawlen;i++) {
					printf("%d ", data->pulses[i]);
				}
				printf("\n");
			}
			logprintf(LOG_DEBUG, "**** RAW CODE ****");
			if(hw->sendOOK(data->pulses, data->rawlen, data->txrpt) == 0) {
				logprintf(LOG_DEBUG, "successfully send %s code", data->protocol);
			} else {
				logprintf(LOG_ERR, "failed to send code");
			}
			if(strcmp(data->protocol, "raw") == 0) {
				int plslen = data->pulses[data->rawlen-1]/PULSE_DIV;
				receive_parse_code(data->pulses, data->rawlen, plslen, -1);
			}
		} else if(hw->comtype == COMAPI && hw->sendAPI != NULL) {
			/*
			 * FIXME
			 */
			if(data != NULL) {
				// if(hw->sendAPI(jbroadcast) == 0) {
					// logprintf(LOG_DEBUG, "successfully send %s command", data->protocol);
				// } else {
					// logprintf(LOG_ERR, "failed to send command");
				// }
			}
		}
	} else if(strcmp(data->protocol, "raw") == 0) {
		int plslen = data->pulses[data->rawlen-1]/PULSE_DIV;
		receive_parse_code(data->pulses, data->rawlen, plslen, -1);
	}
	eventpool_trigger(REASON_SEND_END, NULL, NULL);

	if(data1 != NULL) {
		eventpool_trigger(REASON_CODE_SENT, reason_code_sent_free, data1);
	}

	return (void *)NULL;
}

/*
 * This function requires a struct like this:
 * {
 *      "action": "send",
 *      "code": {
 *              "off": 1,
 *              "unit": 0,
 *              "id": 123456,
 *              "protocol": [ "kaku_switch" ]
 *     }
 * }
*/
static int send_queue(struct JsonNode *json, enum origin_t origin) {
	int match = 0, raw[MAXPULSESTREAMLENGTH-1];
	char message[255];
	struct clients_t *tmp_clients = NULL;
	char *uuid = NULL, *buffer = NULL;
	/* Hold the final protocol struct */
	struct protocol_t *protocol = NULL;

	struct JsonNode *jcode = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jprotocol = NULL;

	tmp_clients = clients;
	while(tmp_clients) {
		if(tmp_clients->forward == 1) {
			if(buffer == NULL) {
				buffer = json_stringify(json, NULL);
			}
			socket_write(tmp_clients->id, buffer);
		}
		tmp_clients = tmp_clients->next;
	}
	if(buffer != NULL) {
		json_free(buffer);
	}

	if((jcode = json_find_member(json, "code")) == NULL) {
		logprintf(LOG_ERR, "sender did not send any codes");
		json_delete(jcode);
		return -1;
	} else if((jprotocols = json_find_member(jcode, "protocol")) == NULL) {
		logprintf(LOG_ERR, "sender did not provide a protocol name");
		json_delete(jcode);
		return -1;
	} else {
		json_find_string(jcode, "uuid", &uuid);
		if(uuid == NULL) {
			json_append_member(jcode, "uuid", json_mkstring(pilight_uuid));
		}
		json_find_string(jcode, "uuid", &uuid);
		/* If we matched a protocol and are not already sending, continue */
		if(uuid != NULL && strcmp(uuid, pilight_uuid) == 0) {
			jprotocol = json_first_child(jprotocols);
			while(jprotocol && match == 0) {
				match = 0;
				if(jprotocol->tag == JSON_STRING) {
					struct protocols_t *pnode = protocols;
					/* Retrieve the used protocol */
					while(pnode) {
						protocol = pnode->listener;
						/* Check if the protocol exists */
						if(protocol_device_exists(protocol, jprotocol->string_) == 0 && match == 0) {
							match = 1;
							break;
						}
						pnode = pnode->next;
					}
				}
				jprotocol = jprotocol->next;
			}
			memset(raw, 0, sizeof(raw));
			protocol->raw = raw;
			if(match == 1 && protocol->createCode != NULL) {
				/* Let the protocol create his code */
				memset(message, 0, 255);
				// if(protocol->createCode(jcode, message) == 0 && main_loop == 1) {
					// struct reason_send_code_t *data = MALLOC(sizeof(struct reason_send_code_t));
					// if(data == NULL) {
						// OUT_OF_MEMORY
					// }
					// data->origin = origin;
					// snprintf(data->message, 1024, "{\"message\":%s}", message);
					// data->rawlen = protocol->rawlen;
					// memcpy(data->pulses, protocol->raw, protocol->rawlen*sizeof(int));
					// data->txrpt = protocol->txrpt;
					// strncpy(data->protocol, protocol->id, 255);
					// data->hwtype = protocol->hwtype;

					/*struct JsonNode *jtmp = NULL;
					char *stmp = NULL;
					int x = 0;

					x += snprintf(data->settings, 1024, "{");
					struct options_t *tmp_options = protocol->options;
					while(tmp_options) {
						if(tmp_options->conftype == DEVICES_SETTING) {
							if(tmp_options->vartype == JSON_NUMBER &&
								(jtmp = json_find_member(jcode, tmp_options->name)) != NULL &&
								 jtmp->tag == JSON_NUMBER) {
								x += snprintf(&data->settings[x], 1024-x, "\"%s\":%.*f,", tmp_options->name, jtmp->decimals_, jtmp->number_);
							} else if(tmp_options->vartype == JSON_STRING && json_find_string(jcode, tmp_options->name, &stmp) == 0) {
								x += snprintf(&data->settings[x], 1024-x, "\"%s\":\"%s\",", tmp_options->name, stmp);
							}
						}
						tmp_options = tmp_options->next;
					}
					if(x > 1) {
						x -= 1;
					}
					strncpy(&data->settings[x], "}", 1024-x);*/

					// if(uuid != NULL) {
						// strcpy(data->uuid, uuid);
					// } else {
						// memset(data->uuid, 0, UUID_LENGTH+1);
					// }
					// eventpool_trigger(REASON_SEND_CODE, reason_send_code_free, data);

					// return 0;
				// } else {
					// return -1;
				// }
			}
		} else {
			return 0;
		}
	}
	return -1;
}

static int control_device(char *dev, char *state, struct JsonNode *values, enum origin_t origin) {
	struct options_t *opt = NULL;
	struct protocol_t *tmp_protocol = NULL;
	struct JsonNode *jvalues = values;
	struct varcont_t id;
	char *name = NULL, *value = NULL, *dev_uuid = NULL;
	double ival = 0.0;
	int i = 0, hwtype = -1, dec = 0;

	struct JsonNode *code = json_mkobject();
	struct JsonNode *json = json_mkobject();
	struct JsonNode *jprotocols = json_mkarray();

	i = 0;
	devices_select_string_setting(ORIGIN_SENDER, dev, "uuid", &dev_uuid);
	while(devices_select_protocol(ORIGIN_SENDER, dev, i++, &tmp_protocol) == 0) {
		opt = tmp_protocol->options;
		if(hwtype == -1) {
			hwtype = tmp_protocol->hwtype;
		}
		json_append_element(jprotocols, json_mkstring(tmp_protocol->devices->id));
		while(opt) {
			if(opt->conftype == DEVICES_SETTING) {
				if(json_find_member(code, opt->name) == NULL) {
					if(opt->vartype == JSON_STRING &&
					   devices_select_string_setting(ORIGIN_SENDER, dev, opt->name, &value) == 0) {
						json_append_member(code, opt->name, json_mkstring(value));
					}
					if(opt->vartype == JSON_NUMBER &&
					   devices_select_number_setting(ORIGIN_SENDER, dev, opt->name, &ival, &dec) == 0) {
						json_append_member(code, opt->name, json_mknumber(ival, dec));
					}
				}
			}
			jvalues = values;
			while(jvalues) {
				if((opt->conftype == DEVICES_VALUE || opt->conftype == DEVICES_OPTIONAL) &&
					 strcmp(jvalues->key, opt->name) == 0 &&
					 json_find_member(code, opt->name) == NULL) {
					if(jvalues->tag == JSON_STRING) {
						json_append_member(code, jvalues->key, json_mkstring(jvalues->string_));
					} else if(jvalues->tag == JSON_NUMBER) {
						json_append_member(code, jvalues->key, json_mknumber(jvalues->number_, jvalues->decimals_));
					}
				}
				jvalues = jvalues->next;
			}
			if(json_find_member(code, opt->name) == NULL) {
				if(opt->conftype == DEVICES_STATE && opt->argtype == OPTION_NO_VALUE && strcmp(opt->name, state) == 0) {
					json_append_member(code, opt->name, json_mknumber(1, 0));
				} else if(opt->conftype == DEVICES_STATE && opt->argtype == OPTION_HAS_VALUE) {
					json_append_member(code, opt->name, json_mkstring(state));
				}
			}
			opt = opt->next;
		}
	}

	i = 0;
	while(devices_select_id(ORIGIN_SENDER, dev, i++, &name, &id) == 0) {
		if(json_find_member(code, name) == NULL) {
			if(id.type_ == JSON_STRING) {
				json_append_member(code, name, json_mkstring(id.string_));
			} else if(id.type_ == JSON_NUMBER) {
				json_append_member(code, name, json_mknumber(id.number_, id.decimals_));
			}
		}
	}

	/* Construct the right json object */
	json_append_member(code, "protocol", jprotocols);
	if(dev_uuid != NULL) {
		json_append_member(code, "uuid", json_mkstring(dev_uuid));
	}
	json_append_member(json, "code", code);
	json_append_member(json, "action", json_mkstring("send"));


	if(send_queue(json, origin) == 0) {
		json_delete(json);
		return 0;
	}

	json_delete(json);
	return -1;
}

static void *control_device1(int reason, void *param, void *userdata) {
	struct reason_control_device_t *data = param;

	control_device(data->dev, data->state, json_first_child(data->values), ORIGIN_ACTION);
	return NULL;
}

#ifdef WEBSERVER
static void client_webserver_parse_code(int i) {
	struct sockaddr_in sockin;
	socklen_t len = sizeof(sockin);
	char *p = NULL, buff[BUFFER_SIZE], *mimetype = NULL, line[255];
	char ip[INET_ADDRSTRLEN+1];
	int l = 0;

	memset(&ip, '\0', INET_ADDRSTRLEN+1);
	memset(&line, '\0', 255);
	memset(&buff, '\0', BUFFER_SIZE);
	memset(&sockin, 0, len);

	if(getsockname(i, (struct sockaddr *)&sockin, &len) == -1) {
		logprintf(LOG_ERR, "could not determine server ip address");
		return;
	} else {
		p = buff;
		if((mimetype = MALLOC(strlen("text/html")+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(mimetype, "text/plain");

		memset(&ip, '\0', INET_ADDRSTRLEN+1);
		uv_ip4_name((void *)&(sockin.sin_addr), ip, INET_ADDRSTRLEN+1);

		if(webserver_enable == 0 || (webserver_http_port < 0 && webserver_https_port < 0)) {
			l = snprintf(line, 255, "The pilight webserver is disabled");
		} else if(webserver_http_port < 0) {
			l = snprintf(line, 255, "The pilight webserver is running at https://%s:%d/", ip, webserver_https_port);
		} else if(webserver_https_port < 0) {
			l = snprintf(line, 255, "The pilight webserver is running at http://%s:%d/", ip, webserver_http_port);
		} else {
			l = snprintf(line, 255, "The pilight webserver is running at http://%s:%d/ or https://%s:%d/", ip, webserver_https_port, ip, webserver_http_port);
		}
		webserver_create_header(&p, "200 OK", mimetype, l);
		socket_write(i, buff, strlen(buff));
		socket_write(i, line, l);
		FREE(mimetype);
	}
}
#endif

static void client_remove(int id) {
	struct clients_t *currP, *prevP;

	prevP = NULL;

	for(currP = clients; currP != NULL; prevP = currP, currP = currP->next) {

		if(currP->id == id) {
			if(prevP == NULL) {
				clients = currP->next;
			} else {
				prevP->next = currP->next;
			}

			FREE(currP);
			break;
		}
	}
}

static void *socket_client_disconnected(int reason, void *param, void *userdata) {
	struct reason_socket_disconnected_t *data = param;

	if(data->fd > -1) {
		client_remove(data->fd);
	}
	return NULL;
}

static int socket_parse_responses(char *buffer, char *media, char **respons) {
	struct JsonNode *json = NULL;
	char *action = NULL, *status = NULL;

	if(strcmp(buffer, "HEART") == 0) {
		if((*respons = MALLOC(strlen("BEAT")+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(*respons, "BEAT");
		return 0;
	}	else {
		if(pilight.runmode != ADHOC) {
			logprintf(LOG_DEBUG, "socket recv: %s", buffer);
		}

		if(json_validate(buffer) == true) {
			json = json_decode(buffer);
			if((json_find_string(json, "status", &status)) == 0) {
				if(strcmp(status, "success") == 0) {
					if((*respons = MALLOC(strlen("{\"status\":\"success\"}")+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(*respons, "{\"status\":\"success\"}");
					json_delete(json);
					return 0;
				}
			}
			if((json_find_string(json, "action", &action)) == 0) {
				if(strcmp(action, "send") == 0) {
					if(send_queue(json, ORIGIN_SENDER) == 0) {
						if((*respons = MALLOC(strlen("{\"status\":\"success\"}")+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(*respons, "{\"status\":\"success\"}");
						json_delete(json);
						return 0;
					} else {
						if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(*respons, "{\"status\":\"failed\"}");
						json_delete(json);
						return 0;
					}
				} else if(strcmp(action, "control") == 0) {
					struct JsonNode *code = NULL;
					char *device = NULL;
					if((code = json_find_member(json, "code")) == NULL || code->tag != JSON_OBJECT) {
						logprintf(LOG_ERR, "client did not send any codes");
						json_delete(json);
						return -1;
					} else {
						/* Check if a location and device are given */
						if(json_find_string(code, "device", &device) != 0) {
							logprintf(LOG_ERR, "client did not send a device");
							json_delete(json);
							return -1;
						/* Check if the device and location exists in the config file */
						} else if(devices_select(ORIGIN_SENDER, device, NULL) == 0) {
							char *state = NULL;
							struct JsonNode *values = NULL;

							json_find_string(code, "state", &state);
							if((values = json_find_member(code, "values")) != NULL) {
								values = json_first_child(values);
							}

							if(control_device(device, state, values, ORIGIN_SENDER) == 0) {
								if((*respons = MALLOC(strlen("{\"status\":\"success\"}")+1)) == NULL) {
									OUT_OF_MEMORY
								}
								strcpy(*respons, "{\"status\":\"success\"}");
								json_delete(json);
								return 0;
							} else {
								if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
									OUT_OF_MEMORY
								}
								strcpy(*respons, "{\"status\":\"failed\"}");
								json_delete(json);
								return 0;
							}
						} else {
							logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
							json_delete(json);
							return -1;
						}
					}
				} else if(strcmp(action, "registry") == 0) {
					struct JsonNode *value = NULL;
					char *type = NULL;
					char *key = NULL;
					// char *sval = NULL;
					// double nval = 0.0;
					// int dec = 0;
					if(json_find_string(json, "type", &type) != 0) {
						logprintf(LOG_ERR, "client did not send a type of action");
						json_delete(json);
						return -1;
					} else {
						if(strcmp(type, "set") == 0) {
							if(json_find_string(json, "key", &key) != 0) {
								logprintf(LOG_ERR, "client did not send a registry key");
								if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
									OUT_OF_MEMORY
								}
								strcpy(*respons, "{\"status\":\"failed\"}");
								json_delete(json);
								return 0;
							} else if((value = json_find_member(json, "value")) == NULL) {
								logprintf(LOG_ERR, "client did not send a registry value");
								if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
									OUT_OF_MEMORY
								}
								strcpy(*respons, "{\"status\":\"failed\"}");
								json_delete(json);
								return 0;
							} else {
								// if(value->tag == JSON_NUMBER) {
									// if(registry_update(ORIGIN_MASTER, key, json_mknumber(value->number_, value->decimals_)) == 0) {
										// if((*respons = MALLOC(strlen("{\"status\":\"success\"}")+1)) == NULL) {
											// OUT_OF_MEMORY
										// }
										// strcpy(*respons, "{\"status\":\"success\"}");
										// json_delete(json);
										// return 0;
									// } else {
										// if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
											// OUT_OF_MEMORY
										// }
										// strcpy(*respons, "{\"status\":\"failed\"}");
										// json_delete(json);
										// return 0;
									// }
								// } else if(value->tag == JSON_STRING) {
									// if(registry_update(ORIGIN_MASTER, key, json_mkstring(value->string_)) == 0) {
										// if((*respons = MALLOC(strlen("{\"status\":\"success\"}")+1)) == NULL) {
											// OUT_OF_MEMORY
										// }
										// strcpy(*respons, "{\"status\":\"success\"}");
										// json_delete(json);
										// return 0;
									// } else {
										// if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
											// OUT_OF_MEMORY
										// }
										// strcpy(*respons, "{\"status\":\"failed\"}");
										// json_delete(json);
										// return 0;
									// }
								// } else {
									// logprintf(LOG_ERR, "registry value can only be a string or number");
									// if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
										// OUT_OF_MEMORY
									// }
									// strcpy(*respons, "{\"status\":\"failed\"}");
									// json_delete(json);
									// return 0;
								// }
							}
						} else if(strcmp(type, "remove") == 0) {
							if(json_find_string(json, "key", &key) != 0) {
								logprintf(LOG_ERR, "client did not send a registry key");
								if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
									OUT_OF_MEMORY
								}
								strcpy(*respons, "{\"status\":\"failed\"}");
								json_delete(json);
								return 0;
							} else {
								// if(registry_delete(ORIGIN_MASTER, key) == 0) {
									// if((*respons = MALLOC(strlen("{\"status\":\"success\"}")+1)) == NULL) {
										// OUT_OF_MEMORY
									// }
									// strcpy(*respons, "{\"status\":\"success\"}");
									// json_delete(json);
									// return 0;
								// } else {
									// logprintf(LOG_ERR, "registry value can only be a string or number");
									// if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
										// OUT_OF_MEMORY
									// }
									// strcpy(*respons, "{\"status\":\"failed\"}");
									// json_delete(json);
									// return 0;
								// }
							}
						} else if(strcmp(type, "get") == 0) {
							if(json_find_string(json, "key", &key) != 0) {
								logprintf(LOG_ERR, "client did not send a registry key");
								if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
									OUT_OF_MEMORY
								}
								strcpy(*respons, "{\"status\":\"failed\"}");
								json_delete(json);
								return 0;
							} else {
								// if(registry_select_number(ORIGIN_MASTER, key, &nval, &dec) == 0) {
									// struct JsonNode *jsend = json_mkobject();
									// json_append_member(jsend, "message", json_mkstring("registry"));
									// json_append_member(jsend, "value", json_mknumber(nval, dec));
									// json_append_member(jsend, "key", json_mkstring(key));
									// char *output = json_stringify(jsend, NULL);
									// if((*respons = MALLOC(strlen(output)+1)) == NULL) {
										// OUT_OF_MEMORY
									// }
									// strcpy(*respons, output);
									// json_free(output);
									// json_delete(jsend);
									// json_delete(json);
									// return 0;
								// } else if(registry_select_string(ORIGIN_MASTER, key, &sval) == 0) {
									// struct JsonNode *jsend = json_mkobject();
									// json_append_member(jsend, "message", json_mkstring("registry"));
									// json_append_member(jsend, "value", json_mkstring(sval));
									// json_append_member(jsend, "key", json_mkstring(key));
									// char *output = json_stringify(jsend, NULL);
									// if((*respons = MALLOC(strlen(output)+1)) == NULL) {
										// OUT_OF_MEMORY
									// }
									// strcpy(*respons, output);
									// json_free(output);
									// json_delete(jsend);
									// json_delete(json);
									// return 0;
								// } else {
									// logprintf(LOG_ERR, "registry key '%s' does not exist", key);
									// if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
										// OUT_OF_MEMORY
									// }
									// strcpy(*respons, "{\"status\":\"failed\"}");
									// json_delete(json);
									// return 0;
								// }
							}
						}
					}
				} else if(strcmp(action, "request config") == 0) {
					struct JsonNode *jsend = json_mkobject();
					struct JsonNode *jconfig = NULL;
					jconfig = config_print(CONFIG_INTERNAL, media);
					json_append_member(jsend, "message", json_mkstring("config"));
					json_append_member(jsend, "config", jconfig);
					char *output = json_stringify(jsend, NULL);
					str_replace("%", "%%", &output);
					if((*respons = MALLOC(strlen(output)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(*respons, output);
					json_free(output);
					json_delete(jsend);
					json_delete(json);
					return 0;
				} else if(strcmp(action, "request values") == 0) {
					struct JsonNode *jsend = json_mkobject();
					struct JsonNode *jvalues = values_print(media);
					json_append_member(jsend, "message", json_mkstring("values"));
					json_append_member(jsend, "values", jvalues);
					char *output = json_stringify(jsend, NULL);
					if((*respons = MALLOC(strlen(output)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(*respons, output);
					json_free(output);
					json_delete(jsend);
					json_delete(json);
					return 0;
				}
			} else {
				json_delete(json);
				return -1;
			}
		}
	}
	return -1;
}

/* Parse the incoming buffer from the client */
static void *socket_parse_data(int reason, void *param, void *userdata) {
	struct reason_socket_received_t *data = param;

	struct sockaddr_in address;
	socklen_t socklen = sizeof(address);

	struct JsonNode *json = NULL;
	struct JsonNode *options = NULL;
	struct clients_t *tmp_clients = NULL;
	struct clients_t *client = NULL;
	char *action = NULL, *media = NULL, *status = NULL, *respons = NULL;
	char all[] = "all";
	int error = 0, exists = 0, sd = -1;

	if(strlen(data->type) == 0) {
		logprintf(LOG_ERR, "socket data misses a socket type");
	}

	if(data->buffer == NULL) {
		logprintf(LOG_ERR, "socket message incorrectly formatted");
		return NULL;
	}

	sd = data->fd;
	getpeername(sd, (struct sockaddr*)&address, &socklen);

	tmp_clients = clients;
	while(tmp_clients) {
		if(tmp_clients->id == sd) {
			exists = 1;
			client = tmp_clients;
			break;
		}
		tmp_clients = tmp_clients->next;
	}
	if(exists == 0) {
		media = all;
	} else {
		media = client->media;
	}

	if(json_validate(data->buffer) == true) {
		json = json_decode(data->buffer);
		if((json_find_string(json, "action", &action)) == 0) {
			if(strcmp(action, "identify") == 0) {
				/* Check if client doesn't already exist */
				if(exists == 0) {
					if((client = MALLOC(sizeof(struct clients_t))) == NULL) {
						OUT_OF_MEMORY
					}
					client->core = 0;
					client->config = 0;
					client->receiver = 0;
					client->forward = 0;
					client->stats = 0;
					client->cpu = 0;
					strcpy(client->media, "all");
					client->next = NULL;
					client->id = sd;
					memset(client->uuid, '\0', sizeof(client->uuid));
				}
				if(json_find_string(json, "media", &media) == 0) {
					if(strcmp(media, "all") == 0 || strcmp(media, "mobile") == 0 ||
						 strcmp(media, "desktop") == 0 || strcmp(media, "web") == 0) {
						strcpy(client->media, media);
					}
				} else {
					strcpy(client->media, "all");
				}
				char *t = NULL;
				if(json_find_string(json, "uuid", &t) == 0) {
					strcpy(client->uuid, t);
				}
				if((options = json_find_member(json, "options")) != NULL) {
					struct JsonNode *childs = json_first_child(options);
					while(childs) {
						if(strcmp(childs->key, "core") == 0 &&
							 childs->tag == JSON_NUMBER) {
							if((int)childs->number_ == 1) {
								client->core = 1;
							} else {
								client->core = 0;
							}
						} else if(strcmp(childs->key, "stats") == 0 &&
							 childs->tag == JSON_NUMBER) {
							if((int)childs->number_ == 1) {
								client->stats = 1;
							} else {
								client->stats = 0;
							}
						} else if(strcmp(childs->key, "receiver") == 0 &&
							 childs->tag == JSON_NUMBER) {
							if((int)childs->number_ == 1) {
								client->receiver = 1;
							} else {
								client->receiver = 0;
							}
						} else if(strcmp(childs->key, "config") == 0 &&
							 childs->tag == JSON_NUMBER) {
							if((int)childs->number_ == 1) {
								client->config = 1;
							} else {
								client->config = 0;
							}
						} else if(strcmp(childs->key, "forward") == 0 &&
							 childs->tag == JSON_NUMBER) {
							if((int)childs->number_ == 1) {
								client->forward = 1;
							} else {
								client->forward = 0;
							}
						} else {
						 error = 1;
						 break;
						}
						childs = childs->next;
					}
				}
				if(exists == 0) {
					if(error == -1) {
						FREE(client);
					} else {
						tmp_clients = clients;
						if(tmp_clients) {
							while(tmp_clients->next != NULL) {
								tmp_clients = tmp_clients->next;
							}
							tmp_clients->next = client;
						} else {
							client->next = clients;
							clients = client;
						}
						socket_write(sd, "{\"status\":\"success\"}");
						json_delete(json);
						return NULL;
					}
				}
				json_delete(json);
				return NULL;
			/*
			 * Parse received codes from nodes
			 */
			} else if(strcmp(action, "update") == 0) {
				if(exists == 1) {
					struct JsonNode *jvalues = NULL;
					char *pname = NULL;
					if((jvalues = json_find_member(json, "values")) != NULL) {
						exists = 0;
						tmp_clients = clients;
						while(tmp_clients) {
							if(tmp_clients->id == sd) {
								exists = 1;
								client = tmp_clients;
								break;
							}
							tmp_clients = tmp_clients->next;
						}
						if(exists == 1) {
							json_find_number(jvalues, "cpu", &client->cpu);
						}
					}
					if(json_find_string(json, "protocol", &pname) == 0) {
						char *out = MALLOC(strlen(data->buffer)+1);
						if(out == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(out, data->buffer);
						eventpool_trigger(REASON_FORWARD, reason_forward_free, out);
						json_delete(json);
						return NULL;
					}
				}
				json_delete(json);
				return NULL;
			} else if((json_find_string(json, "status", &status)) == 0) {
				tmp_clients = clients;
				while(tmp_clients) {
					if(tmp_clients->id == sd) {
						exists = 1;
						client = tmp_clients;
						break;
					}
					tmp_clients = tmp_clients->next;
				}
				if(strcmp(status, "success") == 0) {
					logprintf(LOG_DEBUG, "client \"%s\" successfully executed our latest request", client->uuid);
					json_delete(json);
					return NULL;
				} else if(strcmp(status, "failed") == 0) {
					logprintf(LOG_DEBUG, "client \"%s\" failed executing our latest request", client->uuid);
					json_delete(json);
					return NULL;
				}

				json_delete(json);
				return NULL;
			}
		}
	}

	if(socket_parse_responses(data->buffer, media, &respons) == 0) {
		struct reason_socket_send_t *data1 = MALLOC(sizeof(struct reason_socket_send_t));
		if(data1 == NULL) {
			OUT_OF_MEMORY
		}
		data1->fd = sd;
		if((data1->buffer = MALLOC(strlen(respons)+1)) == NULL) {
			OUT_OF_MEMORY;
		}
		strcpy(data1->buffer, respons);
		strncpy(data1->type, data->type, 255);

		eventpool_trigger(REASON_SOCKET_SEND, reason_socket_send_free, data1);

		FREE(respons);
		json_delete(json);
		return NULL;
	} else {
		logprintf(LOG_ERR, "could not parse response to: %s", data->buffer);
		client_remove(sd);
		socket_close(sd);
		json_delete(json);
		return NULL;
	}
	json_delete(json);
	return NULL;
}

static void socket_read_cb(int fd, char *buf, ssize_t len, char **buf1, ssize_t *len1) {
	/* Serve static webserver page. This is the only request that is
		 expected not to be a json object */
#ifdef WEBSERVER
	if(strstr(buf, " HTTP/") != NULL) {
		client_webserver_parse_code(fd);
		return;
	}
#endif

	if(socket_recv(buf, len, buf1, len1) > 0) {
		if(strstr(*buf1, "\n") != NULL) {
			char **array = NULL;
			unsigned int n = explode(*buf1, "\n", &array), q = 0;
			for(q=0;q<n;q++) {
				struct reason_socket_received_t *data1 = MALLOC(sizeof(struct reason_socket_received_t));
				if(data1 == NULL) {
					OUT_OF_MEMORY
				}
				data1->fd = (int)fd;
				if((data1->buffer = MALLOC(strlen(array[q])+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(data1->buffer, array[q]);
				strncpy(data1->type, "socket", 255);
				eventpool_trigger(REASON_SOCKET_RECEIVED, reason_socket_received_free, data1);
			}
			array_free(&array, n);
		} else {
			struct reason_socket_received_t *data1 = MALLOC(sizeof(struct reason_socket_received_t));
			if(data1 == NULL) {
				OUT_OF_MEMORY
			}
			data1->fd = (int)fd;
			if((data1->buffer = MALLOC(*len1+1)) == NULL) {
				OUT_OF_MEMORY
			}
			strcpy(data1->buffer, *buf1);
			strncpy(data1->type, "socket", 255);
			eventpool_trigger(REASON_SOCKET_RECEIVED, reason_socket_received_free, data1);
		}
		FREE(*buf1);
		*len1 = 0;
	}
}

static void save_pid(pid_t npid) {
	int f = 0;
	char buffer[BUFFER_SIZE];
	memset(buffer, '\0', BUFFER_SIZE);
	if((f = open(pid_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) != -1) {
		lseek(f, 0, SEEK_SET);
		sprintf(buffer, "%d", npid);
		ssize_t i = write(f, buffer, strlen(buffer));
		if(i != strlen(buffer)) {
			logprintf(LOG_WARNING, "could not store pid in %s", pid_file);
		}
	}
	close(f);
}

static void daemonize(void) {
	log_file_enable();
	log_shell_disable();
	/* Get the pid of the fork */

	pid_t npid = fork();
	switch(npid) {
		case 0:
		break;
		case -1:
			logprintf(LOG_ERR, "could not daemonize program");
			exit(1);
		break;
		default:
			save_pid(npid);
			logprintf(LOG_INFO, "daemon started with pid: %d", npid);
			exit(0);
		break;
	}
}

static void pilight_abort(uv_timer_t *timer_req) {
	exit(EXIT_FAILURE);
}

void registerVersion(void) {
	// registry_delete(ORIGIN_MASTER, "pilight.version");
	// registry_update(ORIGIN_MASTER, "pilight.version.current", json_mkstring((char *)PILIGHT_VERSION));
}

static void pilight_stats(uv_timer_t *timer_req) {
	int watchdog = 1, stats = 1;
	int itmp = 0;
	if(config_setting_get_number("watchdog-enable", 0, &itmp) == 0) { watchdog = itmp; }
	if(config_setting_get_number("stats-enable", 0, &itmp) == 0) { stats = itmp; }

	if(pilight.runmode == STANDALONE) {
		registerVersion();
	}

	if(stats == 1) {
		double cpu = 0.0;
		cpu = getCPUUsage();
		if(watchdog == 1 && (cpu > 90)) {
			logprintf(LOG_CRIT, "cpu usage too high %f%%, will abort when this persists", cpu);
		} else {
			if(watchdog == 1 && stats == 1 && timer_abort_req != NULL) {
				uv_timer_stop(timer_abort_req);
				uv_timer_start(timer_abort_req, pilight_abort, 9000, -1);
			}

			struct reason_code_sent_t *data = MALLOC(sizeof(struct reason_code_sent_t));
			memset(data, '\0', sizeof(struct reason_code_sent_t));
			if(data == NULL) {
				OUT_OF_MEMORY
			}
			data->type = PROCESS;
			strcpy(data->origin, "core");
			/*
			 * The missing outer brackets will be
			 * added in the broadcast function.
			 */
			snprintf(data->message, 1024, "{\"values\":{\"cpu\":%f},\"origin\":\"core\",\"type\":%d}", cpu, PROCESS);
			logprintf(LOG_DEBUG, "cpu: %f%%", cpu);
			struct clients_t *tmp_clients = clients;
			while(tmp_clients) {
				if(tmp_clients->cpu > 0) {
					logprintf(LOG_DEBUG, "- client: %s cpu: %f%%",
								tmp_clients->uuid, tmp_clients->cpu);
				}
				tmp_clients = tmp_clients->next;
			}
			eventpool_trigger(REASON_CODE_SENT, reason_code_sent_free, data->message);
		}
	}
	return;
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

void main_gc(void) {
	main_loop = 0;

	event_operator_gc();
	event_action_gc();
	event_function_gc();
	protocol_gc();
	hardware_gc();
	storage_gc();
	socket_gc();
	ssdp_gc();
	webserver_gc();

	/*
	 * FIXME: GC clients
	 */
	
	wiringXGC();

	log_shell_disable();
	eventpool_gc();
	options_gc();
	log_gc();

	struct clients_t *tmp = NULL;	
	while(clients) {
		tmp = clients;		
		clients = clients->next;
		FREE(tmp);
	}
	
	if(timer_stats_req != NULL) {
		uv_timer_stop(timer_stats_req);
	}
	if(timer_abort_req != NULL) {
		uv_timer_stop(timer_abort_req);
	}
	if(webserver_root != NULL) {
		FREE(webserver_root);
	}
	if(pid_file != NULL) {
		FREE(pid_file);
	}
	FREE(progname);
}

static void signal_cb(uv_signal_t *handle, int signum) {
	logprintf(LOG_INFO, "Interrupt signal received. Please wait while pilight is shutting down");
	/*
	 * FIXME: Sync configuration
	 */
	main_gc();
	uv_stop(uv_default_loop());
}

int start_pilight(int argc, char **argv) {
	pilight.process = PROCESS_DAEMON;

	if((progname = MALLOC(16)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(progname, "pilight-daemon");	

	{
		int nr = sizeof(signals)/sizeof(signals[0]), i = 0;
		if((signal_req = MALLOC(sizeof(uv_signal_t *)*nr)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		for(i=0;i<nr;i++) {
			if((signal_req[i] = MALLOC(sizeof(uv_signal_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_signal_init(uv_default_loop(), signal_req[i]);
			uv_signal_start(signal_req[i], signal_cb, signals[i]);
		}
	}

	struct options_t *options = NULL;

	char *args = NULL;
	char *fconfig = NULL;

	int show_default = 0;
	int show_version = 0;
	int show_help = 0;

	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(fconfig, CONFIG_FILE);

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "D", "nodaemon", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "C", "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "S", "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, "P", "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, "258", "debuglevel", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[012]{1}");
	// options_add(&options, 258, "memory-tracer", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	while(1) {
		int c = options_parse1(&options, argc, argv, 1, &args, NULL);
		if(c == -1) {
			break;
		}
		if(c == -2) {
			show_help = 1;
			break;
		}
		switch(c) {
			case 'H':
				show_help = 1;
			break;
			case 'V':
				show_version = 1;
			break;
			case 'C':
				if((fconfig = REALLOC(fconfig, strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(fconfig, args);
			break;
			case 'S':
			break;
			case 'P':
			break;
			case 'D':
				nodaemon = 1;
				verbosity = LOG_DEBUG;
			break;
			case 258:
				pilight.debuglevel = atoi(args);
				nodaemon = 1;
				verbosity = LOG_DEBUG;
			break;
			default:
				show_default = 1;
			break;
		}
	}
	options_delete(options);

	if(show_help == 1) {
		char help[1024];
		char tabs[5];
		strcpy(tabs, "\t\t");
		sprintf(help, "Usage: %s [options]\n"
									"\t -H --help\t\t\tdisplay usage summary\n"
									"\t -V --version\t%sdisplay version\n"
									"\t -C --config\t%sconfig file\n"
									"\t -S --server=x.x.x.x\t\tconnect to server address\n"
									"\t -P --port=xxxx\t%sconnect to server port\n"
									"\t -D --nodaemon\t%sdo not daemonize and\n"
									"\t\t\t%sshow debug information\n"
									"\t    --debuglevel\t\tshow additional development info\n",
									progname, tabs, tabs, tabs, tabs, tabs);
		printf("%s", help);
#if defined(__arm__) || defined(__mips__)
		printf("\n\tThe following GPIO platforms are supported:\n");
		char **out = NULL;
		int z = 0, i = wiringXSupportedPlatforms(&out);

		printf("\t- none\n");
		for(z=0;z<i;z++) {
			printf("\t- %s\n", out[z]);
			free(out[z]);
		}
		free(out);
		printf("\n");
#endif
		goto clear;
	}
	if(show_version == 1) {
#ifdef HASH
	printf("%s version %s\n", progname, HASH);
#else
	printf("%s version %s\n", progname, PILIGHT_VERSION);
#endif
		goto clear;
	}
	if(show_default == 1) {
		printf("Usage: %s [options]\n", progname);
		goto clear;
	}

	if(geteuid() != 0) {
		printf("%s requires root privileges in order to run\n", progname);
		FREE(progname);
		exit(EXIT_FAILURE);
	}

	log_level_set(LOG_INFO);
	log_file_enable();
	log_shell_disable();

	{
		int nrdevs = 0, x = 0;
		char **devs = NULL, *p = NULL;
		if((nrdevs = inetdevs(&devs)) > 0) {
			for(x=0;x<nrdevs;x++) {
				if((p = genuuid(devs[x])) == NULL) {
					logprintf(LOG_ERR, "could not generate the device uuid");
				} else {
					strcpy(pilight_uuid, p);
					FREE(p);
					break;
				}
			}
		}
		array_free(&devs, nrdevs);
	}

	atomicinit();
	
	if(nodaemon == 1) {
		log_level_set(verbosity);
		log_shell_enable();
	}

	int *ret = NULL, n = 0;
	if((n = isrunning("pilight-daemon", &ret)) > 1) {
		int i = 0;
		for(i=0;i<n;i++) {
			if(ret[i] != getpid()) {
				log_shell_enable();
				logprintf(LOG_NOTICE, "pilight-daemon is already running (%d)", ret[i]);
				break;
			}
		}
		FREE(ret);
		goto clear;
	}

	if((n = isrunning("pilight-debug", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-debug instance found (%d)", ret[0]);
		FREE(ret);
		goto clear;
	}

	if((n = isrunning("pilight-raw", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-raw instance found (%d)", ret[0]);
		FREE(ret);
		goto clear;
	}

	eventpool_init(EVENTPOOL_THREADED);
	event_init();
	protocol_init();
	hardware_init();
	storage_init();

	if(storage_read(fconfig, CONFIG_ALL) != 0) {		
		goto clear;
	}
	FREE(fconfig);

	// registerVersion();

	eventpool_callback(REASON_SEND_CODE, send_code, NULL);
	eventpool_callback(REASON_SOCKET_RECEIVED, socket_parse_data, NULL);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_client_disconnected, NULL);
	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain, NULL);
	eventpool_callback(REASON_CODE_RECEIVED, broadcast, NULL);
	eventpool_callback(REASON_CODE_SENT, broadcast, NULL);
	eventpool_callback(REASON_CODE_SEND_SUCCESS, code_sent_success, NULL);
	eventpool_callback(REASON_CODE_SEND_FAIL, code_sent_fail, NULL);
	eventpool_callback(REASON_CONTROL_DEVICE, control_device1, NULL);

#ifdef WEBSERVER
	{
	#ifdef WEBSERVER_HTTPS
		char *pemfile = NULL;
		if(config_setting_get_string("pem-file", 0, &pemfile) != 0) {
			if((pemfile = REALLOC(pemfile, strlen(PEM_FILE)+1)) == NULL) {
				OUT_OF_MEMORY
			}
			strcpy(pemfile, PEM_FILE);
		}

		char *content = NULL;
		unsigned char md5sum[17];
		char md5conv[33];
		int i = 0;
		char *p = (char *)md5sum;
		if(file_exists(pemfile) != 0) {
			logprintf(LOG_ERR, "missing webserver SSL private key %s", pemfile);
			if(pemfile != NULL) {
				FREE(pemfile);
			}
			goto clear;
		}
		if(file_get_contents(pemfile, &content) == 0) {
			mbedtls_md5((const unsigned char *)content, strlen((char *)content), (unsigned char *)p);
			for(i = 0; i < 32; i+=2) {
				sprintf(&md5conv[i], "%02x", md5sum[i/2] );
			}
			// if(strcmp(md5conv, PILIGHT_PEM_MD5) == 0) {
				// registry_update(ORIGIN_MASTER, "webserver.ssl.certificate.secure", json_mknumber(0, 0));
			// } else {
				// registry_update(ORIGIN_MASTER, "webserver.ssl.certificate.secure", json_mknumber(1, 0));
			// }
			// registry_update(ORIGIN_MASTER, "webserver.ssl.certificate.location", json_mkstring(pemfile));
			FREE(content);
		}

		if(pemfile != NULL) {
			FREE(pemfile);
		}
	#endif

		int itmp = 0;
		if(config_setting_get_number("webserver-enable", 0, &itmp) == 0) { webserver_enable = itmp; }
		if(config_setting_get_number("webserver-http-port", 0, &itmp) == 0) { webserver_http_port = itmp; }
		if(config_setting_get_number("webserver-https-port", 0, &itmp) == 0) { webserver_https_port = itmp; }
		if(config_setting_get_string("webserver-root", 0, &webserver_root) != 0) {
			if((webserver_root = REALLOC(webserver_root, strlen(WEBSERVER_ROOT)+1)) == NULL) {
				OUT_OF_MEMORY
			}
			strcpy(webserver_root, WEBSERVER_ROOT);
		}
	}
#endif

#ifndef _WIN32
	{
		char buffer[BUFFER_SIZE];
		int f = 0, pid = 0;
		if(config_setting_get_string("pid-file", 0, &pid_file) != 0) {
			if((pid_file = REALLOC(pid_file, strlen(PID_FILE)+1)) == NULL) {
				OUT_OF_MEMORY
			}
			strcpy(pid_file, PID_FILE);
		}

		if((f = open(pid_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) != -1) {
			if(read(f, buffer, BUFFER_SIZE) != -1) {
				//If the file is empty, create a new process
				strcat(buffer, "\0");
				if(!atoi(buffer)) {
					active = 0;
				} else {
					//Check if the process is running
					kill(atoi(buffer), 0);
					//If not, create a new process
					if(errno == ESRCH) {
						active = 0;
					}
				}
				pid = atoi(buffer);
			}
		} else {
			logprintf(LOG_ERR, "could not open / create pid_file %s", pid_file);
			goto clear;
		}
		close(f);

		if(active == 1) {
			nodaemon = 1;
			logprintf(LOG_NOTICE, "already active (pid %d)", pid);
			log_level_set(LOG_NOTICE);
			log_shell_disable();
			goto clear;
		}
	}
#endif

	{
		int itmp = 0.0;
		char *stmp = NULL;
		if(config_setting_get_number("log-level", 0, &itmp) == 0) {
			log_level_set(itmp);
		}

		if(config_setting_get_string("log-file", 0, &stmp) == 0) {
			if(log_file_set(stmp) == EXIT_FAILURE) {
				goto clear;
			}
			FREE(stmp);
		}
	}

#ifdef HASH
	logprintf(LOG_INFO, "version %s", HASH);
#else
	logprintf(LOG_INFO, "version %s", PILIGHT_VERSION);
#endif

	if(nodaemon == 1 || active == 1) {
		log_file_disable();
		log_shell_enable();

		if(nodaemon == 1) {
			log_level_set(verbosity);
		} else {
			log_level_set(LOG_ERR);
		}
	}

	struct JsonNode *jrespond = NULL;
	struct JsonNode *jchilds = NULL;
	struct hardware_t *hardware = NULL;
	if(hardware_select(ORIGIN_MASTER, NULL, &jrespond) == 0) {
		jchilds = json_first_child(jrespond);
		while(jchilds) {
			if(hardware_select_struct(ORIGIN_MASTER, jchilds->key, &hardware) == 0) {
				if(hardware->init != NULL) {
					if(hardware->comtype == COMOOK) {
						struct protocols_t *tmp = protocols;
						while(tmp) {
							if(tmp->listener->hwtype == hardware->hwtype) {
								if(tmp->listener->maxrawlen > hardware->maxrawlen) {
									hardware->maxrawlen = tmp->listener->maxrawlen;
								}
								if(tmp->listener->minrawlen > 0 && tmp->listener->minrawlen < hardware->minrawlen) {
									hardware->minrawlen = tmp->listener->minrawlen;
								}
								if(tmp->listener->maxgaplen > hardware->maxgaplen) {
									hardware->maxgaplen = tmp->listener->maxgaplen;
								}
								if(tmp->listener->mingaplen > 0 && tmp->listener->mingaplen < hardware->mingaplen) {
									hardware->mingaplen = tmp->listener->mingaplen;
								}
								if(tmp->listener->rawlen > 0) {
									logprintf(LOG_EMERG, "%s: setting \"rawlen\" length is not allowed, use \"minrawlen\" and \"maxrawlen\" instead", tmp->listener->id);
									goto clear;
								}
							}
							tmp = tmp->next;
						}
					}
				}
			}

			jchilds = jchilds->next;
		}
	}

	{
		int itmp = 0.0;
		int port = 0, standalone = 0;
		if(config_setting_get_number("port", 0, &itmp) == 0) { port = itmp; }
		if(config_setting_get_number("standalone", 0, &itmp) == 0) { standalone = itmp; }

		pilight.runmode = STANDALONE;
		if(standalone == 0 || (master_server != NULL && master_port > 0)) {
			if(master_server != NULL && master_port > 0) {
				// if((sockfd = socket_connect(master_server, master_port)) == -1) {
					// logprintf(LOG_NOTICE, "pilight daemon not found @%s, waiting for it to come online", master_server);
				// } else {
					// logprintf(LOG_INFO, "a pilight daemon was found, clientizing");
				// }
				pilight.runmode = ADHOC;
			}
		}

		ssl_init();
		if(pilight.runmode == STANDALONE) {
			socket_start((unsigned short)port, socket_read_cb);
			ssdp_start();
#ifdef WEBSERVER
			webserver_start();
#endif
		}

#ifndef _WIN32
		if(nodaemon == 0) {
			daemonize();
		} else {
			save_pid(getpid());
		}
#endif

		pilight.running = 1;
	}

	if(hardware_select(ORIGIN_MASTER, NULL, &jrespond) == 0) {
		jchilds = json_first_child(jrespond);
		while(jchilds && main_loop == 1) {
			if(hardware_select_struct(ORIGIN_MASTER, jchilds->key, &hardware) == 0) {
				if(hardware->init != NULL) {
					if(hardware->init() == EXIT_FAILURE) {
						if(main_loop == 1) {
							logprintf(LOG_ERR, "could not initialize %s hardware mode", hardware->id);
							goto clear;
						} else {
							break;
						}
					}
				}
			}
			if(main_loop == 0) {
				break;
			}

			jchilds = jchilds->next;
		}
	}

	{
		char *servers = NULL;
		if(config_setting_get_string("ntp-servers", 0, &servers) == 0) {
			FREE(servers);
			int x = 0;
			while(config_setting_get_string("ntp-servers", x, &servers) == 0) {
				strcpy(ntp_servers.server[x].host, servers);
				ntp_servers.server[x].port = 123;
				x++;
				FREE(servers);
			}
			ntp_servers.nrservers = x;
			ntp_servers.callback = NULL;
			ntpsync();
		}
	}

	{
		int itmp = 0;
		int watchdog = 1, stats = 1;

		if(config_setting_get_number("watchdog-enable", 0, &itmp) == 0) { watchdog = itmp; }
		if(config_setting_get_number("stats-enable", 0, &itmp) == 0) { stats = itmp; }

		if(watchdog == 1 && stats == 1) {
			timer_abort_req = MALLOC(sizeof(uv_timer_t));
			if(timer_abort_req == NULL) {
				OUT_OF_MEMORY
			}
			uv_timer_init(uv_default_loop(), timer_abort_req);
			uv_timer_start(timer_abort_req, pilight_abort, 9000, -1);
		}

		timer_stats_req = MALLOC(sizeof(uv_timer_t));
		if(timer_stats_req == NULL) {
			OUT_OF_MEMORY
		}
		uv_timer_init(uv_default_loop(), timer_stats_req);
		uv_timer_start(timer_stats_req, pilight_stats, 1000, 3000);
	}

	return EXIT_SUCCESS;

clear:
	if(fconfig != NULL) {
		FREE(fconfig);
	}
	if(nodaemon == 0) {
		log_level_set(LOG_NOTICE);
		log_shell_disable();
	}
	if(main_loop == 1) {
		main_gc();
	}
	uv_stop(uv_default_loop());
	return EXIT_FAILURE;
}

int main(int argc, char **argv) {
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	

	pilight.running = 0;
	pilight.debuglevel = 0;

	int ret = start_pilight(argc, argv);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	printf("%d\n", xfree());
	return ret;
}
