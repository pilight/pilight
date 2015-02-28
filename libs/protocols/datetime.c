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
	#define MSG_NOSIGNAL 0
#else
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

static unsigned short datetime_loop = 1;
static unsigned short datetime_threads = 0;
static char *datetime_format = NULL;

static pthread_mutex_t datetimelock;
static pthread_mutexattr_t datetimeattr;

static time_t getntptime(char *ntpserver) {
	struct sockaddr_in servaddr;
	struct pkt msg;
	char ip[17];
	int sockfd = 0;

#ifndef _WIN32
	struct timeval tv;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
#endif

	memset(&msg, '\0', sizeof(struct pkt));
	memset(&servaddr, '\0', sizeof(struct sockaddr_in));

	char *p = ip;
	if(host2ip(ntpserver, p) == -1) {
		goto close;
	}

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		goto close;
	}
#endif

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		logprintf(LOG_ERR, "error in socket");
		goto close;
	}

#ifdef _WIN32
	int timeout = 1000;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(struct timeval));
#else
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
#endif

	memset(&servaddr, '\0', sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(123);

	inet_pton(AF_INET, ip, &servaddr.sin_addr);
	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
		logprintf(LOG_ERR, "error in connect");
		goto close;
	}

	msg.li_vn_mode=227;

	if(sendto(sockfd, (char *)&msg, 48, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < -1) {
		logprintf(LOG_ERR, "error in sending");
		goto close;
	}
	if(recvfrom(sockfd, (void *)&msg, 48, 0, NULL, NULL) < -1) {
		logprintf(LOG_ERR, "error in receiving");
		goto close;
	}

	if(msg.refid > 0) {
		(msg.rec).Ul_i.Xl_ui = ntohl((msg.rec).Ul_i.Xl_ui);
		(msg.rec).Ul_f.Xl_f = (int)ntohl((unsigned int)(msg.rec).Ul_f.Xl_f);

		unsigned int adj = 2208988800u;
		close(sockfd);
		return (time_t)(msg.rec.Ul_i.Xl_ui - adj);
	} else {
		logprintf(LOG_INFO, "invalid ntp host");
		goto close;
	}

close:
	if(sockfd > 0) {
		close(sockfd);
	}
	return -1;
}

static void *datetimeParse(void *param) {
	char UTC[] = "UTC";
	struct protocol_threads_t *thread = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)thread->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	char *tz = NULL, *ntpserver = NULL;
	double longitude = 0, latitude = 0;
	int interval = 86400;

	time_t t = -1, ntp = -1;
	struct tm *tm;
	int x = 0, diff = 0;

	datetime_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "longitude") == 0) {
					longitude = jchild1->number_;
				}
				if(strcmp(jchild1->key, "latitude") == 0) {
					latitude = jchild1->number_;
				}
				if(strcmp(jchild1->key, "ntpserver") == 0) {
					if((ntpserver = MALLOC(strlen(jchild1->string_)+1)) == NULL) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(ntpserver, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}

	if((tz = coord2tz(longitude, latitude)) == NULL) {
		logprintf(LOG_DEBUG, "could not determine timezone");
		tz = UTC;
	} else {
		logprintf(LOG_DEBUG, "%.6f:%.6f seems to be in timezone: %s", longitude, latitude, tz);
	}

	while(datetime_loop) {
		pthread_mutex_lock(&datetimelock);
		t = time(NULL);
		if((x == interval || ntp == -1) && (ntpserver != NULL && strlen(ntpserver) > 0)) {
			ntp = getntptime(ntpserver);
			if(ntp > -1) {
				diff = (int)(t - ntp);
				logprintf(LOG_INFO, "datetime %.6f:%.6f adjusted by %d seconds", longitude, latitude, diff);
			}
			x = 0;
		}

		t -= diff;
		if((tm = localtztime(tz, t)) != NULL) {
			int year = tm->tm_year+1900;
			int month = tm->tm_mon+1;
			int day = tm->tm_mday;
			int hour = tm->tm_hour;
			int minute = tm->tm_min;
			int second = tm->tm_sec;
			int weekday = tm->tm_wday+1;

			datetime->message = json_mkobject();

			JsonNode *code = json_mkobject();
			json_append_member(code, "longitude", json_mknumber(longitude, 6));
			json_append_member(code, "latitude", json_mknumber(latitude, 6));
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
			if(x == 0) {
				interval = (int)((23-hour)*10000)+((59-minute)*100)+(60-second);
			}
			x++;
		}
		pthread_mutex_unlock(&datetimelock);
		sleep(1);
	}
	pthread_mutex_unlock(&datetimelock);

	if(ntpserver != NULL) {
		FREE(ntpserver);
	}

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
	module->version = "1.10";
	module->reqversion = "5.0";
	module->reqcommit = "187";
}

void init(void) {
	datetimeInit();
}
#endif
