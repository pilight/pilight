/*
	Copyright (C) 2013 - 2015 CurlyMo

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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <libgen.h>
#include <dirent.h>
#include <dlfcn.h>

#include "mem.h"

static unsigned short memdbg = 0;

struct mallocs_t {
	void *p;
	unsigned long size;
	int line;
	char file[255];
	struct mallocs_t *next;
} mallocs_t;

static struct mallocs_t *mallocs = NULL;

void memtrack(void) {
	memdbg = 1;
}

void xfree(void) {
	if(memdbg == 1) {
		unsigned long totalsize = 0;
		struct mallocs_t *tmp;
		while(mallocs) {
			tmp = mallocs;
			totalsize += mallocs->size;
			free(mallocs->p);
			fprintf(stderr, "WARNING: unfreed pointer in %s at line #%d\n", tmp->file, tmp->line);
			mallocs = mallocs->next;
			free(tmp);
		}
		fprintf(stderr, "ERROR: leaked %lu bytes from pilight libraries and programs.\n", totalsize);
		free(mallocs);
	}
}

void *_malloc(unsigned long a, const char *file, int line) {
	if(memdbg == 1) {
		struct mallocs_t *node = malloc(sizeof(mallocs_t));
		if((node->p = malloc(a)) == NULL) {
			fprintf(stderr, "out of memory\n");
			free(node);
			xfree();
			exit(EXIT_FAILURE);
		}
		node->size = a;
		node->line = line;
		strcpy(node->file, file);
		node->next = mallocs;
		mallocs = node;
		return node->p;
	} else {
		return malloc(a);
	}
}

void *_realloc(void *a, unsigned long b, const char *file, int line) {
	if(memdbg == 1) {
		if(a == NULL) {
			return _malloc(b, file, line);
		} else {
			struct mallocs_t *tmp = mallocs;
			while(tmp) {
				if(tmp->p == a) {
					tmp->line = line;
					strcpy(tmp->file, file);
					tmp->size = b;
					if((tmp->p = realloc(tmp->p, b)) == NULL) {
						fprintf(stderr, "out of memory\n");
						xfree();
						exit(EXIT_FAILURE);
					}
					break;
				}
				tmp = tmp->next;
			}
			if(tmp != NULL) {
				return tmp->p;
			} else {
				return _malloc(b, file, line);
			}
		}
	} else {
		return realloc(a, b);
	}
}

void *_calloc(unsigned long a, unsigned long b, const char *file, int line) {
	if(memdbg == 1) {
		struct mallocs_t *node = malloc(sizeof(mallocs_t));
		if((node->p = malloc(a*b)) == NULL) {
			fprintf(stderr, "out of memory\n");
			free(node);
			xfree();
			exit(EXIT_FAILURE);
		}
		memset(node->p, '\0', a*b);
		node->size = a*b;
		node->line = line;
		strcpy(node->file, file);
		node->next = mallocs;
		mallocs = node;
		return node->p;
	} else {
		return calloc(a, b);
	}
}

void _free(void *a, const char *file, int line) {
	if(memdbg == 1) {
		if(a == NULL) {
			fprintf(stderr, "WARNING: calling free on already freed pointer in %s at line #%d\n", file, line);
		} else {
			struct mallocs_t *currP, *prevP;

			prevP = NULL;

			for(currP = mallocs; currP != NULL; prevP = currP, currP = currP->next) {
				if(currP->p == a) {
					if(prevP == NULL) {
						mallocs = currP->next;
					} else {
						prevP->next = currP->next;
					}

					free(currP->p);
					free(currP);

					break;
				}
			}
		}
	} else {
		free(a);
	}
}
