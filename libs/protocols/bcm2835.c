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

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "json.h"
#include "gc.h"
#include "bcm2835.h"

unsigned short bcm2835_loop = 1;
unsigned short bcm2835_nrfree = 0;
char *bcm2835_temp = NULL;

void *bcm2835Parse(void *param) {
	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jsettings = NULL;
	struct stat st;

	FILE *fp;
	int itmp;
	int *id = malloc(sizeof(int));
	char *content;	
	int interval = 5;		
	int x = 0, nrid = 0, y = 0;
	size_t bytes;
	
	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				id = realloc(id, (sizeof(int)*(size_t)(nrid+1)));
				id[nrid] = itmp;
				nrid++;
			}
			jchild = jchild->next;
		}
	}
	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
	}
	json_delete(json);

	bcm2835_nrfree++;
	
	while(bcm2835_loop) {
		for(y=0;y<nrid;y++) {
			if((fp = fopen(bcm2835_temp, "rb"))) {
				fstat(fileno(fp), &st);
				bytes = (size_t)st.st_size;

				if(!(content = calloc(bytes+1, sizeof(char)))) {
					logprintf(LOG_ERR, "out of memory");
					fclose(fp);
					break;
				}

				if(fread(content, sizeof(char), bytes, fp) == -1) {
					logprintf(LOG_ERR, "cannot read config file: %s", bcm2835_temp);
					fclose(fp);
					sfree((void *)&content);
					break;
				}

				fclose(fp);
				int temp = atoi(content);
				sfree((void *)&content);

				bcm2835->message = json_mkobject();
				JsonNode *code = json_mkobject();
				json_append_member(code, "id", json_mknumber(id[y]));
				json_append_member(code, "temperature", json_mknumber(temp));
									
				json_append_member(bcm2835->message, "code", code);
				json_append_member(bcm2835->message, "origin", json_mkstring("receiver"));
				json_append_member(bcm2835->message, "protocol", json_mkstring(bcm2835->id));
									
				pilight.broadcast(bcm2835->id, bcm2835->message);
				json_delete(bcm2835->message);
				bcm2835->message = NULL;
			} else {
				logprintf(LOG_ERR, "CPU bcm2835 device %s does not exists", bcm2835_temp);
			}
		}
		for(x=0;x<(interval*1000);x++) {
			if(bcm2835_loop) {
				usleep((__useconds_t)(x));
			}
		}
	}
	if(id) sfree((void *)&id);
	bcm2835_nrfree--;

	return (void *)NULL;
}

void bcm2835InitDev(JsonNode *jdevice) {
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("bcm2835", &bcm2835Parse, (void *)json);
	sfree((void *)&output);
}

int bcm2835GC(void) {
	bcm2835_loop = 0;	
	sfree((void *)&bcm2835_temp);
	while(bcm2835_nrfree > 0) {
		usleep(100);
	}
	return 1;
}

void bcm2835Init(void) {
	
	gc_attach(bcm2835GC);

	protocol_register(&bcm2835);
	protocol_set_id(bcm2835, "bcm2835");
	protocol_device_add(bcm2835, "bcm2835", "RPi CPU/GPU temperature sensor");
	bcm2835->devtype = WEATHER;
	bcm2835->hwtype = SENSOR;

	options_add(&bcm2835->options, 't', "temperature", has_value, config_value, "^[0-9]{1,5}$");
	options_add(&bcm2835->options, 'i', "id", has_value, config_id, "[0-9]");

	protocol_setting_add_number(bcm2835, "decimals", 3);
	protocol_setting_add_number(bcm2835, "humidity", 0);
	protocol_setting_add_number(bcm2835, "temperature", 1);
	protocol_setting_add_number(bcm2835, "battery", 0);
	protocol_setting_add_number(bcm2835, "interval", 5);

	bcm2835_temp = malloc(38);
	memset(bcm2835_temp, '\0', 38);
	strcpy(bcm2835_temp, "/sys/class/thermal/thermal_zone0/temp");

	bcm2835->initDev=&bcm2835InitDev;
}