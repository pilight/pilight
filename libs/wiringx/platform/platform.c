/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "platform.h"

static struct platform_t *platforms = NULL;

void platform_register(struct platform_t *platform) {
	platform->next = platforms;
	platforms = platform;
}

struct platform_t *platform_get_by_name(char *name, int *nr) {
	struct platform_t *tmp = platforms;
	int i = 0;
	while(tmp) {
		int len = tmp->nralias;
		for(i=0;i<len;i++) {
			if(strcmp(tmp->name[i], name) == 0) {
				return tmp;
			}
		}
		tmp = tmp->next;
	}
	return NULL;
}

struct platform_t *platform_iterate(int i) {
	struct platform_t *tmp = platforms;
	int x = 0;
	while(tmp) {
		if(x == i) {
			return tmp;
		}
		x++;
		tmp = tmp->next;
	}
	return NULL;
}

int platform_gc(void) {
	struct platform_t *tmp = NULL;
	int i = 0;
	while(platforms) {
		tmp = platforms;
		for(i=0;i<tmp->nralias;i++) {
			free(tmp->name[i]);
		}
		free(tmp->name);
		platforms = platforms->next;
		free(tmp);
	}
	return 0;
}