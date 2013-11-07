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
#include "gc.h"
#include "ds18b20.h"

unsigned short ds18b20_loop = 1;
char *ds18b20_path = NULL;
char *ds18b20_w1nr = NULL;
char *ds18b20_w1id = NULL;
char *ds18b20_w1dir = NULL;
char *ds18b20_w1slave = NULL;

void *ds18b20Parse(void *param) {
	struct dirent *dir;
	struct dirent *file;

	DIR *d = NULL;
	DIR *f = NULL;
	FILE *fp;
	char *content;
	size_t bytes;
	struct stat st;
	
	int w1valid = 0;
	int w1temp = 0;
	int interval = 0;

	protocol_setting_get_number(ds18b20, "interval", &interval);	
	
	while(ds18b20_loop) {
		if((d = opendir(ds18b20_path))) {
			while((dir = readdir(d)) != NULL) {
				if(dir->d_type == DT_LNK) {
					if(strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
						strncpy(ds18b20_w1nr, dir->d_name, 2);
						if(atoi(ds18b20_w1nr) == 28) {
							strncpy(ds18b20_w1id, &dir->d_name[3], 12);

							size_t w1dirlen = strlen(dir->d_name)+strlen(ds18b20_path)+2;
							ds18b20_w1dir = realloc(ds18b20_w1dir, w1dirlen);
							memset(ds18b20_w1dir, '\0', w1dirlen);
							strcat(ds18b20_w1dir, ds18b20_path);
							strcat(ds18b20_w1dir, dir->d_name);
							strcat(ds18b20_w1dir, "/");

							if((f = opendir(ds18b20_w1dir))) {
								while((file = readdir(f)) != NULL) {
									if(file->d_type == DT_REG) {
										if(strcmp(file->d_name, "w1_slave") == 0) {
											size_t w1slavelen = w1dirlen+10;
											ds18b20_w1slave = realloc(ds18b20_w1slave, w1slavelen);
											memset(ds18b20_w1slave, '\0', w1slavelen);
											strncpy(ds18b20_w1slave, ds18b20_w1dir, w1dirlen);
											strcat(ds18b20_w1slave, "w1_slave");

											if(!(fp = fopen(ds18b20_w1slave, "rb"))) {
												logprintf(LOG_ERR, "cannot read w1 file: %s", ds18b20_w1slave);
												break;
											}

											fstat(fileno(fp), &st);
											bytes = (size_t)st.st_size;

											if(!(content = calloc(bytes+1, sizeof(char)))) {
												logprintf(LOG_ERR, "out of memory");
												fclose(fp);
												break;
											}

											if(fread(content, sizeof(char), bytes, fp) == -1) {
												logprintf(LOG_ERR, "cannot read config file: %s", ds18b20_w1slave);
												fclose(fp);
												sfree((void *)&content);
												break;
											}
											fclose(fp);
											w1valid = 0;
											char *pch = strtok(content, "\n=: ");
											int x = 0;
											while(pch) {
												if(strlen(pch) > 2) {
													if(x == 1 && strstr(pch, "YES")) {
														w1valid = 1;
													}
													if(x == 2) {	
														w1temp = atoi(pch);
													}
													x++;
												}
												pch = strtok(NULL, "\n=: ");
											}
											sfree((void *)&content);
											if(w1valid) {
												ds18b20->message = json_mkobject();
												
												JsonNode *code = json_mkobject();
												
												json_append_member(code, "id", json_mkstring(ds18b20_w1id));
												json_append_member(code, "temperature", json_mknumber(w1temp));
												
												json_append_member(ds18b20->message, "code", code);
												json_append_member(ds18b20->message, "origin", json_mkstring("receiver"));
												json_append_member(ds18b20->message, "protocol", json_mkstring(ds18b20->id));
												
												pilight.broadcast(ds18b20->id, ds18b20->message);
												json_delete(ds18b20->message);
												ds18b20->message = NULL;
											}
										}
									}
								}
								closedir(f);
							}
						}
					}
				}
			}
			closedir(d);
		}
		sleep((unsigned int)interval);
	}

	return (void *)NULL;
}

int ds18b20GC(void) {
	ds18b20_loop = 0;
	sfree((void *)&ds18b20_w1slave);	
	sfree((void *)&ds18b20_w1dir);
	sfree((void *)&ds18b20_w1nr);
	sfree((void *)&ds18b20_w1id);
	sfree((void *)&ds18b20_path);
	return 1;
}

void ds18b20Init(void) {
	
	gc_attach(ds18b20GC);

	protocol_register(&ds18b20);
	protocol_set_id(ds18b20, "ds18b20");
	protocol_device_add(ds18b20, "ds18b20", "1-wire temperature sensor");
	ds18b20->devtype = WEATHER;
	ds18b20->hwtype = SENSOR;

	options_add(&ds18b20->options, 't', "temperature", has_value, config_value, "^[0-9]{1,5}$");
	options_add(&ds18b20->options, 'i', "id", has_value, config_id, "^[a-z0-9]{12}$");

	protocol_setting_add_number(ds18b20, "decimals", 3);
	protocol_setting_add_number(ds18b20, "humidity", 0);
	protocol_setting_add_number(ds18b20, "temperature", 1);
	protocol_setting_add_number(ds18b20, "battery", 0);
	protocol_setting_add_number(ds18b20, "interval", 5);
	
	ds18b20_w1nr = malloc(3);
	ds18b20_w1id = malloc(13);
	ds18b20_w1dir = malloc(4);
	ds18b20_w1slave = malloc(4);
	ds18b20_path = malloc(21);

	memset(ds18b20_w1nr, '\0', 3);
	memset(ds18b20_w1id, '\0', 13);	
	memset(ds18b20_w1dir, '\0', 4);	
	memset(ds18b20_w1slave, '\0', 4);	
	memset(ds18b20_path, '\0', 21);	
	strcpy(ds18b20_path, "/sys/bus/w1/devices/");

	threads_register(&ds18b20Parse, (void *)NULL);
}
