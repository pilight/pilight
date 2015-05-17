/*
	Copyright (C) 2015 CurlyMo

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

#include "libs/polarssl/polarssl/sha256.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/common.h"
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
  unsigned char output[33];
	char converted[65], *password = NULL, *args = NULL;
	sha256_context ctx;
	int i = 0, x = 0;

	if((progname = MALLOC(15)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-sha256");

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'p', "password", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

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
				printf("\t -p --password=password\tpassword to encrypt\n");
				goto close;
			break;
			case 'V':
				printf("%s v%s\n", progname, PILIGHT_VERSION);
				goto close;
			break;
			case 'p':
				if((password = MALLOC(strlen(args)+1)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				strcpy(password, args);
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				goto close;
			break;
		}
	}
	options_delete(options);

	if(password == NULL) {
		printf("Usage: %s [options]\n", progname);
		printf("\t -H --help\t\tdisplay usage summary\n");
		printf("\t -V --version\t\tdisplay version\n");
		printf("\t -p --password=password\tpassword to encrypt\n");
		goto close;
	}

	if(strlen(password) < 64) {
		if((password = REALLOC(password, 65)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}		
	}

	for(i=0;i<SHA256_ITERATIONS;i++) {
		sha256_init(&ctx);
		sha256_starts(&ctx, 0);
		sha256_update(&ctx, (unsigned char *)password, strlen((char *)password));
		sha256_finish(&ctx, output);
		for(x=0;x<64;x+=2) {
			sprintf(&password[x], "%02x", output[x/2] );
		}
		sha256_free(&ctx);
	}
	for(x=0;x<64;x+=2) {
		sprintf(&converted[x], "%02x", output[x/2] );
	}
	printf("%s\n", converted);
	sha256_free(&ctx);

close:
	if(password != NULL) {
		FREE(password);
	}
	main_gc();
	return (EXIT_FAILURE);
}
