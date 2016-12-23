/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef struct sha256cache_t {
	char *name;
	char hash[65];
	struct sha256cache_t *next;
} sha256cache_t;

struct sha256cache_t *sha256cache;

int sha256cache_gc(void);
void sha256cache_remove_node(struct sha256cache_t **sha256cache, char *name);
int sha256cache_rm(char *name);
int sha256cache_add(char *name);
char *sha256cache_get_hash(char *name);
