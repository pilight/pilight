/*
	Copyright (C) 2014 CurlyMo

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

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "common.h"
#include "mem.h"
#include "log.h"
#include "dso.h"

void *dso_load(char *object) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

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
		logprintf(LOG_ERR, dlerror());
		return NULL;
	} else {
		struct dso_t *node = MALLOC(sizeof(struct dso_t));
		if(!node) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		node->handle = handle;
		if(!(node->name = MALLOC(strlen(object)+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(node->name, object);
		node->next = dso;
		dso = node;
		return handle;
	}
}

int dso_gc(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct dso_t *tmp = NULL;
	while(dso) {
		tmp = dso;
		dlclose(tmp->handle);
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
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	void *init = (void *)dlsym(handle, name);
	char *error = NULL;
	if((error = dlerror()) != NULL)  {
		logprintf(LOG_ERR, error);
		return 0;
	} else {
		return init;
	}
}
