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
#include <stdint.h>
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
#include "dht22.h"
#include "../pilight/wiringPi.h"
	
#define MAXTIMINGS 100

unsigned short dht22_loop = 1;

static uint8_t sizecvt(const int read_value) {
	/* digitalRead() and friends from wiringpi are defined as returning a value
	   < 256. However, they are returned as int() types. This is a safety function */
	if(read_value > 255 || read_value < 0) {
		logprintf(LOG_NOTICE, "invalid data from wiringPi library");
	}
  
	return (uint8_t)read_value;
}

void dht22ParseCleanUp(void *arg) {
	sfree((void *)&arg);
	
	dht22_loop = 0;
}

void *dht22Parse(void *param) {
	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct timeval tp;
	struct timespec ts;	
	int *id = 0;
	int nrid = 0, y = 0, interval = 10, rc = 0;
	int temp_corr = 0, humi_corr = 0, itmp = 0;
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
	
	pthread_cleanup_push(dht22ParseCleanUp, (void *)id);

	while(dht22_loop) {
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
				while(tries && !got_correct_date && dht22_loop) {

					uint8_t laststate = HIGH;
					uint8_t counter = 0;
					uint8_t j = 0, i = 0;

					int dht22_dat[5] = {0,0,0,0,0};

					// pull pin down for 18 milliseconds
					pinMode(id[y], OUTPUT);			
					digitalWrite(id[y], HIGH);
					usleep(500000);  // 500 ms
					// then pull it up for 40 microseconds
					digitalWrite(id[y], LOW);
					usleep(20000);
					// prepare to read the pin
					pinMode(id[y], INPUT);

					// detect change and read data
					for(i=0; i<MAXTIMINGS; i++) {
						counter = 0;
						delayMicroseconds(10);
						while(sizecvt(digitalRead(id[y])) == laststate && dht22_loop) {
							counter++;
							delayMicroseconds(1);
							if(counter == 255) {
								break;
							}
						}
						laststate = sizecvt(digitalRead(id[y]));

						if(counter == 255) 
							break;

						// ignore first 3 transitions
						if((i >= 4) && (i%2 == 0)) {
						
							// shove each bit into the storage bytes
							dht22_dat[(int)((double)j/8)] <<= 1;
							if(counter > 16)
								dht22_dat[(int)((double)j/8)] |= 1;
							j++;
						}
					}

					// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
					// print it out if data is good
					if((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF))) {
						got_correct_date = 1;

						int h = dht22_dat[0] * 256 + dht22_dat[1];
						int t = (dht22_dat[2] & 0x7F)* 256 + dht22_dat[3];
						t += temp_corr;
						h += humi_corr;

						if((dht22_dat[2] & 0x80) != 0) 
							t *= -1;

						dht22->message = json_mkobject();
						JsonNode *code = json_mkobject();
						json_append_member(code, "gpio", json_mknumber(id[y]));
						json_append_member(code, "temperature", json_mknumber(t));
						json_append_member(code, "humidity", json_mknumber(h));

						json_append_member(dht22->message, "code", code);
						json_append_member(dht22->message, "origin", json_mkstring("receiver"));
						json_append_member(dht22->message, "protocol", json_mkstring(dht22->id));

						pilight.broadcast(dht22->id, dht22->message);
						json_delete(dht22->message);
						dht22->message = NULL;
					} else {
						logprintf(LOG_DEBUG, "dht22 data checksum was wrong");
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

void dht22InitDev(JsonNode *jdevice) {
	wiringPiSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("dht22", &dht22Parse, (void *)json);
	sfree((void *)&output);
}

void dht22Init(void) {
	protocol_register(&dht22);
	protocol_set_id(dht22, "dht22");
	protocol_device_add(dht22, "dht22", "1-wire Temperature and Humidity Sensor");
	protocol_device_add(dht22, "am2302", "1-wire Temperature and Humidity Sensor");	
	dht22->devtype = WEATHER;
	dht22->hwtype = SENSOR;

	options_add(&dht22->options, 't', "temperature", has_value, config_value, "^[0-9]{1,3}$");
	options_add(&dht22->options, 'h', "humidity", has_value, config_value, "^[0-9]{1,3}$");
	options_add(&dht22->options, 'g', "gpio", has_value, config_id, "^([0-9]{1}|1[0-9]|20)$");

	protocol_setting_add_number(dht22, "decimals", 1);
	protocol_setting_add_number(dht22, "humidity", 1);
	protocol_setting_add_number(dht22, "temperature", 1);
	protocol_setting_add_number(dht22, "battery", 0);
	protocol_setting_add_number(dht22, "interval", 10);

	dht22->initDev=&dht22InitDev;
}
