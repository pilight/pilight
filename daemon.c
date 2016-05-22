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
#include <unistd.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <windows.h>
	#include <ws2tcpip.h>
	#include <winsvc.h>
	#include <shellapi.h>
	#define MSG_NOSIGNAL 0
#else
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>

#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/timerpool.h"
#include "libs/pilight/core/threadpool.h"
#include "libs/pilight/core/eventpool.h"
#include "libs/pilight/core/datetime.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/gc.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/arp.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/json.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/dso.h"
#include "libs/pilight/core/firmware.h"
#include "libs/pilight/core/proc.h"
#include "libs/pilight/core/ntp.h"
#include "libs/pilight/core/ssl.h"
#include "libs/pilight/storage/storage.h"

#include "libs/pilight/core/mail.h"

#ifdef EVENTS
	#include "libs/pilight/events/events.h"
	#include "libs/pilight/events/action.h"
	#include "libs/pilight/events/function.h"
	#include "libs/pilight/events/operator.h"
#endif

#ifdef WEBSERVER
	#ifdef WEBSERVER_HTTPS
		#include "libs/mbedtls/mbedtls/md5.h"
	#endif
	#include "libs/pilight/core/webserver.h"
#endif

#include "libs/pilight/hardware/hardware.h"
#include "libs/pilight/protocols/protocol.h"

#include "libs/wiringx/wiringX.h"
#include "libs/wiringx/platform/platform.h"

#ifdef _WIN32
static char server_name[40];
#endif

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
	double ram;
	struct clients_t *next;
} clients_t;

typedef struct ssdp_data_t {
	char server[INET_ADDRSTRLEN+1];
	char name[17];
	char uuid[UUID_LENGTH+1];
	int port;
} ssdp_data_t;

typedef struct adhoc_data_t {
	int steps;
	int connected;
	int reading;
} adhoc_data_t;

static struct clients_t *clients = NULL;

static pthread_mutex_t sendmutexlock;
static unsigned short sendmutexinit = 0;

static pthread_mutex_t broadcastmutexlock;
static unsigned short broadcastmutexinit = 0;

static struct protocol_t *procProtocol;

/* The pid_file and pid of this daemon */
#ifndef _WIN32
static char *pid_file;
static unsigned short pid_file_free = 0;
#endif
static pid_t pid;
/* Daemonize or not */
static int nodaemon = 0;
/* Are we already running */
static int active = 1;
/* Are we currently sending code */
static int sending = 0;
/* Socket identifier to the server if we are running as client */
static int sockfd = 0;
/* While loop conditions */
static unsigned short main_loop = 1;
/* Are we running standalone */
static int standalone = 0;
/* Do we need to connect to a master server:port? */
static char *master_server = NULL;
static unsigned short master_port = 0;

struct adhoc_data_t *adhoc_data = NULL;

static char *configtmp = NULL;
static int verbosity = LOG_NOTICE;

static struct eventpool_fd_t *clientize_client = NULL;

#ifdef _WIN32
	static int console = 0;
	static int oldverbosity = LOG_NOTICE;
#endif

#ifdef WEBSERVER
static int webserver_enable = WEBSERVER_ENABLE;
static int webserver_http_port = WEBSERVER_HTTP_PORT;
static int webserver_https_port = WEBSERVER_HTTPS_PORT;
static char *webserver_root = NULL;
static int webserver_root_free = 0;
#endif

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_forward_free(void *param) {
	FREE(param);
	return NULL;
}

static void *reason_code_sent_free(void *param) {
	struct reason_code_sent_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_send_code_free(void *param) {
	struct reason_send_code_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_socket_send_free(void *param) {
	struct reason_socket_send_t *data = param;
	FREE(data->buffer);
	FREE(data);
	return NULL;
}

static void *reason_broadcast_core_free(void *param) {
	char *code = param;
	FREE(code);
	return NULL;
}

static void *reason_broadcast_free(void *param) {
	char *code = param;
	FREE(code);
	return NULL;
}

static void *reason_adhoc_config_received_free(void *param) {
	json_delete(param);
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

static void receive_parse_api(struct JsonNode *code, int hwtype) {
	struct protocol_t *protocol = NULL;
	struct protocols_t *pnode = protocols;
	char message[255];

	while(pnode != NULL) {
		protocol = pnode->listener;

		if(protocol->hwtype == hwtype && protocol->parseCommand != NULL) {
			memset(message, 0, 255);
			protocol->parseCommand(code, message);
			/*
			 * FIXME
			 */
			// receiver_create_message(protocol, message);
		}
		pnode = pnode->next;
	}
}

void receive_parse_code(int *raw, int rawlen, int plslen, int hwtype) {
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
					protocol->parseCode(message);
					receiver_create_message(protocol, message);
				}
			}
		}
		pnode = pnode->next;
	}

	return;
}

void *receivePulseTrain(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_received_pulsetrain_t *data = task->userdata;
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

void *send_code(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_send_code_t *data = task->userdata;

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

	sending = 1;

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
	if(strlen(data->settings) > 0) {
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
	}

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

	sending = 0;

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

	buffer = json_stringify(json, NULL);
	tmp_clients = clients;
	while(tmp_clients) {
		if(tmp_clients->forward == 1) {
			socket_write(tmp_clients->id, buffer);
		}
		tmp_clients = tmp_clients->next;
	}
	json_free(buffer);

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
				if(protocol->createCode(jcode, message) == 0 && main_loop == 1) {
					struct reason_send_code_t *data = MALLOC(sizeof(struct reason_send_code_t));
					if(data == NULL) {
						OUT_OF_MEMORY
					}
					data->origin = origin;
					snprintf(data->message, 1024, "\"message\":%s", message);
					data->rawlen = protocol->rawlen;
					memcpy(data->pulses, protocol->raw, protocol->rawlen*sizeof(int));
					data->txrpt = protocol->txrpt;
					strncpy(data->protocol, protocol->id, 255);
					data->hwtype = protocol->hwtype;

					struct JsonNode *jtmp = NULL;
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
					strncpy(&data->settings[x-1], "}", 1024-x);

					if(uuid != NULL) {
						strcpy(data->uuid, uuid);
					} else {
						memset(data->uuid, 0, UUID_LENGTH+1);
					}
					eventpool_trigger(REASON_SEND_CODE, reason_send_code_free, data);

					return 0;
				} else {
					return -1;
				}
			}
		} else {
			return 0;
		}
	}
	return -1;
}

