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
unsigned short dht11_threads = 0;

void *dht11Parse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	int *id = 0;
	int nrid = 0, y = 0, interval = 10;
	int temp_offset = 0, humi_offset = 0;
	int itmp = 0, nrloops = 0;

	dht11_threads++;

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

	json_find_number(json, "poll-interval", &interval);
	json_find_number(json, "device-temperature-offset", &temp_offset);
	json_find_number(json, "device-humidity-offset", &humi_offset);

	while(dht11_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
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
						t += temp_offset;
						h += humi_offset;
						
						dht11->message = json_mkobject();
						JsonNode *code = json_mkobject();
						json_append_member(code, "gpio", json_mknumber(id[y]));
						json_append_member(code, "temperature", json_mknumber(t));
						json_append_member(code, "humidity", json_mknumber(h));

						json_append_member(dht11->message, "message", code);
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
	}

	sfree((void *)&id);
	dht11_threads--;
	return (void *)NULL;
}

void dht11InitDev(JsonNode *jdevice) {
	wiringPiSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);

	struct protocol_threads_t *node = protocol_thread_init(dht11, json);
	threads_register("dht11", &dht11Parse, (void *)node, 0);
	sfree((void *)&output);
}

void dht11GC(void) {
	dht11_loop = 0;
	protocol_thread_stop(dht11);
	while(dht11_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(dht11);
}

void dht11Init(void) {
	protocol_register(&dht11);
	protocol_set_id(dht11, "dht11");
	protocol_device_add(dht11, "dht11", "1-wire Temperature and Humidity Sensor");
	dht11->devtype = WEATHER;
	dht11->hwtype = SENSOR;

	options_add(&dht11->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&dht11->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&dht11->options, 'g', "gpio", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|1[0-9]|20)$");

	options_add(&dht11->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&dht11->options, 0, "device-temperature-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&dht11->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&dht11->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&dht11->options, 0, "poll-interval", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	dht11->initDev=&dht11InitDev;
	dht11->gc=&dht11GC;
}
