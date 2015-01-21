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

#include "fcache.h"
#include "common.h"
#include "mem.h"
#include "log.h"
#include "gc.h"

int fcache_gc(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct fcache_t *tmp = fcache;
	while(fcache) {
		tmp = fcache;
		FREE(tmp->name);
		FREE(tmp->bytes);
		fcache = fcache->next;
		FREE(tmp);
	}
	FREE(fcache);

	logprintf(LOG_DEBUG, "garbage collected fcache library");
	return 1;
}

void fcache_remove_node(struct fcache_t **cache, char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct fcache_t *currP, *prevP;

	prevP = NULL;

	for(currP = *cache; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->name, name) == 0) {
			if(prevP == NULL) {
				*cache = currP->next;
			} else {
				prevP->next = currP->next;
			}

			FREE(currP->name);
			FREE(currP->bytes);
			FREE(currP);

			break;
		}
	}
}

int fcache_rm(char *filename) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	fcache_remove_node(&fcache, filename);
	logprintf(LOG_DEBUG, "removed %s from cache", filename);
	return 1;
}

int fcache_add(char *filename) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	unsigned long filesize = 0, i = 0;
	struct stat sb;
	ssize_t rc = 0;
	int fd = 0;

	logprintf(LOG_NOTICE, "caching %s", filename);

	if((rc = stat(filename, &sb)) != 0) {
		logprintf(LOG_NOTICE, "failed to stat %s", filename);
		return -1;
	} else {
		struct fcache_t *node = MALLOC(sizeof(struct fcache_t));
		if(!node) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		filesize = (unsigned long)sb.st_size;
		if(!(node->bytes = MALLOC(filesize + 100))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		memset(node->bytes, '\0', filesize + 100);
		if((fd = open(filename, O_RDONLY, 0)) > -1) {
			i = 0;
			while (i < filesize) {
				rc = read(fd, node->bytes+i, filesize-i);
				i += (unsigned long)rc;
			}
			close(fd);

			node->size = (int)filesize;
			node->name = MALLOC(strlen(filename)+1);
			if(!node->name) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(node->name, filename);
			node->next = fcache;
			fcache = node;
			return 0;
		} else {
			return -1;
		}
	}
	return -1;
}

short fcache_get_size(char *filename, int *out) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct fcache_t *ftmp = fcache;
	while(ftmp) {
		if(strcmp(ftmp->name, filename) == 0) {
			*out = ftmp->size;
			return 0;
		}
		ftmp = ftmp->next;
	}
	return -1;
}

unsigned char *fcache_get_bytes(char *filename) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct fcache_t *ftmp = fcache;
	while(ftmp) {
		if(strcmp(ftmp->name, filename) == 0) {
			return ftmp->bytes;
		}
		ftmp = ftmp->next;
	}
	return NULL;
}
