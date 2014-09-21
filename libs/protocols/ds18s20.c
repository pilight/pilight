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
#include <math.h>
#include <sys/stat.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "json.h"
#include "gc.h"
#include "ds18s20.h"

static unsigned short ds18s20_loop = 1;
static unsigned short ds18s20_threads = 0;
static char ds18s20_path[21];

static pthread_mutex_t ds18s20lock;
static pthread_mutexattr_t ds18s20attr;

static void *ds18s20Parse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct dirent *file = NULL;
	struct stat st;

	DIR *d = NULL;
	FILE *fp = NULL;
	char *ds18s20_sensor = NULL;
	char *stmp = NULL;
	char **id = NULL;
	char *content = NULL;
	int w1valid = 0, w1temp = 0, interval = 10, x = 0;
	int temp_offset = 0, nrid = 0, y = 0, nrloops = 0;
	double itmp = 0;
	size_t bytes = 0;

	ds18s20_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
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
				nrid++;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	if(json_find_number(json, "device-temperature-offset", &itmp) == 0)
		temp_offset = (int)round(itmp);

	while(ds18s20_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&ds18s20lock);
			for(y=0;y<nrid;y++) {
				ds18s20_sensor = realloc(ds18s20_sensor, strlen(ds18s20_path)+strlen(id[y])+5);
				if(!ds18s20_sensor) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				sprintf(ds18s20_sensor, "%s10-%s/", ds18s20_path, id[y]);
				if((d = opendir(ds18s20_sensor))) {
					while((file = readdir(d)) != NULL) {
						if(file->d_type == DT_REG) {
							if(strcmp(file->d_name, "w1_slave") == 0) {
								size_t w1slavelen = strlen(ds18s20_sensor)+10;
								char ds18s20_w1slave[w1slavelen];
								memset(ds18s20_w1slave, '\0', w1slavelen);
								strncpy(ds18s20_w1slave, ds18s20_sensor, strlen(ds18s20_sensor));
								strcat(ds18s20_w1slave, "w1_slave");

								if(!(fp = fopen(ds18s20_w1slave, "rb"))) {
									logprintf(LOG_ERR, "cannot read w1 file: %s", ds18s20_w1slave);
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
									logprintf(LOG_ERR, "cannot read config file: %s", ds18s20_w1slave);
									fclose(fp);
									break;
								}
								fclose(fp);
								w1valid = 0;
								char *pch = strtok(content, "\n=: ");
								x = 0;
								while(pch) {
									if(strlen(pch) > 2) {
										if(x == 1 && strstr(pch, "YES")) {
											w1valid = 1;
										}
										if(x == 2) {
											w1temp = atoi(pch)+temp_offset;
										}
										x++;
									}
									pch = strtok(NULL, "\n=: ");
								}

								if(w1valid) {
									ds18s20->message = json_mkobject();

									JsonNode *code = json_mkobject();

									json_append_member(code, "id", json_mkstring(id[y]));
									json_append_member(code, "temperature", json_mknumber(w1temp));

									json_append_member(ds18s20->message, "message", code);
									json_append_member(ds18s20->message, "origin", json_mkstring("receiver"));
									json_append_member(ds18s20->message, "protocol", json_mkstring(ds18s20->id));

									pilight.broadcast(ds18s20->id, ds18s20->message);
									json_delete(ds18s20->message);
									ds18s20->message = NULL;
								}
							}
						}
					}
					closedir(d);
				} else {
					logprintf(LOG_ERR, "1-wire device %s does not exists", ds18s20_sensor);
				}
			}
			pthread_mutex_unlock(&ds18s20lock);
		}
	}
	if(ds18s20_sensor) {
		sfree((void *)&ds18s20_sensor);
	}
	if(content) {
		sfree((void *)&content);
	}
	for(y=0;y<nrid;y++) {
		sfree((void *)&id[y]);
	}
	sfree((void *)&id);
	ds18s20_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *ds18s20InitDev(JsonNode *jdevice) {
	ds18s20_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(ds18s20, json);
	return threads_register("ds18s20", &ds18s20Parse, (void *)node, 0);
}

static void ds18s20ThreadGC(void) {
	ds18s20_loop = 0;
	protocol_thread_stop(ds18s20);
	while(ds18s20_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(ds18s20);
}

#ifndef MODULE
__attribute__((weak))
#endif
void ds18s20Init(void) {
	pthread_mutexattr_init(&ds18s20attr);
	pthread_mutexattr_settype(&ds18s20attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&ds18s20lock, &ds18s20attr);

	protocol_register(&ds18s20);
	protocol_set_id(ds18s20, "ds18s20");
	protocol_device_add(ds18s20, "ds18s20", "1-wire Temperature Sensor");
	ds18s20->devtype = WEATHER;
	ds18s20->hwtype = SENSOR;

	options_add(&ds18s20->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&ds18s20->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, "^[a-z0-9]{12}$");

	options_add(&ds18s20->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&ds18s20->options, 0, "device-temperature-offset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&ds18s20->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&ds18s20->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&ds18s20->options, 0, "poll-interval", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	memset(ds18s20_path, '\0', 21);
	strcpy(ds18s20_path, "/sys/bus/w1/devices/");

	ds18s20->initDev=&ds18s20InitDev;
	ds18s20->threadGC=&ds18s20ThreadGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "ds18s20";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	ds18s20Init();
}
#endif
