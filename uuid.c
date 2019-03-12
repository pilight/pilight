/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>

#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/gc.h"

int main_gc(void) {
	log_shell_disable();

	options_gc();
	log_gc();
	gc_clear();

	FREE(progname);
	xfree();

	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	// memtrack();
	atomicinit();
	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;
	char *p = NULL;
	int help = 0;

	if((progname = MALLOC(13)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(progname, "pilight-uuid");

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	// if(argc == 1) {
		// printf("Usage: %s [options]\n", progname);
		// goto close;
	// }

	if(options_parse(options, argc, argv, 1) == -1) {
		help = 1;
	}

	if(options_exists(options, "H") == 0 || help == 1) {
		printf("Usage: %s [options]\n", progname);
		printf("\t -H --help\t\tdisplay usage summary\n");
		printf("\t -V --version\t\tdisplay version\n");
		goto close;
	}

	if(options_exists(options, "V") == 0) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto close;
	}

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

close:
	options_delete(options);
	main_gc();
	return (EXIT_FAILURE);
}
