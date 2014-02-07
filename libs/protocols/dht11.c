/*
	Copyright (C) 2013 CurlyMo

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
#define __USE_GNU
#include <pthread.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "json.h"
#include "dht11.h"
#include "../pilight/wiringPi.h"
	
#define MAXTIMINGS 10000

unsigned short dht11_loop = 1;

void dht11ParseCleanUp(void *arg) {
	sfree((void *)&arg);
	
	dht11_loop = 0;
}

void *dht11Parse(void *param) {

	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct timeval tp;
	struct timespec ts;	
	int *id = 0;
	int nrid = 0, y = 0, interval = 10;
	int temp_corr = 0, humi_corr = 0, rc = 0;
	int itmp = 0;
	int firstrun = 1;

#ifndef __FreeBSD__
	pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;        
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
#else
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mutex, &attr);
#endif

    pthread_cond_init(&cond, NULL);

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "gpio", &itmp) == 0) {
				id = realloc(id, (sizeof(int)*(size_t)(nrid+1)));
				id[nrid] = itmp;
				nrid++;
			}
			jchild = jchild->next;
		}
	}

	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
		json_find_number(jsettings, "temp-corr", &temp_corr);
		json_find_number(jsettings, "humi-corr", &humi_corr);
	}
	json_delete(json);	

	pthread_cleanup_push(dht11ParseCleanUp, (void *)id);
	
	while(dht11_loop) {
		rc = gettimeofday(&tp, NULL);
		ts.tv_sec = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;
		if(firstrun) {
			ts.tv_sec += 1;
			firstrun = 0;
		} else {
			ts.tv_sec += interval;
		}

		pthread_mutex_lock(&mutex);
		rc = pthread_cond_timedwait(&cond, &mutex, &ts);
		if(rc == ETIMEDOUT) {	
			for(y=0;y<nrid;y++) {
				int tries = 5;
				unsigned short got_correct_date = 0;

				while(tries && !got_correct_date && dht11_loop) {
					int dht11_dat[5], cnt = 7, idx = 0;
					int i = 0;
					int error = 0;
					unsigned int loopCnt = MAXTIMINGS;
					long curtime = 0;
					struct timeval tv;

					for(i=0;i<5;i++) {
						dht11_dat[i] = 0;
					}

					pinMode(id[y], OUTPUT);
					digitalWrite(id[y], LOW);
					delay(20);
					digitalWrite(id[y], HIGH);
					delayMicroseconds(40);
					pinMode(id[y], INPUT);


					while(digitalRead(id[y]) == LOW) {
						if(loopCnt-- == 0) {
							error = 1;
						}
					}

					loopCnt = MAXTIMINGS;
					while(digitalRead(id[y]) == HIGH) {
						if(loopCnt-- == 0) {
							error = 1;
						}
					}

					for(i=0;i<40;i++) {
						loopCnt = MAXTIMINGS;
						while(digitalRead(id[y]) == LOW) {
							if(loopCnt-- == 0) {
								error = 1;
							}
						}

						gettimeofday(&tv, NULL);
						curtime = (1000000 * tv.tv_sec + tv.tv_usec);
						loopCnt = MAXTIMINGS;

						while(digitalRead(id[y]) == HIGH) {
							if(loopCnt-- == 0) {
								error = 1;
							}
						}

						gettimeofday(&tv, NULL);
						if(((1000000 * tv.tv_sec + tv.tv_usec) - curtime) > 40) {
							dht11_dat[idx] |= (1 << cnt);
						}
						if(cnt == 0) {
							cnt = 7;   
							idx++;      
						}
						else {
							cnt--;
						}
					}
					
					if(error == 0 && dht11_dat[4] == (dht11_dat[0] + dht11_dat[2])) {
						got_correct_date = 1;

						int h = dht11_dat[0];
						int t = dht11_dat[2];
						t += temp_corr;
						h += humi_corr;
						
						dht11->message = json_mkobject();
						JsonNode *code = json_mkobject();
						json_append_member(code, "gpio", json_mknumber(id[y]));
						json_append_member(code, "temperature", json_mknumber(t));
						json_append_member(code, "humidity", json_mknumber(h));

						json_append_member(dht11->message, "code", code);
						json_append_member(dht11->message, "origin", json_mkstring("receiver"));
						json_append_member(dht11->message, "protocol", json_mkstring(dht11->id));
						pilight.broadcast(dht11->id, dht11->message);
						json_delete(dht11->message);
						dht11->message = NULL;
					} else {
						logprintf(LOG_DEBUG, "dht11 data checksum was wrong");
						tries--;
						sleep(1);
					}
				}
			}
		}
		pthread_mutex_unlock(&mutex);
	}
	
	pthread_cleanup_pop(1);

	return (void *)NULL;
}

void dht11InitDev(JsonNode *jdevice) {
	wiringPiSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("dht11", &dht11Parse, (void *)json);
	sfree((void *)&output);
}

void dht11Init(void) {
	protocol_register(&dht11);
	protocol_set_id(dht11, "dht11");
	protocol_device_add(dht11, "dht11", "1-wire Temperature and Humidity Sensor");
	dht11->devtype = WEATHER;
	dht11->hwtype = SENSOR;

	options_add(&dht11->options, 't', "temperature", has_value, config_value, "^[0-9]{1,3}$");
	options_add(&dht11->options, 'h', "humidity", has_value, config_value, "^[0-9]{1,3}$");
	options_add(&dht11->options, 'g', "gpio", has_value, config_id, "^([0-9]{1}|1[0-9]|20)$");

	protocol_setting_add_number(dht11, "decimals", 0);
	protocol_setting_add_number(dht11, "humidity", 1);
	protocol_setting_add_number(dht11, "temperature", 1);
	protocol_setting_add_number(dht11, "battery", 0);
	protocol_setting_add_number(dht11, "interval", 10);

	dht11->initDev=&dht11InitDev;
}
