/*
	Copyright (C) 2014 CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include "pthread.h"
	#include "implement.h"
	#define MSG_NOSIGNAL 0
#else
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pthread.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif
#include <stdint.h>
#include <time.h>

#include "pilight.h"
#include "socket.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "json.h"
#include "gc.h"
#include "../pilight/datetime.h"
#include "datetime.h"

typedef struct l_fp {
	union {
		unsigned int Xl_ui;
		int Xl_i;
	} Ul_i;
	union {
		unsigned int Xl_uf;
		int Xl_f;
	} Ul_f;
} l_fp;

typedef struct pkt {
	int	li_vn_mode;
	int rootdelay;
	int rootdispersion;
	int refid;
	struct l_fp ref;
	struct l_fp org;
	struct l_fp rec;
	/* Make sure the pkg is 48 bits */
	double tmp;
} pkt;

typedef struct datetime_data_t {
	double longitude;
	double latitude;
	char *ntpserver;
	int counter;
	int diff;
	int interval;
	int running;
	int instance;
	time_t time;
	char id[255];
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;	
	time_t ntptime;
	struct datetime_data_t *next;
} datetime_data_t;

static unsigned short datetime_loop = 1;
static unsigned short datetime_threads = 0;
static char *datetime_format = NULL;
static struct datetime_data_t *datetime_data;

static pthread_mutexattr_t datetimeattr;
static pthread_mutex_t datetimelock;

static void *getntptime(void *param) {
	struct datetime_data_t *data = (struct datetime_data_t *)param;
	struct sockaddr_in servaddr;
	struct pkt msg;
	char ip[INET_ADDRSTRLEN+1];
	int sockfd = 0, firstrun = 1;

	memset(&msg, '\0', sizeof(struct pkt));
	memset(&servaddr, '\0', sizeof(struct sockaddr_in));

	data->running = 1;
	
	char *p = ip;
	if(host2ip(data->ntpserver, p) == -1) {
		return (void *)NULL;
	}

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		return (void *)NULL;
	}
#endif

	while(datetime_loop) {
		pthread_mutex_lock(&data->lock);
		if((data->counter == data->interval || (data->ntptime == -1 && ((data->counter % 10) == 0)) || (firstrun == 1))
			 && (data->ntpserver != NULL && strlen(data->ntpserver) > 0)) {
			firstrun = 0;
			if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
				logprintf(LOG_ERR, "Error in ntp server socket connection @%s", data->ntpserver);
			} else {
				memset(&servaddr, '\0', sizeof(servaddr));
				servaddr.sin_family = AF_INET;
				servaddr.sin_port = htons(123);

				inet_pton(AF_INET, ip, &servaddr.sin_addr);

				switch(socket_timeout_connect(sockfd, (struct sockaddr *)&servaddr, 3)) {
					case -1:
						logprintf(LOG_ERR, "could not connect to ntp server @%s", data->ntpserver);
						continue;
					break;
					case -2:
						logprintf(LOG_ERR, "ntp server connection timeout @%s", data->ntpserver);
						continue;
					break;
					case -3:
						logprintf(LOG_ERR, "Error in ntp server socket connection @%s", data->ntpserver);
						continue;
					break;
					default:
					break;
				}				

				msg.li_vn_mode=227;

				if(sendto(sockfd, (char *)&msg, 48, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < -1) {
					logprintf(LOG_ERR, "error in sending");
				} else {
					if(recvfrom(sockfd, (void *)&msg, 48, 0, NULL, NULL) < -1) {
						logprintf(LOG_ERR, "error in receiving");
					} else {
						if(msg.refid > 0) {
							(msg.rec).Ul_i.Xl_ui = ntohl((msg.rec).Ul_i.Xl_ui);
							(msg.rec).Ul_f.Xl_f = (int)ntohl((unsigned int)(msg.rec).Ul_f.Xl_f);

							unsigned int adj = 2208988800u;
							data->ntptime = (time_t)(msg.rec.Ul_i.Xl_ui - adj);
							data->diff = (int)(data->time - data->ntptime);
							logprintf(LOG_INFO, "datetime #%d %.6f:%.6f adjusted by %d seconds", data->instance, data->longitude, data->latitude, data->diff);
							data->counter = 0;
						} else {
							logprintf(LOG_INFO, "could not sync with ntp server: %s", data->ntpserver);
						}
					}
				}
			}
			if(sockfd > 0) {
				close(sockfd);
			}
		}
		pthread_mutex_unlock(&data->lock);
		sleep(1);
	}
	pthread_mutex_unlock(&data->lock);

	data->running = 0;
	
	return (void *)NULL;
}

