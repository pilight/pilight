/*
	Copyright (C) 2014 - 2016 CurlyMo

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
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#include <unistd.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <ctype.h>

#include "libs/libuv/uv.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"

static uv_signal_t *signal_req = NULL;

int main_gc(void) {
	log_shell_disable();

	eventpool_gc();
	options_gc();
	log_gc();

	FREE(progname);

	return EXIT_SUCCESS;
}

void signal_cb(uv_signal_t *handle, int signum) {
	uv_stop(uv_default_loop());
	main_gc();
}

void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	uv_close(handle, close_cb);
}

int main(int argc, char **argv) {
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	pilight.process = PROCESS_CLIENT;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	log_init();
	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;
	char *p = NULL;
	char *args = NULL;

	if((progname = MALLOC(13)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(progname, "pilight-uuid");

	if((signal_req = malloc(sizeof(uv_signal_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_signal_init(uv_default_loop(), signal_req);
	uv_signal_start(signal_req, signal_cb, SIGINT);	

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	while (1) {
		int c;
		c = options_parse1(&options, argc, argv, 1, &args, NULL);
		if(c == -1)
			break;
		if(c == -2)
			c = 'H';
		switch (c) {
			case 'H':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");
				return (EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s v%s\n", progname, PILIGHT_VERSION);
				return (EXIT_SUCCESS);
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	options_delete(options);

	int nrdevs = 0, x = 0;
	char **devs = NULL;
	if((nrdevs = inetdevs(&devs)) > 0) {
		for(x=0;x<nrdevs;x++) {
			if((p = genuuid(devs[x])) == NULL) {
				logprintf(LOG_ERR, "could not generate the device uuid");
			} else {
				strcpy(pilight_uuid, p);
				FREE(p);
				break;
			}
		}
	}
	array_free(&devs, nrdevs);

	printf("%s\n", pilight_uuid);

	signal_cb(NULL, SIGINT);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);	
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		usleep(10);
	}

	main_gc();
	return (EXIT_FAILURE);
}
