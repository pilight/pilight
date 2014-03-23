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

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "json.h"
#include "gc.h"
#include "program.h"

unsigned short program_loop = 1;
unsigned short program_threads = 0;

void *programParse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;

	int interval = 3, nrloops = 0, i = 0;
	char *prog = NULL, *args = NULL;
	pid_t pid = 0;

	program_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		i = 0;
		while(jchild) {
			i++;
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "program") == 0) {
					if(!(prog = malloc(strlen(jchild1->string_)+1))) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(prog, jchild1->string_);
				}
				if(strcmp(jchild1->key, "arguments") == 0) {
					if(!(args = malloc(strlen(jchild1->string_)+1))) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(args, jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			if(i > 1) {
				logprintf(LOG_ERR, "each program definition can only have a single ID object defined");
			}
			jchild = jchild->next;
		}
	}

	json_find_number(json, "poll-interval", &interval);

	while(program_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			program->message = json_mkobject();

			JsonNode *code = json_mkobject();
			json_append_member(code, "program", json_mkstring(prog));
			json_append_member(code, "arguments", json_mkstring(args));

			if((pid = proc_find(prog, args)) > 0) {
				json_append_member(code, "state", json_mkstring("on"));
				json_append_member(code, "pid", json_mknumber((int)pid));
			} else {
				json_append_member(code, "state", json_mkstring("off"));
				json_append_member(code, "pid", json_mknumber(0));
			}
			
			json_append_member(program->message, "message", code);
			json_append_member(program->message, "origin", json_mkstring("receiver"));
			json_append_member(program->message, "protocol", json_mkstring(program->id));
								
			pilight.broadcast(program->id, program->message);
			json_delete(program->message);
			program->message = NULL;
		}
	}

	sfree((void *)&prog);
	sfree((void *)&args);
	program_threads--;
	return (void *)NULL;
}

struct threadqueue_t *programInitDev(JsonNode *jdevice) {
	program_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(program, json);	
	return threads_register("program", &programParse, (void *)node, 0);
}

int programCreateCode(JsonNode *code) {
	char *start = NULL;
	char *stop = NULL;
	char *prog = NULL;
	char *args = NULL;
	int tmp = 0;
	FILE *fp = NULL;

	if(json_find_string(code, "start-command", &start) == 0
	   && json_find_string(code, "stop-command", &stop) == 0
	   && json_find_string(code, "program", &prog) == 0) {
		program->message = json_mkobject();
		json_append_member(program->message, "program", json_mkstring(prog));
		if(json_find_string(code, "arguments", &args) == 0) {
			json_append_member(program->message, "arguments", json_mkstring(args));
		} else {
			json_append_member(program->message, "arguments", json_mkstring(""));
		}
		
		if(json_find_number(code, "off", &tmp) == 0) {
			json_append_member(program->message, "state", json_mkstring("off"));
			system(stop);
		} else if(json_find_number(code, "on", &tmp) == 0) {
			json_append_member(program->message, "state", json_mkstring("on"));
			system(start);
		}
		json_append_member(program->message, "pid", json_mknumber(0));
	}

	return 1;
}

void programThreadGC(void) {
	program_loop = 0;
	protocol_thread_stop(program);
	while(program_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(program);
}

void programInit(void) {

	protocol_register(&program);
	protocol_set_id(program, "program");
	protocol_device_add(program, "program", "Start / Stop / Status of a program");
	program->devtype = SWITCH;
	program->hwtype = API;

	options_add(&program->options, 'x', "start-command", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'y', "stop-command", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'p', "program", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'i', "pid", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&program->options, 'a', "arguments", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, NULL);
	options_add(&program->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&program->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);

	options_add(&program->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&program->options, 0, "poll-interval", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)3, "[0-9]");

	program->createCode=&programCreateCode;
	program->initDev=&programInitDev;
	program->threadGC=&programThreadGC;
}