static void *datetimeParse(void *param) {
	char UTC[] = "UTC";
	struct protocol_threads_t *thread = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)thread->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct datetime_data_t *dnode = MALLOC(sizeof(struct datetime_data_t));
	char *tz = NULL;
	int has_longitude = 0, has_latitude = 0, has_server = 0;
	int target_offset = 0, dst = 0;

	struct tm tm;

	datetime_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "longitude") == 0) {
					has_longitude = 1;
					dnode->longitude = jchild1->number_;
				}
				if(strcmp(jchild1->key, "latitude") == 0) {
					has_latitude = 1;
					dnode->latitude = jchild1->number_;
				}
				if(strcmp(jchild1->key, "ntpserver") == 0) {
					if((dnode->ntpserver = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					has_server = 1;
					strcpy(dnode->ntpserver, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			if(has_longitude == 1 && has_latitude == 1) {
				dnode->next = datetime_data;
				datetime_data = dnode;
			} else {
				if(has_server == 1) {
					FREE(dnode->ntpserver);
				}
				FREE(dnode);
				dnode = NULL;
			}
			jchild = jchild->next;
		}
	}

	if(dnode == NULL) {
		return 0;
	}

	dnode->instance = datetime_threads;
	dnode->diff = 0;
	dnode->counter = 0;
	dnode->running = 0;
	dnode->time = time(NULL);
	dnode->ntptime = -1;
	dnode->interval = 86400;

	pthread_mutexattr_init(&dnode->attr);
	pthread_mutexattr_settype(&dnode->attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&dnode->lock, &dnode->attr);	

	memset(dnode->id, '\0', sizeof(dnode->id));
	sprintf(dnode->id, "ntp resolve #%d (%s)", datetime_threads, dnode->ntpserver);
	threads_register(dnode->id, &getntptime, (void *)dnode, 0);

	if((tz = coord2tz(dnode->longitude, dnode->latitude)) == NULL) {
		logprintf(LOG_INFO, "datetime #%d, could not determine timezone", datetime_threads);
		tz = UTC;
	} else {
		logprintf(LOG_INFO, "datetime #%d %.6f:%.6f seems to be in timezone: %s", datetime_threads, dnode->longitude, dnode->latitude, tz);
	}

	/* Check how many hours we differ from UTC? */
	target_offset = tzoffset(UTC, tz);
	/* Are we in daylight savings time? */
	dst = isdst(tz);

	while(datetime_loop) {
		pthread_mutex_lock(&datetimelock);
		dnode->time = time(NULL);
		dnode->time -= dnode->diff;

		/* Get UTC time */
#ifdef _WIN32
		if(gmtime(&dnode->time) == 0) {
#else
		if(gmtime_r(&dnode->time, &tm) != NULL) {
#endif
			int year = tm.tm_year+1900;
			int month = tm.tm_mon+1;
			int day = tm.tm_mday;
			/* Add our hour difference to the UTC time */
			tm.tm_hour += target_offset;
			/* Add possible daylist savings time hour */
			tm.tm_hour += dst;
			int hour = tm.tm_hour;
			int minute = tm.tm_min;
			int second = tm.tm_sec;
			int weekday = tm.tm_wday+1;
			if(hour >= 24) {
				/* If hour becomes 24 or more we need to normalize it */
				time_t t = mktime(&tm);
#ifdef _WIN32
				localtime(&t);
#else
				localtime_r(&t, &tm);
#endif

				year = tm.tm_year+1900;
				month = tm.tm_mon+1;
				day = tm.tm_mday;
				hour = tm.tm_hour;
				minute = tm.tm_min;
				second = tm.tm_sec;
				weekday = tm.tm_wday+1;
			}
			if(second == 0 && minute == 0) {
				/* Check for dst each hour */
				dst = isdst(tz);
			}

			datetime->message = json_mkobject();

			JsonNode *code = json_mkobject();
			json_append_member(code, "longitude", json_mknumber(dnode->longitude, 6));
			json_append_member(code, "latitude", json_mknumber(dnode->latitude, 6));
			json_append_member(code, "year", json_mknumber(year, 0));
			json_append_member(code, "month", json_mknumber(month, 0));
			json_append_member(code, "day", json_mknumber(day, 0));
			json_append_member(code, "weekday", json_mknumber(weekday, 0));
			json_append_member(code, "hour", json_mknumber(hour, 0));
			json_append_member(code, "minute", json_mknumber(minute, 0));
			json_append_member(code, "second", json_mknumber(second, 0));

			json_append_member(datetime->message, "message", code);
			json_append_member(datetime->message, "origin", json_mkstring("receiver"));
			json_append_member(datetime->message, "protocol", json_mkstring(datetime->id));

			if(pilight.broadcast != NULL) {
				pilight.broadcast(datetime->id, datetime->message);
			}

			json_delete(datetime->message);
			datetime->message = NULL;
			if(dnode->counter == 0) {
				dnode->interval = (int)((23-hour)*10000)+((59-minute)*100)+(60-second);
			}
			dnode->counter++;
		}
		pthread_mutex_unlock(&datetimelock);
		sleep(1);
	}
	pthread_mutex_unlock(&datetimelock);

	datetime_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *datetimeInitDev(JsonNode *jdevice) {
	datetime_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	json_free(output);

	struct protocol_threads_t *node = protocol_thread_init(datetime, json);
	return threads_register("datetime", &datetimeParse, (void *)node, 0);
}

static void datetimeThreadGC(void) {
	datetime_loop = 0;

	protocol_thread_stop(datetime);
	while(datetime_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(datetime);

	struct datetime_data_t *dtmp = NULL;
	while(datetime_data) {
		dtmp = datetime_data;
		pthread_mutex_unlock(&dtmp->lock);
		while(dtmp->running) {
			usleep(10);
		}
		thread_stop(dtmp->id);
		FREE(datetime_data->ntpserver);
		datetime_data = datetime_data->next;
		FREE(dtmp);
	}
	if(datetime_data != NULL) {
		FREE(datetime_data);
	}
}

static void datetimeGC(void) {
	FREE(datetime_format);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void datetimeInit(void) {
	
	pthread_mutexattr_init(&datetimeattr);
	pthread_mutexattr_settype(&datetimeattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&datetimelock, &datetimeattr);

	datetime_format = MALLOC(20);
	strcpy(datetime_format, "HH:mm:ss YYYY-MM-DD");

	protocol_register(&datetime);
	protocol_set_id(datetime, "datetime");
	protocol_device_add(datetime, "datetime", "Date and Time protocol");
	datetime->devtype = DATETIME;
	datetime->hwtype = API;
	datetime->multipleId = 0;
#ifdef PILIGHT_V6
	datetime->masterOnly = 1;
#endif

	options_add(&datetime->options, 'o', "longitude", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'a', "latitude", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'n', "ntpserver", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&datetime->options, 'y', "year", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&datetime->options, 'm', "month", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&datetime->options, 'd', "day", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'w', "weekday", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'h', "hour", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'i', "minute", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 's', "second", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	options_add(&datetime->options, 0, "show-datetime", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&datetime->options, 0, "format", OPTION_HAS_VALUE, GUI_SETTING, JSON_STRING, (void *)datetime_format, NULL);

	datetime->initDev=&datetimeInitDev;
	datetime->threadGC=&datetimeThreadGC;
	datetime->gc=&datetimeGC;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "datetime";
	module->version = "2.1";
	module->reqversion = "5.0";
	module->reqcommit = "187";
}

void init(void) {
	datetimeInit();
}
#endif
