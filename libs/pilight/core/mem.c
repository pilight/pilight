/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "mem.h"

typedef struct mempool_unit_t {
	struct mempool_unit_t *next;
} mempool_unit_t;

static struct mempool_unit_t *mempool_freeblocks = NULL;

unsigned long mempool_unitsize = 0;
unsigned long mempool_blocksize = 0;
void *mempool_block = NULL;

void mempool_init(unsigned long num, unsigned long size) {
	mempool_unitsize = size;
	mempool_blocksize = num * (size + sizeof(struct mempool_unit_t));
	mempool_block = malloc(mempool_blocksize);

	if(mempool_block != NULL) {
		unsigned long i = 0;
		for(i=0;i<num;i++) {
			struct mempool_unit_t *cur = (mempool_block + i * (size + sizeof(struct mempool_unit_t)));
			cur->next = mempool_freeblocks;
			mempool_freeblocks = cur;
		}
	}
}

// void xfree(void) {
	// free(mempool_block);
// }

void *_malloc(unsigned long size, char *f, int l) {
	struct mempool_unit_t *unit = __sync_val_compare_and_swap(&mempool_freeblocks, NULL, NULL);
	if(size > mempool_unitsize || mempool_block == NULL || unit == NULL) {
		void *p = malloc(size);
		return p;
	}

	while(__sync_bool_compare_and_swap(&mempool_freeblocks, unit, unit->next) == 0) { }
	return (void *)((char *)unit + sizeof(struct mempool_unit_t));
}

void *_realloc(void *p, unsigned long size, char *f, int l) {
	if(size > mempool_unitsize || p == NULL) {
		void *a = _malloc(size, f, l);
		if(p != NULL) {
			memcpy(a, p, size);
		}
		return a;
	}
	return p;
}

void *_calloc(unsigned long a, unsigned long b, char *f, int l) {
	void *p = _malloc(a*b, f, l);
	memset(p, 0, a*b);
	return p;
}

void _free(void *p, char *f, int l) {
	if(mempool_block <= p && p <= (mempool_block + mempool_blocksize)) {
		struct mempool_unit_t *unit = (p - sizeof(struct mempool_unit_t));
		struct mempool_unit_t *head = __sync_val_compare_and_swap(&mempool_freeblocks, NULL, NULL);
		do {
			unit->next = head;
		} while(__sync_bool_compare_and_swap(&mempool_freeblocks, head, unit) == 0);
   } else {
		free(p);
	}
}

