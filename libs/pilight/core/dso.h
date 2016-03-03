/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _DSO_H_
#define _DSO_H_

typedef struct dso_t {
	void *handle;
	char *name;
	ssize_t size;
	struct dso_t *next;
} dso_t;

typedef struct module_t {
	const char *name;
	const char *version;
	const char *reqversion;
	const char *reqcommit;
} module_t;

struct dso_t *dso;

void *dso_load(char *object);
int dso_gc(void);
void *dso_function(void *handle, const char *name);

#endif
