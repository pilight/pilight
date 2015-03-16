/*
	Copyright (C) 2015 CurlyMo & MartinR

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

/*
	TODO
	test for double id
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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <stdint.h>
#include <math.h>

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
#include "timer.h"


static unsigned short timer_loop = 1;
static unsigned short timer_threads = 0;

static pthread_mutex_t timerlock;
static pthread_mutexattr_t timerattr;

typedef struct timer_data_t {
	int nrid;
	int *id;
	long *time;
	long *start;
	long *delay;
	struct timespec *started;
	int *delay_ready;
	int *restart;
} timer_data_t;


static timer_data_t *timer_data;
static unsigned int timer_resolution=50; //1000 = 1 Second


static void timerCreateMessage(int id, int state){
	timer->message = json_mkobject();
	JsonNode *code = json_mkobject();
	json_append_member(code, "id", json_mknumber(id, 0));
	if(state == 1) {
		json_append_member(code, "state", json_mkstring("on"));
	} else {
		json_append_member(code, "state", json_mkstring("off"));
	}
	json_append_member(timer->message, "message", code);
	json_append_member(timer->message, "origin", json_mkstring("receiver"));
	json_append_member(timer->message, "protocol", json_mkstring(timer->id));

	if(pilight.broadcast != NULL) {
		pilight.broadcast(timer->id, timer->message);
	}
	json_delete(timer->message);
	timer->message = NULL;
}

static void *timerParse(void *param){

	int i,fd;
	timer_threads++;
	struct timespec last,curr;
	int secs;
	long nsecs;
	struct itimerspec new_value;
	uint64_t exp1;
	ssize_t s;
//	sleep(1); //wait for all timers read

	if (clock_gettime(CLOCK_MONOTONIC, &last) == -1) {
		logprintf(LOG_ERR, "clock_gettime failed");
		exit(EXIT_FAILURE);
	}

	fd = timerfd_create(CLOCK_REALTIME, 0);
	if (fd == -1) {
		logprintf(LOG_ERR, "timerfd_create failed");
		exit(EXIT_FAILURE);
	}

	new_value.it_value.tv_sec = last.tv_sec + 1;
	new_value.it_value.tv_nsec = last.tv_nsec;
	new_value.it_interval.tv_sec = (long)timer_resolution/1000;
	new_value.it_interval.tv_nsec = (long)timer_resolution*1000000-new_value.it_interval.tv_sec*1000000000;
	if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1) {
		logprintf(LOG_ERR, "timerfd_settime failed");
		exit(EXIT_FAILURE);
	}
	while(timer_loop) {
		pthread_mutex_lock(&timerlock);
		for (i=0; i<timer_data->nrid; i++) {
			if(timer_data->time[i] != -1 ) {
				if( timer_data->time[i] <= timer_resolution ) {
					timer_data->delay_ready[i]=0;
					if (clock_gettime(CLOCK_MONOTONIC, &curr) == -1)
						printf("clock_gettime");
					secs = curr.tv_sec - timer_data->started[i].tv_sec;
					nsecs = curr.tv_nsec - timer_data->started[i].tv_nsec;
					if (nsecs < 0) {
						secs--;
						nsecs += 1000000000;
					}
					logprintf(LOG_DEBUG,"timer%d elapsed: %d.%ld\n", i, secs, nsecs );
					timerCreateMessage(timer_data->id[i],  0);
					timer_data->time[i]=-1;

				} else if( !timer_data->delay_ready[i] && timer_data->start[i] >= timer_data->time[i]-(long)timer_resolution ) {
					//printf("timer%d: -> %f (%f:%f)\n",timer_data->id[i],(float)timer_data->time[i]/1000,(float)timer_data->delay[i]/1000,(float)timer_data->start[i]/1000);
					if (clock_gettime(CLOCK_MONOTONIC, &curr) == -1)
						printf("clock_gettime");
					secs = curr.tv_sec - timer_data->started[i].tv_sec;
					nsecs = curr.tv_nsec - timer_data->started[i].tv_nsec;
					if (nsecs < 0) {
						secs--;
						nsecs += 1000000000;
					}
					logprintf(LOG_DEBUG,"timer%d delayed: %d.%ld\n", i, secs, nsecs );
					if (clock_gettime(CLOCK_MONOTONIC, &timer_data->started[i]) == -1)
						printf("clock_gettime");
					timerCreateMessage(timer_data->id[i],  1);
					timer_data->delay_ready[i]=1;
				} else {
					timer_data->time[i]-=(long)timer_resolution;
					//printf("timer%d: -> %f (%f:%f)\n",timer_data->id[i],(float)timer_data->time[i]/1000,(float)timer_data->delay[i]/1000,(float)timer_data->start[i]/1000);
				}
			}
		}

		pthread_mutex_unlock(&timerlock);

		s = read(fd, &exp1, sizeof(uint64_t));
		if (s != sizeof(uint64_t)) {
			logprintf(LOG_ERR, "clockfd read failed");
			exit(EXIT_FAILURE);
		}


	}
	pthread_mutex_unlock(&timerlock);
	close(fd);

	timer_threads--;
	return (void *)NULL;
}

static void timerPrintHelp(void){
	printf("\t -i --id=\t\tcontrol timer with id\n");
	printf("\t -t --on\t\t\tstart the timer\n");
	printf("\t -f --off\t\t\tstop the timer\n");

}

static int timerCreateCode(JsonNode *code){
	double itmp;

	int id=-1,i,state = -1;
	if(json_find_number(code, "id", &itmp) == 0) {
		id=(int)round(itmp);
	}
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;
	if(id==-1||state==-1) {
		logprintf(LOG_ERR, "timer: insufficient number of arguments");
		return EXIT_FAILURE;
	}
	pthread_mutex_lock(&timerlock);
	for (i=0; i<timer_data->nrid; i++) {
		if(timer_data->id[i]==id) {
			if( state ) {
				if(timer_data->restart[i]||timer_data->time[i]==-1) {

//					 if( timer_data->delay[i]==0 ) {
//						 timerCreateMessage(id,  state);
//						 timer_data->time[i]=timer_data->start[i];
//						 timer_data->delay_ready[i]=1;
// 					   	if (clock_gettime(CLOCK_MONOTONIC, &timer_data->started[i]) == -1)
// 					       		printf("clock_gettime");
//					 } else {
					if(timer_data->delay_ready[i]) {
						timer_data->time[i]=timer_data->start[i];
						timerCreateMessage(id, state);
					} else if(timer_data->time[i]==-1) {
						timer_data->time[i]=timer_data->start[i]+timer_data->delay[i];
						if (clock_gettime(CLOCK_MONOTONIC, &timer_data->started[i]) == -1)
							printf("clock_gettime");
					}
//					 }
					break;
				}
			} else {
				timer_data->time[i]=-1;
				timer_data->delay_ready[i]=0;
				timerCreateMessage(id,  state);
				break;
			}

		}
	}
	pthread_mutex_unlock(&timerlock);


	return EXIT_SUCCESS;
}

static struct threadqueue_t *timerInitDev(JsonNode *jdevice) {
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	int id=-1,restart=-1;
	double start=-1,delay=-1;
	double itmp = -1;

	if(json_find_number(jdevice, "seconds", &itmp) == 0) {
		start = itmp;
	}
	if(json_find_number(jdevice, "delay", &itmp) == 0) {
		delay = itmp;
	}
	if(json_find_number(jdevice, "restart", &itmp) == 0) {
		restart = (int)round(itmp);
	}
	if(json_find_number(jdevice, "resolution", &itmp) == 0) {
		if(timer_resolution > (int)round(itmp))
			timer_resolution = (unsigned int)round(itmp);
	}

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		if(json_find_number(jchild, "id", &itmp) == 0) {
			id = (int)round(itmp);
		}

		timer_data->id = REALLOC(timer_data->id, (sizeof(int)*(size_t)(timer_data->nrid+1)));
		if(!timer_data->id) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		timer_data->time = REALLOC(timer_data->time, (sizeof(int)*(size_t)(timer_data->nrid+1)));
		if(!timer_data->time) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		timer_data->start = REALLOC(timer_data->start, (sizeof(long)*(size_t)(timer_data->nrid+1)));
		if(!timer_data->time) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		timer_data->delay = REALLOC(timer_data->delay, (sizeof(long)*(size_t)(timer_data->nrid+1)));
		if(!timer_data->time) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		timer_data->delay_ready = REALLOC(timer_data->delay_ready, (sizeof(int)*(size_t)(timer_data->nrid+1)));
		if(!timer_data->time) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		timer_data->restart = REALLOC(timer_data->restart, (sizeof(int)*(size_t)(timer_data->nrid+1)));
		if(!timer_data->restart) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		timer_data->started = REALLOC(timer_data->started, (sizeof(struct timespec)*(size_t)(timer_data->nrid+1)));
		if(!timer_data->started) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}

		timer_data->id[timer_data->nrid]=id;
		timer_data->time[timer_data->nrid]=0;
		timer_data->start[timer_data->nrid]=(long)round(start*1000);
		timer_data->delay[timer_data->nrid]=(long)round(delay*1000);
		timer_data->delay_ready[timer_data->nrid]=0;
		timer_data->restart[timer_data->nrid]=restart;
		timer_data->started[timer_data->nrid].tv_nsec=0;
		timer_data->started[timer_data->nrid].tv_sec=0;
		timer_data->nrid++;

		timer_loop = 1;
		char *output = json_stringify(jdevice, NULL);
		JsonNode *json = json_decode(output);
		json_free(output);

		if(timer_data->nrid==1) {
			struct protocol_threads_t *node = protocol_thread_init(timer, json);
			return threads_register("timer", &timerParse, (void *)node, 0);
		}
	}
	return NULL;
}

static void timerThreadGC(void){
	timer_loop = 0;
	protocol_thread_stop(timer);
	while(timer_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(timer);
}

static void timerGC(void){
	FREE(timer_data);
}

#ifndef MODULE
__attribute__((weak))
#endif
void timerInit(void){
	pthread_mutexattr_init(&timerattr);
	pthread_mutexattr_settype(&timerattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&timerlock, &timerattr);


	protocol_register(&timer);
	protocol_set_id(timer, "timer");
	protocol_device_add(timer, "timer", "timer protocol");
	timer->devtype = SWITCH;
	timer->hwtype = API;
	timer_data = MALLOC(sizeof(struct timer_data_t));
	timer_data->nrid=0;
	timer_data->id=0;
	timer_data->time=0;
	timer_data->start=0;
	timer_data->delay=0;
	timer_data->delay_ready=0;
	timer_data->restart=0;

	timer->multipleId = 0;

	options_add(&timer->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^[0-9]{1,2}$");
	options_add(&timer->options, 's', "seconds", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^([0-9]{1,10}$");
	options_add(&timer->options, 'd', "delay", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^([0-9]{1,10}$");
	options_add(&timer->options, 'r', "restart", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)0, "^[10]{1}$");
//	options_add(&timer->options, 0, "resolution", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)0, "^[10]{1}$");

	options_add(&timer->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&timer->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);


	options_add(&timer->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	timer->initDev=&timerInitDev;
	timer->threadGC=&timerThreadGC;
	timer->gc=&timerGC;
	timer->createCode=&timerCreateCode;
	timer->printHelp=&timerPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module){
	module->name = "timer";
	module->version = "1.2";
	module->reqversion = "5.0";
	module->reqcommit = "187";
}

void init(void){
	timerInit();
}
#endif
