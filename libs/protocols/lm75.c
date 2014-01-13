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
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "json.h"
#include "lm75.h"
#include "../pilight/wiringPi.h"
#include "../pilight/wiringPiI2C.h"

unsigned short lm75_loop = 1;
int lm75_nrfree = 0;

void *lm75Parse(void *param) {

	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	char **id = NULL;
	int i = 0;
	int nrid = 0, y = 0, interval = 5, x = 0;
	char *stmp;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				id = realloc(id, (sizeof(char *)*(size_t)(nrid+1)));
				id[nrid] = malloc(strlen(stmp)+1);
				strcpy(id[nrid], stmp);
				nrid++;
			}
			jchild = jchild->next;
		}
	}
	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
	}
	json_delete(json);
	
	lm75_nrfree++;	

	while(lm75_loop) {
		for(y=0;y<nrid;y++) {
			int fd = 0;
			if((fd = wiringPiI2CSetup(strtol(id[y], NULL, 16))) < 0) {

                int raw = wiringPiI2CReadReg16(fd, 0x00);            
                float temp = ((float)((raw&0x00ff)+((raw>>15)?0:0.5))*10);

				lm75->message = json_mkobject();
				JsonNode *code = json_mkobject();
				json_append_member(code, "id", json_mkstring(id[y]));
				json_append_member(code, "temperature", json_mknumber((int)temp));

				json_append_member(lm75->message, "code", code);
				json_append_member(lm75->message, "origin", json_mkstring("receiver"));
				json_append_member(lm75->message, "protocol", json_mkstring(lm75->id));

				pilight.broadcast(lm75->id, lm75->message);
				json_delete(lm75->message);
				lm75->message = NULL;
			} else {
				logprintf(LOG_DEBUG, "error connecting to lm75");
				logprintf(LOG_DEBUG, "(probably i2c bus error from wiringPiI2CSetup)");
				logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
				for(x=0;x<1000;x++) {
					if(lm75_loop) {
						usleep((__useconds_t)(x));
					}
				}
			}
		}
		for(x=0;x<(interval*1000);x++) {
			if(lm75_loop) {
				usleep((__useconds_t)(x));
			}
		}
	}

	if(id) {
		for(i=0;i<nrid;i++) {
			sfree((void *)&id[i]);
		}
		sfree((void *)&id);
	}
	lm75_nrfree--;

	return (void *)NULL;
}

void lm75InitDev(JsonNode *jdevice) {
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("lm75", &lm75Parse, (void *)json);
	sfree((void *)&output);
}

int lm75GC(void) {
	lm75_loop = 0;
	while(lm75_nrfree > 0) {
		usleep(100);
	}
	return 1;
}

void lm75Init(void) {
	gc_attach(lm75GC);

	protocol_register(&lm75);
	protocol_set_id(lm75, "lm75");
	protocol_device_add(lm75, "lm75", "1-wire Temperature and Humidity Sensor");
	protocol_device_add(lm75, "am2302", "1-wire Temperature and Humidity Sensor");	
	lm75->devtype = WEATHER;
	lm75->hwtype = SENSOR;

    options_add(&lm75->options, 't', "temperature", has_value, config_value, "^[0-9]{1,3}$");
    options_add(&lm75->options, 'i', "id", has_value, config_id, "0x[0-9]{2}");

	protocol_setting_add_number(lm75, "decimals", 1);
	protocol_setting_add_number(lm75, "humidity", 0);
	protocol_setting_add_number(lm75, "temperature", 1);
	protocol_setting_add_number(lm75, "battery", 0);
	protocol_setting_add_number(lm75, "interval", 5);

	lm75->initDev=&lm75InitDev;
	
	wiringPiSetup();
}