/*
 * This function construct a struct like this:
 * {
 * 		"code": {
 * 					"id": 11,
 * 					"off": 1,
 * 					"protocol": [ "generic_switch" ]
 * 	},
 * 	"action": "send"
 * }
*/
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

#ifdef WEBSERVER
static void client_webserver_parse_code(int i, char buffer[BUFFER_SIZE]) {
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
		inet_ntop(AF_INET, (void *)&(sockin.sin_addr), ip, INET_ADDRSTRLEN+1);

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
		eventpool_fd_write(i, buff, strlen(buff));
		eventpool_fd_write(i, line, l);
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
					char *sval = NULL;
					double nval = 0.0;
					int dec = 0;
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
								if(value->tag == JSON_NUMBER) {
									if(registry_update(ORIGIN_MASTER, key, json_mknumber(value->number_, value->decimals_)) == 0) {
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
								} else if(value->tag == JSON_STRING) {
									if(registry_update(ORIGIN_MASTER, key, json_mkstring(value->string_)) == 0) {
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
									logprintf(LOG_ERR, "registry value can only be a string or number");
									if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
										OUT_OF_MEMORY
									}
									strcpy(*respons, "{\"status\":\"failed\"}");
									json_delete(json);
									return 0;
								}
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
								if(registry_delete(ORIGIN_MASTER, key) == 0) {
									if((*respons = MALLOC(strlen("{\"status\":\"success\"}")+1)) == NULL) {
										OUT_OF_MEMORY
									}
									strcpy(*respons, "{\"status\":\"success\"}");
									json_delete(json);
									return 0;
								} else {
									logprintf(LOG_ERR, "registry value can only be a string or number");
									if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
										OUT_OF_MEMORY
									}
									strcpy(*respons, "{\"status\":\"failed\"}");
									json_delete(json);
									return 0;
								}
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
								if(registry_select_number(ORIGIN_MASTER, key, &nval, &dec) == 0) {
									struct JsonNode *jsend = json_mkobject();
									json_append_member(jsend, "message", json_mkstring("registry"));
									json_append_member(jsend, "value", json_mknumber(nval, dec));
									json_append_member(jsend, "key", json_mkstring(key));
									char *output = json_stringify(jsend, NULL);
									if((*respons = MALLOC(strlen(output)+1)) == NULL) {
										OUT_OF_MEMORY
									}
									strcpy(*respons, output);
									json_free(output);
									json_delete(jsend);
									json_delete(json);
									return 0;
								} else if(registry_select_string(ORIGIN_MASTER, key, &sval) == 0) {
									struct JsonNode *jsend = json_mkobject();
									json_append_member(jsend, "message", json_mkstring("registry"));
									json_append_member(jsend, "value", json_mkstring(sval));
									json_append_member(jsend, "key", json_mkstring(key));
									char *output = json_stringify(jsend, NULL);
									if((*respons = MALLOC(strlen(output)+1)) == NULL) {
										OUT_OF_MEMORY
									}
									strcpy(*respons, output);
									json_free(output);
									json_delete(jsend);
									json_delete(json);
									return 0;
								} else {
									logprintf(LOG_ERR, "registry key '%s' doesn't exists", key);
									if((*respons = MALLOC(strlen("{\"status\":\"failed\"}")+1)) == NULL) {
										OUT_OF_MEMORY
									}
									strcpy(*respons, "{\"status\":\"failed\"}");
									json_delete(json);
									return 0;
								}
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
static void *socket_parse_data(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_socket_received_t *data = task->userdata;

	struct sockaddr_in address;
	socklen_t socklen = sizeof(address);

	struct JsonNode *json = NULL;
	struct JsonNode *options = NULL;
	struct clients_t *tmp_clients = NULL;
	struct clients_t *client = NULL;
	char *action = NULL, *media = NULL, *status = NULL, *respons = NULL;
	char all[] = "all";
	int error = 0, exists = 0, sd = -1, i = 0;

	if(strlen(data->type) == 0) {
		logprintf(LOG_ERR, "socket data misses a socket type");
	}

	if(data->buffer == NULL) {
		logprintf(LOG_ERR, "socket message incorrectly formatted");
		return NULL;
	}

	// if(pilight.runmode == ADHOC) {
		// sd = sockfd;
	// } else {
		sd = data->fd;
		getpeername(sd, (struct sockaddr*)&address, &socklen);
	// }

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

	/* Serve static webserver page. This is the only request that is
		 expected not to be a json object */
#ifdef WEBSERVER
	if(strstr(data->buffer, " HTTP/") != NULL) {
		client_webserver_parse_code(i, data->buffer);
		return NULL;
	} else if(json_validate(data->buffer) == true) {
#else
	if(json_validate(data->buffer) == true) {
#endif
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
					client->ram = 0;
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
							json_find_number(jvalues, "ram", &client->ram);
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
		logprintf(LOG_ERR, "could not parse respons to: %s", data->buffer);
		client_remove(sd);
		socket_close(sd);
		json_delete(json);
		return NULL;
	}
	json_delete(json);
	return NULL;
}

static void *socket_client_disconnected(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_socket_disconnected_t *data = task->userdata;

	if(data->fd > -1) {
		client_remove(data->fd);
	}
	return NULL;
}

void *broadcast(void *param) {
	pthread_mutex_lock(&sendmutexlock);

	struct threadpool_tasks_t *task = param;
	char /**threadid = NULL, */ *protocol = NULL, *origin = NULL;
	char *message = NULL, *uuid = NULL, *settings = NULL, *conf = NULL;
	char internal[1025];
	int repeats = 0, type = 0, len = 0, len1 = 0;
	int broadcasted = 0, free_conf = 1;

	if((conf = MALLOC(1025)) == NULL) {
		OUT_OF_MEMORY
	}

	switch(task->reason) {
		case REASON_FORWARD: {
			strncpy(conf, task->userdata, 1024);
		} break;
		case REASON_CODE_RECEIVED: {
			struct reason_code_received_t *data = task->userdata;
			protocol = data->protocol;
			origin = data->origin;
			message = data->message;
			uuid = data->uuid;
			repeats = data->repeat;
			len = snprintf(conf, 1024, "{\"message\":%s,\"origin\":\"%s\",\"protocol\":\"%s\",\"uuid\":\"%s\",\"repeats\":%d}",
				message, origin, protocol, uuid, repeats);
		} break;
		case REASON_CODE_SENT: {
			struct reason_code_sent_t *data = task->userdata;
			type = data->type;
			if(strlen(data->protocol) > 0) {
				protocol = data->protocol;
			}
			message = data->message;
			if(strlen(data->uuid) > 0) {
				uuid = data->uuid;
			}
			repeats = data->repeat;
			if(strlen(data->settings) > 0) {
				settings = data->settings;
			}
			origin = data->origin;
			/* The settings objects inside the broadcast queue is only of interest for the
			 internal pilight functions. For the outside world we only communicate the
			 message part of the queue so we remove the settings */
			len1 += snprintf(&internal[len1], 1024-len1, "{");
			len += snprintf(&conf[len], 1024-len, "{");
			if(message != NULL) {
				len1 += snprintf(&internal[len1], 1024-len1, "%s,", message);
				len += snprintf(&conf[len], 1024-len, "%s,", message);
			}
			if(protocol != NULL) {
				len1 += snprintf(&internal[len1], 1024-len1, "\"protocol\":\"%s\",", protocol);
				len += snprintf(&conf[len], 1024-len, "\"protocol\":\"%s\",", protocol);
			}
			if(uuid != NULL) {
				len1 += snprintf(&internal[len1], 1024-len1, "\"uuid\":\"%s\",", uuid);
				len += snprintf(&conf[len], 1024-len, "\"uuid\":\"%s\",", uuid);
			}
			if(repeats > 0) {
				len1 += snprintf(&internal[len1], 1024-len1, "\"repeats\":%d,", repeats);
				len += snprintf(&conf[len], 1024-len, "\"repeats\":%d}", repeats);
			} else {
				len += snprintf(&conf[len-1], 1024-len, "}");
			}

			if(settings != NULL) {
				len1 += snprintf(&internal[len1], 1024-len1, "\"settings\":%s}", settings);
			} else {
				len1 += snprintf(&internal[len1-1], 1024-len1, "}");
			}
		} break;
		default: {
			FREE(conf);
			pthread_mutex_unlock(&sendmutexlock);
			return NULL;
		} break;
	}
	// json_find_string(jdata, "thread", &threadid);

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

		// struct JsonNode *childs = json_first_child(jdata);
		// int nrchilds = 0;
		// while(childs) {
			// nrchilds++;
			// childs = childs->next;
		// }

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

	pthread_mutex_unlock(&sendmutexlock);

	return NULL;
}

static void *adhoc_reconnect(void *param) {
	if(adhoc_data->connected == 0) {
		struct timeval tv;
		tv.tv_sec = 3;
		tv.tv_usec = 0;
		threadpool_add_scheduled_work("adhoc mode reconnect", adhoc_reconnect, tv, NULL);
		ssdp_seek();
	}

	return NULL;
}

static int clientize(struct eventpool_fd_t *node, int event) {
	struct timeval tv;
	struct ssdp_data_t *ssdp_data = node->userdata;

	adhoc_data->reading = 0;
	switch(event) {
		case EV_CONNECT_SUCCESS: {
			adhoc_data->connected = 1;
			eventpool_fd_enable_write(node);
		} break;
		case EV_READ: {
			switch(adhoc_data->steps) {
				case 1: {
					char c = 0;
					/* Connection lost */
					if(recv(node->fd, &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
						return -1;
					}

					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						logprintf(LOG_DEBUG, "socket recv: %s", node->buffer);
						if(strcmp("{\"status\":\"success\"}", node->buffer) == 0) {
							sockfd = node->fd;
							adhoc_data->steps = 2;
							eventpool_fd_enable_write(node);
							eventpool_trigger(REASON_ADHOC_CONNECTED, NULL, NULL);
							FREE(node->buffer);
							node->len = 0;
						}
					}
				} break;
				case 3: {
					struct JsonNode *jchilds = NULL;
					struct JsonNode *tmp = NULL;
					char *message = NULL;
					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						logprintf(LOG_DEBUG, "socket recv: %s", node->buffer);
						if(json_validate(node->buffer) == true) {
							struct JsonNode *json = json_decode(node->buffer);
							if(json_find_string(json, "message", &message) == 0) {
								if(strcmp(message, "config") == 0) {
									struct JsonNode *jconfig = NULL;
									if((jconfig = json_find_member(json, "config")) != NULL) {
										int match = 1;
										while(match) {
											jchilds = json_first_child(jconfig);
											match = 0;
											while(jchilds) {
												tmp = jchilds;
												if(strcmp(tmp->key, "devices") != 0) {
													json_remove_from_parent(tmp);
													match = 1;
												}
												jchilds = jchilds->next;
												if(match == 1) {
													json_delete(tmp);
												}
											}
										}

										struct JsonNode *jout = NULL;
										if(json_clone(jconfig, &jout) == 0) {
											eventpool_trigger(REASON_ADHOC_CONFIG_RECEIVED, reason_adhoc_config_received_free, jout);
										}

										adhoc_data->steps = 4;
										eventpool_fd_enable_read(node);
									}
								}
							}
							json_delete(json);
						}
						FREE(node->buffer);
						node->len = 0;
					}
				} break;
				case 4: {
					int x = socket_recv(node->fd, &node->buffer, &node->len);
					if(x == -1) {
						return -1;
					} else if(x == 0) {
						eventpool_fd_enable_read(node);
						return 0;
					} else {
						char *action = NULL, *origin = NULL, *protocol = NULL;
						struct JsonNode *json = NULL;
						logprintf(LOG_DEBUG, "socket recv: %s", node->buffer);
						char **array = NULL;
						unsigned int z = explode(node->buffer, "\n", &array), q = 0;
						for(q=0;q<z;q++) {
							if(json_validate(array[q]) == true) {
								json = json_decode(array[q]);

								if(json_find_string(json, "action", &action) == 0) {
									if(strcmp(action, "send") == 0 /*||
										 strcmp(action, "control") == 0*/) {

										char *respons = NULL;
										if(socket_parse_responses(array[q], "all", &respons) == 0) {
											socket_write(node->fd, respons);
										}/* else {
											logprintf(LOG_ERR, "could not parse respons to: %s", buffer);
											client_remove(sd);
											socket_close(sd);
											return NULL;
										}*/
										FREE(respons);
									}
								} else if(json_find_string(json, "origin", &origin) == 0 &&
										json_find_string(json, "protocol", &protocol) == 0) {
									if(strcmp(origin, "receiver") == 0 ||
											 strcmp(origin, "sender") == 0) {
										char *out = MALLOC(strlen(array[q])+1);
										if(out == NULL) {
											OUT_OF_MEMORY
										}
										strcpy(out, array[q]);
										eventpool_trigger(REASON_FORWARD, reason_forward_free, out);
									}
								}
								json_delete(json);
							}
							array_free(&array, z);
						}
						FREE(node->buffer);
						node->len = 0;
					}
					eventpool_fd_enable_read(node);
				} break;
			}
		} break;
		case EV_WRITE: {
			switch(adhoc_data->steps) {
				case 0: {
					struct JsonNode *json = json_mkobject();
					struct JsonNode *joptions = json_mkobject();
					json_append_member(json, "action", json_mkstring("identify"));
					json_append_member(joptions, "receiver", json_mknumber(1, 0));
					json_append_member(joptions, "forward", json_mknumber(1, 0));
					json_append_member(joptions, "config", json_mknumber(1, 0));
					json_append_member(json, "uuid", json_mkstring(pilight_uuid));
					json_append_member(json, "options", joptions);

					char *output = json_stringify(json, NULL);
					socket_write(node->fd, output);
					json_free(output);
					json_delete(json);

					adhoc_data->steps = 1;

					eventpool_fd_enable_read(node);
				} break;
				case 2: {
					struct JsonNode *json = json_mkobject();
					json_append_member(json, "action", json_mkstring("request config"));
					char *output = json_stringify(json, NULL);

					socket_write(node->fd, output);

					json_free(output);
					json_delete(json);

					adhoc_data->steps = 3;

					eventpool_fd_enable_read(node);
				} break;
			}
		} break;
		case EV_DISCONNECTED: {
			clientize_client = NULL;
			adhoc_data->steps = 0;
			adhoc_data->connected = 0;

			tv.tv_sec = 3;
			tv.tv_usec = 0;
			logprintf(LOG_DEBUG, "lost connection to master server");
			threadpool_add_scheduled_work("adhoc mode reconnect", adhoc_reconnect, tv, NULL);
			eventpool_trigger(REASON_ADHOC_DISCONNECTED, NULL, NULL);

			eventpool_fd_remove(node);
			FREE(ssdp_data);
		} break;
	}
	return 0;
}

static void *ssdp_found(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_ssdp_received_t *data = task->userdata;

	char *master = NULL;
	int match = 0;

	if(data->ip != NULL && data->port > 0 && data->name != NULL && data->uuid != NULL) {

		if(settings_select_string(ORIGIN_MASTER, "adhoc-master", &master) == 0) {
			if(strcmp(master, data->name) == 0) {
				match = 1;
			}
		}

		if(match == 1) {
			struct ssdp_data_t *ssdp_data = MALLOC(sizeof(struct ssdp_data_t));
			if(ssdp_data == NULL) {
				OUT_OF_MEMORY
			}

			strncpy(ssdp_data->server, data->ip, INET_ADDRSTRLEN);
			ssdp_data->port = data->port;
			strncpy(ssdp_data->uuid, data->uuid, UUID_LENGTH);

			logprintf(LOG_NOTICE, "pilight master daemon \"%s\" was found @%s, clientizing", data->name, data->ip);
			pilight.runmode = ADHOC;

			adhoc_data->connected = 0;
			adhoc_data->steps = 0;
			if(clientize_client != NULL) {
				eventpool_fd_remove(clientize_client);
			}
			clientize_client = eventpool_socket_add("clientize", ssdp_data->server, ssdp_data->port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, clientize, NULL, ssdp_data);
		}
	}
	return NULL;
}

#ifndef _WIN32
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
#endif

/* Garbage collector of main program */
int main_gc(void) {
	active = 0;
	pilight.running = 0;
	main_loop = 0;

	/* If we are running in node mode, the clientize
	   thread is waiting for a response from the main
	   daemon. This means we can't gracefull stop that
	   thread. However, by sending a HEART message the
	   main daemon will response with a BEAT. This allows
	   us to stop the socket_read function and properly
	   stop the clientize thread. */
	if(pilight.runmode == ADHOC) {
		socket_write(sockfd, "HEART");
	}

	pilight.runmode = STANDALONE;

	timer_thread_gc();
	threadpool_gc();
	eventpool_gc();
	while(sending == 1) {
		usleep(1000);
	}
#ifdef EVENTS
	events_gc();
#endif

	struct clients_t *tmp_clients;
	while(clients) {
		tmp_clients = clients;
		clients = clients->next;
		FREE(tmp_clients);
	}
	if(clients != NULL) {
		FREE(clients);
	}

#ifndef _WIN32
	if(active == 0) {
		if(pid_file != NULL) {
			/* Remove the stale pid file */
			if(access(pid_file, F_OK) != -1) {
				if(remove(pid_file) != -1) {
					logprintf(LOG_INFO, "removed stale pid_file %s", pid_file);
				} else {
					logprintf(LOG_ERR, "could not remove stale pid file %s", pid_file);
				}
			}
		}
	}
	if(pid_file_free) {
		FREE(pid_file);
	}
#endif
#ifdef WEBSERVER
	if(webserver_enable == 1) {
		webserver_gc();
	}
	if(webserver_root_free == 1) {
		FREE(webserver_root);
	}
#endif

	if(master_server != NULL) {
		FREE(master_server);
	}

	datetime_gc();
	options_gc();
	socket_gc();

	hardware_gc();
	storage_gc();
	protocol_gc();
	ntp_gc();
	whitelist_free();
#ifndef _WIN32
	wiringXGC();
#endif
	dso_gc();
	log_gc();
	if(configtmp != NULL) {
		FREE(configtmp);
	}

	ssl_gc();
	proc_gc();
	gc_clear();
	FREE(progname);

#ifdef _WIN32
	WSACleanup();
	if(console == 1) {
		FreeConsole();
	}
#endif

	return 0;
}

static void procProtocolInit(void) {
	protocol_register(&procProtocol);
	protocol_set_id(procProtocol, "process");
	protocol_device_add(procProtocol, "process", "pilight proc. API");
	procProtocol->devtype = PROCESS;
	procProtocol->hwtype = API;
	procProtocol->multipleId = 0;
	procProtocol->config = 0;

	options_add(&procProtocol->options, 'c', "cpu", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&procProtocol->options, 'r', "ram", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
}

#ifndef _WIN32
#pragma GCC diagnostic push  // require GCC 4.6
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
void registerVersion(void) {
	registry_delete(ORIGIN_MASTER, "pilight.version");
	registry_update(ORIGIN_MASTER, "pilight.version.current", json_mkstring((char *)PILIGHT_VERSION));
}
#ifndef _WIN32
#pragma GCC diagnostic pop   // require GCC 4.6
#endif

#ifdef _WIN32
void closeconsole(void) {
	log_shell_disable();
	verbosity = oldverbosity;
	FreeConsole();
	console = 0;
}

BOOL CtrlHandler(DWORD fdwCtrlType) {
	closeconsole();
	return TRUE;
}

void openconsole(void) {
	DWORD lpMode;
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	HWND hWnd = GetConsoleWindow();
	if(hWnd != NULL) {
		GetConsoleMode(hWnd, &lpMode);
		SetConsoleMode(hWnd, lpMode & ~ENABLE_PROCESSED_INPUT);
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
		HMENU hMenu = GetSystemMenu(hWnd, FALSE);
		if(hMenu != NULL) {
			RemoveMenu(hMenu, SC_CLOSE, MF_GRAYED);
			RemoveMenu(hMenu, SC_MINIMIZE, MF_GRAYED);
			RemoveMenu(hMenu, SC_MAXIMIZE, MF_GRAYED);
		}

		console = 1;
		oldverbosity = verbosity;
		verbosity = LOG_DEBUG;
		log_level_set(verbosity);
		log_shell_enable();
	}
}
#endif

void *pilight_stats(void *param) {
	struct timeval tv;
	tv.tv_sec = 3;
	tv.tv_usec = 0;

	int checkram = 0, checkcpu = 0, i = -1, x = 0, watchdog = 1, stats = 1;
	double itmp = 0.0;
	if(settings_select_number(ORIGIN_MASTER, "watchdog-enable", &itmp) == 0) { watchdog = (int)itmp; }
	if(settings_select_number(ORIGIN_MASTER, "stats-enable", &itmp) == 0) { stats = (int)itmp; }

	if(pilight.runmode == STANDALONE) {
		registerVersion();
	}

	if(stats == 1) {
		threadpool_add_scheduled_work("pilight stats", pilight_stats, tv, NULL);
		double cpu = 0.0, ram = 0.0;
		cpu = getCPUUsage();
		ram = getRAMUsage();

		if(watchdog == 1 && (i > -1) && (cpu > 60)) {
			if(nodaemon == 1) {
				x ^= 1;
			}
			if(checkcpu == 0) {
				if(cpu > 90) {
					logprintf(LOG_CRIT, "cpu usage way too high %f%%", cpu);
				} else {
					logprintf(LOG_WARNING, "cpu usage too high %f%%", cpu);
				}
				logprintf(LOG_WARNING, "checking again in 10 seconds");
				sleep(10);
			} else {
				if(cpu > 90) {
					logprintf(LOG_ALERT, "cpu usage still way too high %f%%, exiting", cpu);
				} else {
					logprintf(LOG_CRIT, "cpu usage still too high %f%%, stopping", cpu);
				}
			}
			if(checkcpu == 1) {
				if(cpu > 90) {
					exit(EXIT_FAILURE);
				} else {
					main_gc();
				}
			}
			checkcpu = 1;
		} else if(watchdog == 1 && (i > -1) && (ram > 60)) {
			if(checkram == 0) {
				if(ram > 90) {
					logprintf(LOG_CRIT, "ram usage way too high %f%%", ram);
					exit(EXIT_FAILURE);
				} else {
					logprintf(LOG_WARNING, "ram usage too high %f%%", ram);
				}
				logprintf(LOG_WARNING, "checking again in 10 seconds");
				sleep(10);
			} else {
				if(ram > 90) {
					logprintf(LOG_ALERT, "ram usage still way too high %f%%, exiting", ram);
				} else {
					logprintf(LOG_CRIT, "ram usage still too high %f%%, stopping", ram);
				}
			}
			if(checkram == 1) {
				if(ram > 90) {
					exit(EXIT_FAILURE);
				} else {
					main_gc();
				}
			}
			checkram = 1;
		} else {
			checkcpu = 0;
			checkram = 0;
			if((i > 0 && i%3 == 0) || (i == -1)) {
				struct reason_code_sent_t *data = MALLOC(sizeof(struct reason_code_sent_t));
				memset(data, '\0', sizeof(struct reason_code_sent_t));
				if(data == NULL) {
					OUT_OF_MEMORY
				}
				data->type = PROCESS;
				strcpy(data->origin, "core");
				snprintf(data->message, 1024, "\"values\":{\"cpu\":%f,\"ram\":%f},\"origin\":\"core\",\"type\":%d", cpu, ram, PROCESS);
				logprintf(LOG_DEBUG, "cpu: %f%%, ram: %f%%", cpu, ram);
				struct clients_t *tmp_clients = clients;
				while(tmp_clients) {
					if(tmp_clients->cpu > 0 && tmp_clients->ram > 0) {
						logprintf(LOG_DEBUG, "- client: %s cpu: %f%%, ram: %f%%",
									tmp_clients->uuid, tmp_clients->cpu, tmp_clients->ram);
					}
					tmp_clients = tmp_clients->next;
				}
				eventpool_trigger(REASON_CODE_SENT, reason_code_sent_free, data->message);

				i = 0;
				x = 0;
			}
			i++;
		}
	}

	return (void *)NULL;
}

int start_pilight(int argc, char **argv) {
	struct options_t *options = NULL;

#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
	/* Make sure the pilight receiving gets
	   the highest priority available */
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = 1;
	pthread_setschedparam(pthread_self(), SCHED_RR, &sched);
#endif

	char buffer[BUFFER_SIZE], *fconfig = NULL;
	int show_default = 0, show_version = 0, show_help = 0;
	double itmp = 0.0;
#ifndef _WIN32
	int f = 0;
#endif
	char *stmp = NULL, *args = NULL, *p = NULL;
	int port = 0;

	// mempool_init(8192, 1024);

	pilight.process = PROCESS_DAEMON;

	if((progname = MALLOC(16)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(progname, "pilight-daemon");

	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(fconfig, CONFIG_FILE);

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'D', "nodaemon", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'C', "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'S', "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, 258, "debuglevel", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[012]{1}");
	// options_add(&options, 258, "memory-tracer", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	while(1) {
		int c = options_parse(&options, argc, argv, 1, &args);
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
				if((master_server = MALLOC(strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(master_server, args);
			break;
			case 'P':
				master_port = (unsigned short)atoi(args);
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
#ifdef _WIN32
		strcpy(tabs, "\t");
#else
		strcpy(tabs, "\t\t");
#endif
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
#ifdef _WIN32
		MessageBox(NULL, help, "pilight :: info", MB_OK);
#else
		printf("%s", help);
#if defined(__arm__) || defined(__mips__)
		printf("\n\tThe following GPIO platforms are supported:\n");
		char *tmp = NULL;
		int z = 0;
		wiringXSetup("", logprintf);
		printf("\t- none\n");
		while((tmp = platform_iterate_name(z++)) != NULL) {
			printf("\t- %s\n", tmp);
		}
		printf("\n");
#endif
#endif
		goto clear;
	}
	if(show_version == 1) {
#ifdef HASH
	#ifdef _WIN32
			char version[50];
			snprintf(version, 50, "%s version %s", progname, HASH);
			MessageBox(NULL, version, "pilight :: info", MB_OK);
	#else
			printf("%s version %s\n", progname, HASH);
	#endif
#else
	#ifdef _WIN32
			char version[50];
			snprintf(version, 50, "%s version %s", progname, PILIGHT_VERSION);
			MessageBox(NULL, version, "pilight :: info", MB_OK);
	#else
			printf("%s version %s\n", progname, PILIGHT_VERSION);
	#endif
#endif
		goto clear;
	}
	if(show_default == 1) {
#ifdef _WIN32
		char def[50];
		snprintf(def, 50, "Usage: %s [options]\n", progname);
		MessageBox(NULL, def, "pilight :: info", MB_OK);
#else
		printf("Usage: %s [options]\n", progname);
#endif
		goto clear;
	}

#ifdef _WIN32
	if(nodaemon == 1) {
		openconsole();
		console = 1;
	}
#endif

	datetime_init();
	atomicinit();
	procProtocolInit();

#ifndef _WIN32
	if(geteuid() != 0) {
		printf("%s requires root privileges in order to run\n", progname);
		FREE(progname);
		exit(EXIT_FAILURE);
	}
#endif

	gc_attach(main_gc);
	gc_catch();

	log_level_set(LOG_INFO);
	log_file_enable();
	log_shell_disable();

	int nrdevs = 0, x = 0;
	char **devs = NULL;
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

	memset(buffer, '\0', BUFFER_SIZE);

	if(nodaemon == 1) {
		log_level_set(verbosity);
		log_shell_enable();
	}


#ifdef _WIN32
	if((pid = check_instances(L"pilight-daemon")) != -1) {
		logprintf(LOG_NOTICE, "pilight is already running");
		goto clear;
	}
#endif

	if((pid = isrunning("pilight-raw")) != -1) {
		logprintf(LOG_NOTICE, "pilight-raw instance found (%d)", (int)pid);
		goto clear;
	}

	if((pid = isrunning("pilight-debug")) != -1) {
		logprintf(LOG_NOTICE, "pilight-debug instance found (%d)", (int)pid);
		goto clear;
	}

	/* Export certain daemon function to global usage */
	// pilight.broadcast = &broadcast_queue;
	pilight.send = &send_queue;
	pilight.control = &control_device;
	pilight.receive = &receive_parse_api;
	pilight.socket = &socket_parse_responses;

	protocol_init();
	hardware_init();
	event_init();
	storage_init();

	if(storage_read(fconfig, CONFIG_ALL) != 0) {
		goto clear;
	}
	FREE(fconfig);

	registerVersion();

	pthread_mutex_init(&sendmutexlock, NULL);
	pthread_mutex_init(&broadcastmutexlock, NULL);
	sendmutexinit = 1;
	broadcastmutexinit = 1;

	eventpool_callback(REASON_SEND_CODE, send_code);
	eventpool_callback(REASON_CODE_RECEIVED, broadcast);
	eventpool_callback(REASON_CODE_SENT, broadcast);
	eventpool_callback(REASON_FORWARD, broadcast);
	eventpool_callback(REASON_SOCKET_RECEIVED, socket_parse_data);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_client_disconnected);
	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain);

#ifdef WEBSERVER
	#ifdef WEBSERVER_HTTPS
	char *pemfile = NULL;
	int pem_free = 0;
	if(settings_select_string(ORIGIN_MASTER, "pem-file", &pemfile) != 0) {
		if((pemfile = REALLOC(pemfile, strlen(PEM_FILE)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(pemfile, PEM_FILE);
		pem_free = 1;
	}

	char *content = NULL;
	unsigned char md5sum[17];
	char md5conv[33];
	int i = 0;
	p = (char *)md5sum;
	if(file_exists(pemfile) != 0) {
		logprintf(LOG_ERR, "missing webserver SSL private key %s", pemfile);
		if(pem_free == 1) {
			FREE(pemfile);
		}
		goto clear;
	}
	if(file_get_contents(pemfile, &content) == 0) {
		mbedtls_md5((const unsigned char *)content, strlen((char *)content), (unsigned char *)p);
		for(i = 0; i < 32; i+=2) {
			sprintf(&md5conv[i], "%02x", md5sum[i/2] );
		}
		if(strcmp(md5conv, PILIGHT_PEM_MD5) == 0) {
			registry_update(ORIGIN_MASTER, "webserver.ssl.certificate.secure", json_mknumber(0, 0));
		} else {
			registry_update(ORIGIN_MASTER, "webserver.ssl.certificate.secure", json_mknumber(1, 0));
		}
		registry_update(ORIGIN_MASTER, "webserver.ssl.certificate.location", json_mkstring(pemfile));
		FREE(content);
	}

	if(pem_free == 1) {
		FREE(pemfile);
	}
	#endif

	if(settings_select_number(ORIGIN_MASTER, "webserver-enable", &itmp) == 0) { webserver_enable = (int)itmp; }
	if(settings_select_number(ORIGIN_MASTER, "webserver-http-port", &itmp) == 0) { webserver_http_port = (int)itmp; }
	if(settings_select_number(ORIGIN_MASTER, "webserver-https-port", &itmp) == 0) { webserver_https_port = (int)itmp; }
	if(settings_select_string(ORIGIN_MASTER, "webserver-root", &webserver_root) != 0) {
		if((webserver_root = REALLOC(webserver_root, strlen(WEBSERVER_ROOT)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(webserver_root, WEBSERVER_ROOT);
		webserver_root_free = 1;
	}
#endif

#ifndef _WIN32
	if(settings_select_string(ORIGIN_MASTER, "pid-file", &pid_file) != 0) {
		if((pid_file = REALLOC(pid_file, strlen(PID_FILE)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(pid_file, PID_FILE);
		pid_file_free = 1;
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
		}
	} else {
		logprintf(LOG_ERR, "could not open / create pid_file %s", pid_file);
		goto clear;
	}
	close(f);
#endif

	if(settings_select_number(ORIGIN_MASTER, "log-level", &itmp) == 0) {
		log_level_set((int)itmp);
	}

	if(settings_select_string(ORIGIN_MASTER, "log-file", &stmp) == 0) {
		if(log_file_set(stmp) == EXIT_FAILURE) {
			goto clear;
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

#ifndef _WIN32
	if(active == 1) {
		nodaemon = 1;
		logprintf(LOG_NOTICE, "already active (pid %d)", atoi(buffer));
		log_level_set(LOG_NOTICE);
		log_shell_disable();
		goto clear;
	}
#endif

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
									logprintf(LOG_EMERG, "%s: setting \"rawlen\" length is not allowed, use the \"minrawlen\" and \"maxrawlen\" instead", tmp->listener->id);
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

	if(settings_select_number(ORIGIN_MASTER, "port", &itmp) == 0) { port = (int)itmp; }
	if(settings_select_number(ORIGIN_MASTER, "standalone", &itmp) == 0) { standalone = (int)itmp; }

	pilight.runmode = STANDALONE;
	if(standalone == 0 || (master_server != NULL && master_port > 0)) {
		if(master_server != NULL && master_port > 0) {
			if((sockfd = socket_connect(master_server, master_port)) == -1) {
				logprintf(LOG_NOTICE, "pilight daemon not found @%s, waiting for it to come online", master_server);
			} else {
				logprintf(LOG_INFO, "a pilight daemon was found, clientizing");
			}
			pilight.runmode = ADHOC;
		}
	}

	ssl_init();
	if(pilight.runmode == STANDALONE) {
		socket_start((unsigned short)port);
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

	timer_thread_start();

	int nrcores = getnrcpu();
	if(nrcores > 1) {
		logprintf(LOG_INFO, "pilight will dynamically use between 1 and %d cores", nrcores);
	}
	threadpool_init(1, nrcores, 10);
	eventpool_init(EVENTPOOL_THREADED);

	log_init();

	struct timeval tv;
	char *adhoc_mode = NULL;
	if(settings_select_string(ORIGIN_MASTER, "adhoc-mode", &adhoc_mode) == 0) {
		if(strcmp(adhoc_mode, "client") == 0) {
			/* The daemon running in client mode, register a seperate thread that
				 communicates with the server */
			if((adhoc_data = MALLOC(sizeof(struct adhoc_data_t))) == NULL) {
				OUT_OF_MEMORY
			}
			adhoc_data->steps = 0;
			tv.tv_sec = 3;
			tv.tv_usec = 0;
			threadpool_add_scheduled_work("adhoc mode reconnect", adhoc_reconnect, tv, NULL);
			adhoc_data->reading = 0;

			eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found);
			ssdp_seek();
		}
	}

	if(hardware_select(ORIGIN_MASTER, NULL, &jrespond) == 0) {
		jchilds = json_first_child(jrespond);
		while(jchilds && main_loop == 1) {
			if(hardware_select_struct(ORIGIN_MASTER, jchilds->key, &hardware) == 0) {
				if(hardware->init != NULL) {
					if(hardware->init(receivePulseTrain) == EXIT_FAILURE) {
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

	if(main_loop == 0) {
		goto clear;
	}

	char *ntpsync = NULL;
	if(settings_select_string_element(ORIGIN_MASTER, "ntp-servers", 0, &ntpsync) == 0) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		threadpool_add_scheduled_work("ntp sync", ntpthread, tv, NULL);
	}

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work("pilight stats", pilight_stats, tv, NULL);

	return EXIT_SUCCESS;

clear:
	FREE(fconfig);
	if(nodaemon == 0) {
		log_level_set(LOG_NOTICE);
		log_shell_disable();
	}
	if(main_loop == 1) {
		main_gc();
	}
	return EXIT_FAILURE;
}

#ifdef _WIN32
#define ID_TRAYICON 100
#define ID_QUIT 101
#define ID_SEPARATOR 102
#define ID_INSTALL_SERVICE 103
#define ID_REMOVE_SERVICE 104
#define ID_START 105
#define ID_STOP 106
#define ID_WEBGUI 107
#define ID_CONSOLE 108
#define ID_ICON 200
static NOTIFYICONDATA TrayIcon;

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  char buf[200], *service_argv[] = {__argv[0], NULL};
  POINT pt;
  HMENU hMenu;

  switch(msg) {
    case WM_CREATE:
			if(start_pilight(__argc, __argv) == EXIT_FAILURE) {
				if(main_loop == 1) {
					main_gc();
				}
				exit(EXIT_FAILURE);
			}
			active = 1;
		break;
    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case ID_QUIT:
					if(main_loop == 1) {
						main_gc();
					}
          Shell_NotifyIcon(NIM_DELETE, &TrayIcon);
          PostQuitMessage(0);
				break;
				case ID_STOP:
					if(active == 1) {
						if(main_loop == 1) {
							main_gc();
						}
						active = 0;
						MessageBox(NULL, "pilight stopped", "pilight :: notice", MB_OK);
					}
				break;
				case ID_CONSOLE:
					if(console == 0) {
						openconsole();
					} else {
						closeconsole();
					}
				break;
				case ID_START:
					if(active == 0) {
						main_loop = 1;
						if(start_pilight(1, service_argv) == EXIT_FAILURE) {
							if(main_loop == 1) {
								main_gc();
								MessageBox(NULL, "pilight failed to start", "pilight :: error", MB_OK);
							}
						}
						active = 1;
						MessageBox(NULL, "pilight started", "pilight :: notice", MB_OK);
					}
				break;
				case ID_WEBGUI:
					if(webserver_http_port == 80) {
						snprintf(buf, sizeof(buf), "http://localhost");
					} else {
						snprintf(buf, sizeof(buf), "http://localhost:%d", webserver_http_port);
					}
					ShellExecute(NULL, "open", buf, NULL, NULL, SW_SHOWNORMAL);
				break;
      }
		break;
    case WM_USER:
      switch(lParam) {
        case WM_RBUTTONUP:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
          hMenu = CreatePopupMenu();

          AppendMenu(hMenu, MF_STRING | MF_GRAYED, ID_SEPARATOR, server_name);
					AppendMenu(hMenu, MF_SEPARATOR, ID_SEPARATOR, "");
          AppendMenu(hMenu, MF_STRING | MF_GRAYED, ID_SEPARATOR, ((pilight.runmode == ADHOC) ? "Ad-Hoc mode" : "Standalone mode"));
					AppendMenu(hMenu, MF_SEPARATOR, ID_SEPARATOR, "");
					if(pilight.runmode == STANDALONE) {
						AppendMenu(hMenu, MF_STRING | (active ? 0 : MF_GRAYED), ID_WEBGUI, "Open webGUI");
					}
					if(console == 0) {
						AppendMenu(hMenu, MF_STRING, ID_CONSOLE, "Open console");
					} else {
						AppendMenu(hMenu, MF_STRING, ID_CONSOLE, "Close console");
					}
          AppendMenu(hMenu, MF_SEPARATOR, ID_SEPARATOR, "");

					// snprintf(buf, sizeof(buf), "pilight is %s", running ? "running" : "stopped");

					// AppendMenu(hMenu, MF_STRING | MF_GRAYED, ID_SEPARATOR, buf);
          // AppendMenu(hMenu, MF_SEPARATOR, ID_SEPARATOR, "");

					// AppendMenu(hMenu, MF_STRING | (running ? 0 : MF_GRAYED), ID_STOP, "Stop pilight");
					// AppendMenu(hMenu, MF_STRING | (running ? MF_GRAYED : 0), ID_START, "Start pilight");

          AppendMenu(hMenu, MF_STRING, ID_QUIT, "Exit");

          GetCursorPos(&pt);
          SetForegroundWindow(hWnd);
          TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hWnd, NULL);
          PostMessage(hWnd, WM_NULL, 0, 0);
          DestroyMenu(hMenu);
				break;
      }
		break;
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int show) {
	pilight.running = 0;
	pilight.debuglevel = 0;

	HWND hWnd;
  WNDCLASS cls;
  MSG msg;
#ifdef HASH
	snprintf(server_name, sizeof(server_name), "pilight-daemon %s", HASH);
#else
	snprintf(server_name, sizeof(server_name), "pilight-daemon %s", PILIGHT_VERSION);
#endif
  memset(&cls, 0, sizeof(cls));
  cls.lpfnWndProc = (WNDPROC)WindowProc;
  cls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  cls.lpszClassName = server_name;

  RegisterClass(&cls);
  hWnd = CreateWindow(cls.lpszClassName, server_name, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
  ShowWindow(hWnd, SW_HIDE);

  TrayIcon.cbSize = sizeof(TrayIcon);
  TrayIcon.uID = ID_TRAYICON;
  TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  TrayIcon.hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_ICON), IMAGE_ICON, 16, 16, 0);
  TrayIcon.hWnd = hWnd;
  snprintf(TrayIcon.szTip, sizeof(TrayIcon.szTip), "%s", server_name);

  TrayIcon.uCallbackMessage = WM_USER;
  Shell_NotifyIcon(NIM_ADD, &TrayIcon);

  while(GetMessage(&msg, hWnd, 0, 0) > 0 && main_loop == 1) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
	return 0;
}
#else
int main(int argc, char **argv) {
	pilight.running = 0;
	pilight.debuglevel = 0;

	int ret = start_pilight(argc, argv);
	if(ret == EXIT_SUCCESS) {
		eventpool_process(NULL);
	}
	return ret;
}
#endif
