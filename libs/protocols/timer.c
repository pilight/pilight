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
        int *time;
        int *start;
        int *delay;
        int *delay_ready;
        int *restart;
} timer_data_t;


static timer_data_t *timer_data;


static void timerCreateMessage(int id, int state) {
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

static void *timerParse(void *param) {

        int i;
        timer_threads++;



        while(timer_loop) {
                pthread_mutex_lock(&timerlock);
                for (i=0; i<timer_data->nrid; i++) {
                        if( timer_data->time[i] == 0 ) {
                                timerCreateMessage(timer_data->id[i],  0);
                        } else if( timer_data->delay[i] && (timer_data->start[i] - timer_data->delay[i] == timer_data->time[i] )) {
                                timerCreateMessage(timer_data->id[i],  1);
                                timer_data->delay_ready[i]=1;
                        }
                        //printf("timer:%d -> %d\n",timer_data->id[i],timer_data->time[i]);
                        if( timer_data->time[i] > -1 )
                                timer_data->time[i]--;
                }

                pthread_mutex_unlock(&timerlock);
                sleep(1);
        }
        pthread_mutex_unlock(&timerlock);


        timer_threads--;
        return (void *)NULL;
}

static void timerPrintHelp(void) {
        printf("\t -i --id=\t\tcontrol timer with id\n");
        printf("\t -t --on\t\t\tstart the timer\n");
        printf("\t -f --off\t\t\tstop the timer\n");

}

static int timerCreateCode(JsonNode *code) {
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
        for (i=0; i<timer_data->nrid; i++) {
                if(timer_data->id[i]==id) {
                        if( state ) {
                                if(timer_data->restart[i]||timer_data->time[i]==-1) {

                                        if( timer_data->delay[i]==0 ) {
                                                timerCreateMessage(id,  state);
                                                timer_data->time[i]=timer_data->start[i];
                                                timer_data->delay_ready[i]=1;
                                        } else {
                                                if(timer_data->delay_ready[i]) {
                                                        timer_data->time[i]=timer_data->start[i];
                                                        timerCreateMessage(id, state);
                                                } else if(timer_data->time[i]==-1) {
                                                        timer_data->time[i]=timer_data->start[i]+timer_data->delay[i];
                                                }
                                        }
                                        break;
                                }
                        } else {
                                timer_data->time[i]=-1;
                                timerCreateMessage(id,  state);
                                break;
                        }

                }
        }


        return EXIT_SUCCESS;
}

static struct threadqueue_t *timerInitDev(JsonNode *jdevice) {
        struct JsonNode *jid = NULL;
        struct JsonNode *jchild = NULL;
        int id=-1,start=-1,delay=-1,restart=-1;
        double itmp = -1;

        if(json_find_number(jdevice, "seconds", &itmp) == 0) {
                start = (int)round(itmp);
        }
        if(json_find_number(jdevice, "delay", &itmp) == 0) {
                delay = (int)round(itmp);
        }
        if(json_find_number(jdevice, "restart", &itmp) == 0) {
                restart = (int)round(itmp);
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
                timer_data->start = REALLOC(timer_data->start, (sizeof(int)*(size_t)(timer_data->nrid+1)));
                if(!timer_data->time) {
                        logprintf(LOG_ERR, "out of memory");
                        exit(EXIT_FAILURE);
                }
                timer_data->delay = REALLOC(timer_data->delay, (sizeof(int)*(size_t)(timer_data->nrid+1)));
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

                timer_data->id[timer_data->nrid]=id;
                timer_data->time[timer_data->nrid]=0;
                timer_data->start[timer_data->nrid]=start;
                timer_data->delay[timer_data->nrid]=delay;
                timer_data->delay_ready[timer_data->nrid]=0;
                timer_data->restart[timer_data->nrid]=restart;
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

static void timerThreadGC(void) {
        timer_loop = 0;
        protocol_thread_stop(timer);
        while(timer_threads > 0) {
                usleep(10);
        }
        protocol_thread_free(timer);
}

static void timerGC(void) {
        FREE(timer_data);
}

#ifndef MODULE
__attribute__((weak))
#endif
void timerInit(void) {
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
void compatibility(struct module_t *module) {
        module->name = "timer";
        module->version = "1.1";
        module->reqversion = "5.0";
        module->reqcommit = "187";
}

void init(void) {
        timerInit();
}
#endif
