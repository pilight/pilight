/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
	#include <dlfcn.h>
#endif

#include "common.h"
#include "mem.h"
#include "log.h"
#include "dso.h"

void *dso_load(char *object) {
#ifndef _WIN32
	void *handle = NULL;
	int match = 0;
	struct dso_t *tmp = dso;
	while(tmp) {
		if(strcmp(tmp->name, object) == 0) {
			match = 1;
			break;
		}
		tmp = tmp->next;
	}
	if(match) {
		return NULL;
	}

#if defined(RTLD_GROUP) && defined(RTLD_WORLD) && defined(RTLD_PARENT)
	if(!(handle = dlopen(object, RTLD_LAZY | RTLD_GLOBAL | RTLD_GROUP | RTLD_WORLD | RTLD_PARENT))) {
#elif defined(RTLD_DEEPBIND)
	if(!(handle = dlopen(object, RTLD_LAZY | RTLD_GLOBAL | RTLD_DEEPBIND))) {
#else
	if(!(handle = dlopen(object, RTLD_LAZY))) {
#endif
		atomiclock();
		logprintf(LOG_ERR, dlerror());
		atomicunlock();
		return NULL;
	} else {
		struct dso_t *node = MALLOC(sizeof(struct dso_t));
		if(node == NULL) {
			OUT_OF_MEMORY
		}
		node->handle = handle;
		if((node->name = MALLOC(strlen(object)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->name, object);
		node->next = dso;
		dso = node;
		return handle;
	}
#else
	logprintf(LOG_ERR, "dynamic loading not implemented on Windows");
	return NULL;
#endif
}

int dso_gc(void) {
	struct dso_t *tmp = NULL;
	while(dso) {
		tmp = dso;
#ifndef _WIN32
		dlclose(tmp->handle);
#endif
		FREE(tmp->name);
		dso = dso->next;
		FREE(tmp);
	}
	if(dso != NULL) {
		FREE(dso);
	}

	logprintf(LOG_DEBUG, "garbage collected dso library");
	return 0;
}

void *dso_function(void *handle, const char *name) {
#ifndef _WIN32
	void *init = (void *)dlsym(handle, name);
	char *error = NULL;
	atomiclock();
	if((error = dlerror()) != NULL)  {
		logprintf(LOG_ERR, error);
		atomicunlock();
		return 0;
	} else {
		atomicunlock();
		return init;
	}
#else
	return 0;
#endif
}
