/*
	Copyright (C) 2014 mmodzele

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
#include <math.h>
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
#include "owfs_ds2408.h"

static unsigned short owfs_ds2408_loop = 1;
static unsigned short owfs_ds2408_threads = 0;
static char owfs_ds2408_path[21];

static void *owfs_ds2408Parse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct stat st;

	DIR *d = NULL;
	FILE *fp = NULL;
	char *owfs_ds2408_sensor = NULL;
	char *stmp = NULL;
	char **id = NULL;
	char *content = NULL;
	int interval = 10;
	int nrid = 0, y = 0, nrloops = 0;
	double itmp = 0;
	size_t bytes = 0;
	int pio_number=0;
	char cpio_number[10];
	owfs_ds2408_threads++;

	if(json_find_number(json, "pio-number", &itmp) == 0)
		pio_number = (int)round(itmp);
	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "address", &stmp) == 0) {
				id = realloc(id, (sizeof(char *)*(size_t)(nrid+1)));
				if(!id) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				id[nrid] = malloc(strlen(stmp)+1);
				if(!id[nrid]) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(id[nrid], stmp);
	
				
				if(json_find_number(jchild, "pio-number", &itmp) == 0)
					pio_number = (int)round(itmp);
				nrid++;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	
	while(owfs_ds2408_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			for(y=0;y<nrid;y++) {
				owfs_ds2408_sensor = realloc(owfs_ds2408_sensor, strlen(owfs_ds2408_path)+strlen(id[y])+5);
				if(!owfs_ds2408_sensor) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				sprintf(owfs_ds2408_sensor, "%s%s/", owfs_ds2408_path, id[y]);
				if((d = opendir(owfs_ds2408_sensor))) {
					char owfs_ds2408_pio_file[50];
					memset(owfs_ds2408_pio_file, '\0', 50);
					strncpy(owfs_ds2408_pio_file, owfs_ds2408_sensor, strlen(owfs_ds2408_sensor));
					strcat(owfs_ds2408_pio_file, "PIO.");
					
					memset(cpio_number, '\0', 10);
					sprintf(cpio_number, "%d", pio_number);
					
					strcat(owfs_ds2408_pio_file,cpio_number);
					if(!(fp = fopen(owfs_ds2408_pio_file, "rb"))) {
						logprintf(LOG_ERR, "OWFS_DS2408: cannot open PIO file: %s", owfs_ds2408_pio_file);
						break;
					}			
					fstat(fileno(fp), &st);
					bytes = (size_t)st.st_size;

					if(!(content = realloc(content, bytes+1))) {
						logprintf(LOG_ERR, "out of memory");
						fclose(fp);
						break;
					}
					memset(content, '\0', bytes+1);

					if(fread(content, sizeof(char), bytes, fp) == -1) {
						logprintf(LOG_ERR, "OWFS_DS2408: cannot read PIO file: %s", owfs_ds2408_pio_file);
						fclose(fp);
						break;
					}
					fclose(fp);
					if(true) {
						owfs_ds2408->message = json_mkobject();						
						JsonNode *code = json_mkobject();
						
						json_append_member(code, "address", json_mkstring(id[y]));
						json_append_member(code, "pio-number", json_mknumber(pio_number));
						if(strcmp(content,"1")==0) {
							json_append_member(code, "state", json_mkstring("on"));
						} else {
							json_append_member(code, "state", json_mkstring("off"));
						}
						json_append_member(owfs_ds2408->message, "message", code);
						json_append_member(owfs_ds2408->message, "origin", json_mkstring("receiver"));
						json_append_member(owfs_ds2408->message, "protocol", json_mkstring(owfs_ds2408->id));
						
						pilight.broadcast(owfs_ds2408->id, owfs_ds2408->message);
						json_delete(owfs_ds2408->message);
						owfs_ds2408->message = NULL;
					}						
				} else {
					logprintf(LOG_ERR, "OWFS_DS2408: 1-wire device %s does not exists", owfs_ds2408_sensor);
				}
			}
		}
	}

	if(owfs_ds2408_sensor) {
		sfree((void *)&owfs_ds2408_sensor);
	}
	if(content) {
		sfree((void *)&content);
	}
	for(y=0;y<nrid;y++) {
		sfree((void *)&id[y]);
	}
	sfree((void *)&id);
	owfs_ds2408_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *owfs_ds2408InitDev(JsonNode *jdevice) {
	owfs_ds2408_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(owfs_ds2408, json);
	return threads_register("owfs_ds2408", &owfs_ds2408Parse, (void *)node, 0);
}

static void owfs_ds2408ThreadGC(void) {
	owfs_ds2408_loop = 0;
	protocol_thread_stop(owfs_ds2408);
	while(owfs_ds2408_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(owfs_ds2408);
}

static void owfs_ds2408CreateMessage(char* address, int pio_number, int state) {
	FILE *fp = NULL;
	char owfs_ds2408_pio_file[50];
	char content[2]={'\0','\0'};
	memset(owfs_ds2408_pio_file, '\0', 50);

	sprintf(content,"%d",state);
	sprintf(owfs_ds2408_pio_file, "%s/%s/%s%d", owfs_ds2408_path, address, "PIO.", pio_number);
	if(!(fp = fopen(owfs_ds2408_pio_file, "w"))) {
		logprintf(LOG_ERR, "OWFS_DS2408: cannot open PIO file: %s", owfs_ds2408_pio_file);
		return;
	}
	if(fwrite(content, sizeof(char), strlen(content), fp) == -1) {
		logprintf(LOG_ERR, "OWFS_DS2408: cannot write PIO file: %s", owfs_ds2408_pio_file);
		fclose(fp);
		return;
	}
	fclose(fp);
	
	owfs_ds2408->message = json_mkobject();
	json_append_member(owfs_ds2408->message, "address", json_mkstring(address));
	json_append_member(owfs_ds2408->message, "pio-number", json_mknumber(pio_number));
	
	if(state == 1) {
		json_append_member(owfs_ds2408->message, "state", json_mkstring("on"));
	} else {
		json_append_member(owfs_ds2408->message, "state", json_mkstring("off"));
	}
}

static int owfs_ds2408CreateCode(JsonNode *code) {
	char address[16]={'\0'};
	char * stmp;
	int pio_number=0;
	int state = -1;
	double itmp = 0;
	
	if(json_find_string(code, "address", &stmp) == 0)
		strcpy(address, stmp);
	if(json_find_number(code, "pio-number", &itmp) == 0)
		pio_number = (int)round(itmp);
		
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(address == '\0' || state == -1) {
		logprintf(LOG_ERR, "owfs_ds2408: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		owfs_ds2408CreateMessage(address, pio_number, state);
	}

	return EXIT_SUCCESS;

}
#ifndef MODULE
__attribute__((weak))
#endif
void owfs_ds2408Init(void) {
	protocol_register(&owfs_ds2408);
	protocol_set_id(owfs_ds2408, "owfs_ds2408");
	protocol_device_add(owfs_ds2408, "owfs_ds2408", "1-wire 8-channel relay switch");
	owfs_ds2408->devtype = SWITCH;
	owfs_ds2408->hwtype = SENSOR;

	options_add(&owfs_ds2408->options, 'a', "address", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, "^(29.)[a-zA-Z0-9]{12}$");
	options_add(&owfs_ds2408->options, 'p', "pio-number", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&owfs_ds2408->options, 'o', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&owfs_ds2408->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);	
	
	options_add(&owfs_ds2408->options, 0, "poll-interval", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)3, "[0-9]");

	memset(owfs_ds2408_path, '\0', 21);	
	strcpy(owfs_ds2408_path, "/mnt/1wire/");

	owfs_ds2408->initDev=&owfs_ds2408InitDev;
	owfs_ds2408->createCode=&owfs_ds2408CreateCode;
	owfs_ds2408->threadGC=&owfs_ds2408ThreadGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "owfs_ds2408";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	owfs_ds2408Init();
}
#endif