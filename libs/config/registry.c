/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include <time.h>
#include <libgen.h>

#include "../../pilight.h"
#include "common.h"
#include "json.h"
#include "registry.h"
#include "http_lib.h"
#include "log.h"

struct JsonNode *registry = NULL;
static char *buff = NULL;

static int registry_get_value_recursive(struct JsonNode *root, const char *key, void **value, void **decimals, int type) {
	char *sub = strstr(key, ".");
	buff = realloc(buff, strlen(key)+1);
	strcpy(buff, key);
	if(sub != NULL) {
		int pos = sub-key;
		buff[pos] = '\0';
	}
	struct JsonNode *member = json_find_member(root, buff);
	if(member != NULL) {
		if(member->tag == type) {
			if(type == JSON_NUMBER) {
				*value = (void *)&member->number_;
				*decimals = (void *)&member->decimals_;
			} else if(type == JSON_STRING) {
				*value = (void *)member->string_;
			}
			return 0;
		} else if(member->tag == JSON_OBJECT) {
			if(sub != NULL) {
				int pos = sub-key;
				strcpy(buff, &key[pos+1]);
			}
			int ret = registry_get_value_recursive(member, buff, value, decimals, type);
			return ret;
		}
	}
	return -1;
}

static int registry_set_value_recursive(struct JsonNode *root, const char *key, void *value, int decimals, int type) {
	char *sub = strstr(key, ".");
	buff = realloc(buff, strlen(key)+1);
	strcpy(buff, key);
	if(sub != NULL) {
		int pos = sub-key;
		buff[pos] = '\0';
	}
	struct JsonNode *member = json_find_member(root, buff);
	if(member != NULL) {
		if(member->tag == type) {
			if(type == JSON_NUMBER) {
				member->number_ = *(double *)value;
				member->decimals_ = decimals;
			} else if(type == JSON_STRING) {
				member->string_ = realloc(member->string_, strlen(value)+1);
				strcpy(member->string_, (char *)value);
			}
			return 0;
		} else if(member->tag == JSON_OBJECT) {
			if(sub != NULL) {
				int pos = sub-key;
				strcpy(buff, &key[pos+1]);
			}
			int ret = registry_set_value_recursive(member, buff, value, decimals, type);
			return ret;
		}
	} else if(sub != NULL) {
		member = json_mkobject();
		json_append_member(root, buff, member);
		int pos = sub-key;
		strcpy(buff, &key[pos+1]);
		int ret = registry_set_value_recursive(member, buff, value, decimals, type);
		return ret;
	} else {
		if(type == JSON_NUMBER) {
			json_append_member(root, buff, json_mknumber(*(double *)value, decimals));
		} else if(type == JSON_STRING) {
			json_append_member(root, buff, json_mkstring(value));
		}
		return 0;
	}
	return -1;
}

static void registry_remove_empty_parent(struct JsonNode *root) {
	struct JsonNode *parent = root->parent;
	if(json_first_child(root) == NULL) {
		json_remove_from_parent(root);
		json_delete(root);
		if(parent != NULL) {
			registry_remove_empty_parent(parent);
		}
	}
}

static int registry_remove_value_recursive(struct JsonNode *root, const char *key) {
	char *sub = strstr(key, ".");
	buff = realloc(buff, strlen(key)+1);
	strcpy(buff, key);
	if(sub != NULL) {
		int pos = sub-key;
		buff[pos] = '\0';
	}
	struct JsonNode *member = json_find_member(root, buff);
	if(member != NULL) {
		if(sub == NULL) {
			json_remove_from_parent(member);
			json_delete(member);
			registry_remove_empty_parent(root);
			return 0;
		}
		if(member->tag == JSON_OBJECT) {
			if(sub != NULL) {
				int pos = sub-key;
				strcpy(buff, &key[pos+1]);
			}
			int ret = registry_remove_value_recursive(member, buff);
			return ret;
		}
	}
	return -1;
}

int registry_get_string(const char *key, char **value) {
	if(registry == NULL) {
		return -1;
	}
	return registry_get_value_recursive(registry, key, (void *)value, NULL, JSON_STRING);
}

int registry_get_number(const char *key, double *value, int *decimals) {
	if(registry == NULL) {
		return -1;
	}
	void *p = NULL;
	void *q = NULL;
	int ret = registry_get_value_recursive(registry, key, &p, &q, JSON_NUMBER);
	if(ret == 0) {
		*value = *(double *)p;
		*decimals = *(int *)q;
	}
	return ret;
}

int registry_set_string(const char *key, char *value) {
	if(registry == NULL) {
		registry = json_mkobject();
	}
	return registry_set_value_recursive(registry, key, (void *)value, 0, JSON_STRING);
}

int registry_set_number(const char *key, double value, int decimals) {
	if(registry == NULL) {
		registry = json_mkobject();
	}
	void *p = (void *)&value;
	return registry_set_value_recursive(registry, key, p, decimals, JSON_NUMBER);
}

int registry_remove_value(const char *key) {
	if(registry == NULL) {
		return -1;
	}
	return registry_remove_value_recursive(registry, key);
}

static int registry_parse(JsonNode *root) {
	char *content = json_stringify(root, NULL);
	registry = json_decode(content);
	sfree((void *)&content);
	return 0;
}

static JsonNode *registry_sync(int level) {
	if(registry != NULL) {
		char *content = json_stringify(registry, NULL);
		struct JsonNode *jret = json_decode(content);
		sfree((void *)&content);
		return jret;
	} else {
		return NULL;
	}
}

static int registry_gc(void) {
	if(registry != NULL) {
		json_delete(registry);
	}
	if(buff) {
		sfree((void *)&buff);
		buff = NULL;
	}
	logprintf(LOG_DEBUG, "garbage collected config registry library");
	return 1;
}

void registry_init(void) {
	/* Request settings json object in main configuration */
	config_register(&config_registry, "registry");
	config_registry->readorder = 5;
	config_registry->writeorder = 5;
	config_registry->parse=&registry_parse;
	config_registry->sync=&registry_sync;
	config_registry->gc=&registry_gc;
}
