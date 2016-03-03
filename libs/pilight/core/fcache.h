/*
	Copyright 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _FCACHE_H_
#define _FCACHE_H_

typedef struct fcache_t {
	char *name;
	int size;
	unsigned char *bytes;
	struct fcache_t *next;
} fcaches_t;

struct fcache_t *fcache;

int fcache_gc(void);
int fcache_add(char *filename);
int fcache_rm(char *filename);
short fcache_get_size(char *filename, int *out);
unsigned char *fcache_get_bytes(char *filename);

#endif
