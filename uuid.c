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
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <ctype.h>

#include "pilight.h"
#include "common.h"
#include "log.h"
#include "options.h"
#include "ssdp.h"
#include "gc.h"

struct pilight_t pilight;

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

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;
	struct ifaddrs *ifaddr, *ifa;
	int family = 0;
	char *p = NULL;
	char *args = NULL;

	progname = MALLOC(13);
	if(!progname) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-uuid");

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	while (1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &args);
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
				printf("%s %s\n", progname, PILIGHT_VERSION);
				return (EXIT_SUCCESS);
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	options_delete(options);

#ifdef __FreeBSD__
	if(rep_getifaddrs(&ifaddr) == -1) {
		logprintf(LOG_ERR, "could not get network adapter information");
		goto clear;
	}
#else
	if(getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		goto clear;
	}
#endif

	for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr == NULL) {
			continue;
		}

		family = ifa->ifa_addr->sa_family;

		if((strstr(ifa->ifa_name, "lo") == NULL && strstr(ifa->ifa_name, "vbox") == NULL
		    && strstr(ifa->ifa_name, "dummy") == NULL) && (family == AF_INET || family == AF_INET6)) {
			if((p = genuuid(ifa->ifa_name)) == NULL) {
				logprintf(LOG_ERR, "could not generate the device uuid");
				freeifaddrs(ifaddr);
				goto clear;
			} else {
				printf("%s\n", p);
				FREE(p);
				break;
			}
		}
	}

	freeifaddrs(ifaddr);

clear:
	main_gc();
	return (EXIT_FAILURE);
}
