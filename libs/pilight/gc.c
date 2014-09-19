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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

#include "gc.h"
#include "json.h"
#include "config.h"
#include "common.h"

static unsigned short gc_enable = 1;

void gc_handler(int sig) {
	if(sig == SIGSEGV) {
		fprintf(stderr, "segmentation fault\n");
		exit(EXIT_FAILURE);
	} else if(sig == SIGBUS) {
		fprintf(stderr, "buserror\n");
		exit(EXIT_FAILURE);
	}
	if(((sig == SIGINT || sig == SIGTERM || sig == SIGTSTP) && gc_enable == 1) ||
	  (!(sig == SIGINT || sig == SIGTERM || sig == SIGTSTP) && gc_enable == 0)) {
		if(configfile != NULL && gc_enable == 1) {
			gc_enable = 0;
			JsonNode *joutput = config2json(-1);
			char *output = json_stringify(joutput, "\t");
			config_write(output);
			json_delete(joutput);
			sfree((void *)&output);
			joutput = NULL;
			sfree((void *)&configfile);
			configfile = NULL;
		}
		gc_enable = 0;
		config_gc();
		gc_run();
	}
}

/* Add function to gc */
void gc_attach(int (*fp)(void)) {
	struct collectors_t *gnode = malloc(sizeof(struct collectors_t));
	if(!gnode) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	gnode->listener = fp;
	gnode->next = gc;
	gc = gnode;
}

void gc_clear(void) {
	struct collectors_t *tmp = gc;
	while(gc) {
		tmp = gc;
		gc = gc->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&gc);
}

/* Run the GC manually */
int gc_run(void) {
    unsigned int s;
	struct collectors_t *tmp = gc;

	while(tmp) {
		if(tmp->listener() != 0) {
			s=1;
		}
		tmp = tmp->next;
	}

	while(gc) {
		tmp = gc;
		gc = gc->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&gc);

	if(s)
		return EXIT_FAILURE;
	else
		return EXIT_SUCCESS;
}

/* Initialize the catch all gc */
void gc_catch(void) {
	struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = gc_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    sigaction(SIGBUS,  &act, NULL);
    sigaction(SIGILL,  &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGFPE,  &act, NULL);
}
