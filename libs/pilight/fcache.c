/*
Copyright 2013 CurlyMo <curlymoo1@gmail.com>

This file is part of Splash - Linux GUI Framebuffer Splash.

Splash is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Splash is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along
with Splash. If not, see <http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "fcache.h"
#include "common.h"
#include "log.h"
#include "gc.h"

int fcache_gc(void) {
	struct fcache_t *tmp = fcache;
	while(fcache) {
		tmp = fcache;
		sfree((void *)&tmp->name);
		sfree((void *)&tmp->bytes);
		fcache = fcache->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&fcache);
	
	logprintf(LOG_DEBUG, "garbage collected fcache library");
	return 1;
}

int fcache_add(char *filename) {

	unsigned long filesize = 0, i = 0;
	struct stat sb;
	int rc = 0;

	logprintf(LOG_NOTICE, "caching %s", filename);

	if((rc = stat(filename, &sb)) != 0) {
		logprintf(LOG_ERR, "failed to stat %s", filename);
		return -1;
	} else {
		struct fcache_t *node = malloc(sizeof(struct fcache_t));
		
		filesize = (unsigned long)sb.st_size;
		node->bytes = malloc(filesize + 100);
		memset(node->bytes, '\0', filesize + 100);
		int fd = open(filename, O_RDONLY);

		i = 0;
		while (i < filesize) {
			rc = read(fd, node->bytes+i, filesize-i);
			i += (unsigned long)rc;
		}
		close(fd);

		node->size = (int)filesize;
		node->name = malloc(strlen(filename)+1);
		strcpy(node->name, filename);
		node->next = fcache;
		fcache = node;
		return 0;
	}
	return -1;
}

short fcache_get_size(char *filename, int *out) {
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
	struct fcache_t *ftmp = fcache;
	while(ftmp) {
		if(strcmp(ftmp->name, filename) == 0) {
			return ftmp->bytes;
		}
		ftmp = ftmp->next;
	}
	return NULL;
}