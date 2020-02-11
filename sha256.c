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
#include <mbedtls/sha256.h>

#include "libs/libuv/uv.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/options.h"

int main_gc(void) {
	log_shell_disable();

	eventpool_gc();
	options_gc();
	log_gc();
	FREE(progname);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	pilight.process = PROCESS_CLIENT;

#ifdef PILIGHT_REWRITE
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	log_init();
#endif
	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;
	unsigned char output[34] = { '\0' };
	unsigned char converted[65] = { '\0' };
	char *password = NULL;
	mbedtls_sha256_context ctx;
	int i = 0, x = 0, help = 0;

	if((progname = MALLOC(15)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(progname, "pilight-sha256");

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "p", "password", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

	if(options_parse(options, argc, argv, 1) == -1) {
		help = 1;
	}

	if(options_exists(options, "H") == 0 || help == 1) {
		printf("Usage: %s [options]\n", progname);
		printf("\t -H --help\t\tdisplay usage summary\n");
		printf("\t -V --version\t\tdisplay version\n");
		printf("\t -p --password=password\tpassword to encrypt\n");
		goto close;
	}
	if(options_exists(options, "V") == 0) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto close;
	}

	if(options_exists(options, "p") == 0) {
		char *tmp = NULL;
		options_get_string(options, "p", &tmp);
		if(tmp != NULL) {
			if((password = STRDUP(tmp)) == NULL) {
				OUT_OF_MEMORY
			}
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

	mbedtls_sha256_init(&ctx);
	mbedtls_sha256_starts(&ctx, 0);
	mbedtls_sha256_update(&ctx, (unsigned char *)password, strlen((char *)password));
	mbedtls_sha256_finish(&ctx, output);
	for(x=0;x<64;x+=2) {
		sprintf((char *)&converted[x], "%02x", output[x/2]);
	}
	mbedtls_sha256_free(&ctx);

	for(i=1;i<SHA256_ITERATIONS;i++) {
		mbedtls_sha256_init(&ctx);
		mbedtls_sha256_starts(&ctx, 0);
		mbedtls_sha256_update(&ctx, (unsigned char *)converted, 64);
		mbedtls_sha256_finish(&ctx, output);
		for(x=0;x<64;x+=2) {
			sprintf((char *)&converted[x], "%02x", output[x/2]);
		}
		mbedtls_sha256_free(&ctx);
	}

	printf("%.64s\n", converted);
	mbedtls_sha256_free(&ctx);

close:
	if(password != NULL) {
		FREE(password);
	}

	main_gc();

	return (EXIT_FAILURE);
}
