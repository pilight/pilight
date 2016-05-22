/*
	Copyright (C) 2015 - 2016 CurlyMo

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

#include "libs/mbedtls/mbedtls/sha256.h"
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

	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	atomicinit();
	gc_attach(main_gc);

	pilight.process = PROCESS_CLIENT;

	/* Catch all exit signals for gc */
	gc_catch();

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;
  unsigned char output[33];
	char converted[65], *password = NULL, *args = NULL;
	mbedtls_sha256_context ctx;
	int i = 0, x = 0;

	if((progname = MALLOC(15)) == NULL) {
		OUT_OF_MEMORY
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
					OUT_OF_MEMORY
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
			OUT_OF_MEMORY
		}
	}

	for(i=0;i<SHA256_ITERATIONS;i++) {
		mbedtls_sha256_init(&ctx);
		mbedtls_sha256_starts(&ctx, 0);
		mbedtls_sha256_update(&ctx, (unsigned char *)password, strlen((char *)password));
		mbedtls_sha256_finish(&ctx, output);
		for(x=0;x<64;x+=2) {
			sprintf(&password[x], "%02x", output[x/2] );
		}
		mbedtls_sha256_free(&ctx);
	}
	for(x=0;x<64;x+=2) {
		sprintf(&converted[x], "%02x", output[x/2] );
	}
	printf("%s\n", converted);
	mbedtls_sha256_free(&ctx);

close:
	if(password != NULL) {
		FREE(password);
	}
	main_gc();
	return (EXIT_FAILURE);
}
