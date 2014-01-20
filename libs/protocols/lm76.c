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
#include "lm76.h"
#include "../pilight/wiringPi.h"
#include "../pilight/wiringPiI2C.h"

unsigned short lm76_loop = 1;
int lm76_nrfree = 0;

void *lm76Parse(void *param) {

	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	char **id = NULL;
	int *fd = 0;
	int i = 0;
	int nrid = 0, y = 0, interval = 5, x = 0;
	int temp_corr = 0;
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
		json_find_number(jsettings, "temp-corr", &temp_corr);
	}
	json_delete(json);
	
	lm76_nrfree++;	

	fd = realloc(fd, (sizeof(int)*(size_t)(nrid+1)));
	for(y=0;y<nrid;y++) {
		fd[y] = wiringPiI2CSetup((int)strtol(id[y], NULL, 16));
	}
	
	while(lm76_loop) {
		for(y=0;y<nrid;y++) {
			if(fd[y] > 0) {
                int raw = wiringPiI2CReadReg16(fd[y], 0x00);            
                float temp = ((float)((raw&0x00ff)+((raw>>15)?0:0.0625))*10);

				lm76->message = json_mkobject();
				JsonNode *code = json_mkobject();
				json_append_member(code, "id", json_mkstring(id[y]));
				json_append_member(code, "temperature", json_mknumber((int)temp+temp_corr));

				json_append_member(lm76->message, "code", code);
				json_append_member(lm76->message, "origin", json_mkstring("receiver"));
				json_append_member(lm76->message, "protocol", json_mkstring(lm76->id));

				pilight.broadcast(lm76->id, lm76->message);
				json_delete(lm76->message);
				lm76->message = NULL;
			} else {
				logprintf(LOG_DEBUG, "error connecting to lm76");
				logprintf(LOG_DEBUG, "(probably i2c bus error from wiringPiI2CSetup)");
				logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
				for(x=0;x<1000;x++) {
					if(lm76_loop) {
						usleep((__useconds_t)(x));
					}
				}
			}
		}
		for(x=0;x<(interval*1000);x++) {
			if(lm76_loop) {
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
	if(fd) {
		for(y=0;y<nrid;y++) {
			if(fd[y] > 0) {
				close(fd[y]);
			}
		}
		sfree((void *)&fd);
	}
	lm76_nrfree--;

	return (void *)NULL;
}

void lm76InitDev(JsonNode *jdevice) {
	wiringPiSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("lm76", &lm76Parse, (void *)json);
	sfree((void *)&output);
}

int lm76GC(void) {
	lm76_loop = 0;
	while(lm76_nrfree > 0) {
		usleep(100);
	}
	return 1;
}

void lm76Init(void) {
	gc_attach(lm76GC);

	protocol_register(&lm76);
	protocol_set_id(lm76, "lm76");
	protocol_device_add(lm76, "lm76", "TI I2C Temperature Sensor");
	lm76->devtype = WEATHER;
	lm76->hwtype = SENSOR;

    options_add(&lm76->options, 't', "temperature", has_value, config_value, "^[0-9]{1,3}$");
    options_add(&lm76->options, 'i', "id", has_value, config_id, "0x[0-9a-f]{2}");

	protocol_setting_add_number(lm76, "decimals", 3);
	protocol_setting_add_number(lm76, "humidity", 0);
	protocol_setting_add_number(lm76, "temperature", 1);
	protocol_setting_add_number(lm76, "battery", 0);
	protocol_setting_add_number(lm76, "interval", 5);

	lm76->initDev=&lm76InitDev;
}
