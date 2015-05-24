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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "sha256cache.h"
#include "common.h"
#include "mem.h"
#include "log.h"
#include "gc.h"
#include "../../polarssl/polarssl/sha256.h"

int sha256cache_gc(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct sha256cache_t *tmp = sha256cache;
	while(sha256cache) {
		tmp = sha256cache;
		FREE(tmp->name);
		sha256cache = sha256cache->next;
		FREE(tmp);
	}
	if(sha256cache != NULL) {
		FREE(sha256cache);
	}

	logprintf(LOG_DEBUG, "garbage collected sha256cache library");
	return 1;
}

void sha256cache_remove_node(struct sha256cache_t **cache, char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct sha256cache_t *currP, *prevP;

	prevP = NULL;

	for(currP = *cache; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->name, name) == 0) {
			if(prevP == NULL) {
				*cache = currP->next;
			} else {
				prevP->next = currP->next;
			}

			FREE(currP->name);
			FREE(currP);

			break;
		}
	}
}

int sha256cache_rm(char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	sha256cache_remove_node(&sha256cache, name);

	logprintf(LOG_DEBUG, "removed %s from cache", name);
	return 1;
}

int sha256cache_add(char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	logprintf(LOG_INFO, "chached new sha256 hash");
	
	unsigned char output[33];
	char *password = NULL;
	int i = 0, x = 0, len = 65;
	sha256_context ctx;

	if(strlen(name) < 64) {
		len = 65;
	} else {
		len = strlen(name)+1;
	}

	if((password = MALLOC(len)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(password, name);
	
	struct sha256cache_t *node = MALLOC(sizeof(struct sha256cache_t));
	if(node == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	if((node->name = MALLOC(strlen(name)+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(node->name, name);
	
	for(i=0;i<SHA256_ITERATIONS;i++) {
		sha256_init(&ctx);
		sha256_starts(&ctx, 0);
		sha256_update(&ctx, (unsigned char *)password, strlen((char *)password));
		sha256_finish(&ctx, output);
		for(x=0;x<64;x+=2) {
			sprintf(&password[x], "%02x", output[x/2]);
		}
		sha256_free(&ctx);
	}

	for(i=0;i<64;i+=2) {
		sprintf(&node->hash[i], "%02x", output[i/2]);
	}
	
	node->next = sha256cache;
	sha256cache = node;
	FREE(password);
	return 0;
}

char *sha256cache_get_hash(char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct sha256cache_t *ftmp = sha256cache;
	while(ftmp) {
		if(strcmp(ftmp->name, name) == 0) {
			return ftmp->hash;
		}
		ftmp = ftmp->next;
	}
	return NULL;
}
