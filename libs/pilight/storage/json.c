/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#ifndef _WIN32
	#include <regex.h>
	#include <sys/ioctl.h>
	#include <dlfcn.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pthread.h>
#endif
#include <sys/stat.h>
#include <time.h>
// #include <libgen.h>
// #include <dirent.h>

#include "../core/log.h"
#include "json.h"

static struct JsonNode *jconfig = NULL;
static char *configfile = NULL;
static unsigned short has_config = 0;

static void *_values_update(int reason, void *param, void *userdata) {
	struct reason_config_update_t *data = param;

	struct JsonNode *jcdevices = json_find_member(jconfig, "devices");
	struct JsonNode *jcdev_childs = json_first_child(jcdevices);
	struct JsonNode *jvalue = NULL;
	int i = 0, x = 0;

	for(i=0;i<data->nrdev;i++) {
		while(jcdev_childs) {
			if(strcmp(jcdev_childs->key, "timestamp") != 0 &&
			   strcmp(jcdev_childs->key, data->devices[i]) == 0) {
				for(x=0;x<data->nrval;x++) {
					if((jvalue = json_find_member(jcdev_childs, data->values[x].name)) != NULL) {
						if(data->values[x].type == JSON_NUMBER) {
							if(jvalue->tag == JSON_STRING) {
								json_free(jvalue->string_);
							}
							jvalue->number_ = data->values[x].number_;
							jvalue->decimals_ = data->values[x].decimals;
							jvalue->tag = JSON_NUMBER;
						}
						if(data->values[x].type == JSON_STRING) {
							if(jvalue->tag == JSON_NUMBER) {
								jvalue->string_ = NULL;
							}
							if((jvalue->string_ = REALLOC(jvalue->string_, strlen(data->values[x].string_)+1)) == NULL) {
								OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
							}
							strcpy(jvalue->string_, data->values[x].string_);
							jvalue->tag = JSON_STRING;
						}
					}
				}
			}
			jcdev_childs = jcdev_childs->next;
		}
	}
	return NULL;
}

static int _gui_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	*jrespond = json_find_member(jconfig, "gui");
	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

static int _devices_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	*jrespond = json_find_member(jconfig, "devices");
	if(*jrespond == NULL) {
		return -1;
	}
	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

static int _rules_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	*jrespond = json_find_member(jconfig, "rules");

	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

static int _hardware_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	*jrespond = json_find_member(jconfig, "hardware");

	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

static int _settings_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	*jrespond = json_find_member(jconfig, "settings");

	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

static int _registry_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	*jrespond = json_find_member(jconfig, "registry");

	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

/*
 * To prevent conflicts with windows _read
 * we add double underscores
 */
static int __read(char *file, int objects) {
	struct JsonNode *jelement = NULL;
	char *content = NULL;

	if((configfile = MALLOC(strlen(file)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(configfile, file);

	if(file_get_contents(file, &content) == 0) {
		if(json_validate(content) == true) {
			jconfig = json_decode(content);
			if(((objects & CONFIG_DEVICES) == CONFIG_DEVICES || (objects & CONFIG_ALL) == CONFIG_ALL)) {
				if((jelement = json_find_member(jconfig, "devices")) != NULL && jelement->tag != JSON_OBJECT) {
					logprintf(LOG_ERR, "config has an invalid \"devices\" object");
					FREE(content);
					json_free(jconfig);
					jconfig = NULL;
					return -1;
				}
			}
			if(((objects & CONFIG_RULES) == CONFIG_RULES || (objects & CONFIG_ALL) == CONFIG_ALL)) {
				if((jelement = json_find_member(jconfig, "rules")) != NULL && jelement->tag != JSON_OBJECT) {
					logprintf(LOG_ERR, "config has an invalid \"rules\" object");
					FREE(content);
					json_free(jconfig);
					jconfig = NULL;
					return -1;
				}
			}
			if(((objects & CONFIG_GUI) == CONFIG_GUI || (objects & CONFIG_ALL) == CONFIG_ALL)) {
				if((jelement = json_find_member(jconfig, "gui")) != NULL && jelement->tag != JSON_OBJECT) {
					logprintf(LOG_ERR, "config has an invalid \"gui\" object");
					FREE(content);
					json_free(jconfig);
					jconfig = NULL;
					return -1;
				}
			}
			if(((objects & CONFIG_SETTINGS) == CONFIG_SETTINGS || (objects & CONFIG_ALL) == CONFIG_ALL)) {
				if((jelement = json_find_member(jconfig, "settings")) != NULL && jelement->tag != JSON_OBJECT) {
					logprintf(LOG_ERR, "config has an invalid \"settings\" object");
					FREE(content);
					json_free(jconfig);
					jconfig = NULL;
					return -1;
				}
			}
			if(((objects & CONFIG_HARDWARE) == CONFIG_HARDWARE || (objects & CONFIG_ALL) == CONFIG_ALL)) {
				if((jelement = json_find_member(jconfig, "hardware")) != NULL && jelement->tag != JSON_OBJECT) {
					logprintf(LOG_ERR, "config has an invalid \"hardware\" object");
					FREE(content);
					json_free(jconfig);
					jconfig = NULL;
					return -1;
				}
			}
			if(((objects & CONFIG_REGISTRY) == CONFIG_REGISTRY || (objects & CONFIG_ALL) == CONFIG_ALL)) {
				if((jelement = json_find_member(jconfig, "registry")) != NULL && jelement->tag != JSON_OBJECT) {
					logprintf(LOG_ERR, "config has an invalid \"registry\" object");
					FREE(content);
					json_free(jconfig);
					jconfig = NULL;
					return -1;
				}
			}
			FREE(content);
			has_config = 1;
			return 0;
		}
		FREE(content);
	}

	return -1;
}

static int _sync(void) {
	FILE *fp = NULL;

	if(has_config == 1) {
		/* Overwrite config file with proper format */
		if((fp = fopen(configfile, "w+")) == NULL) {
			logprintf(LOG_ERR, "cannot write config file: %s", configfile);
			return -1;
		}
		fseek(fp, 0L, SEEK_SET);
		char *content = NULL;
		if((content = json_stringify(jconfig, "\t")) != NULL) {
			fwrite(content, sizeof(char), strlen(content), fp);
			json_free(content);
		}
		fclose(fp);
	}
	return 0;
}

static int _gc(void) {
	if(jconfig != NULL) {
		json_delete(jconfig);
		jconfig = NULL;
	}
	if(configfile != NULL) {
		FREE(configfile);
	}
	return 0;
}

void jsonInit(void) {
	storage_register(&json, "json");

	json->read=&__read;
	json->sync=&_sync;
	json->gc=&_gc;

	eventpool_callback(REASON_CONFIG_UPDATE, _values_update, NULL);

	json->devices_select=&_devices_select;
	json->rules_select=&_rules_select;
	json->gui_select=&_gui_select;
	json->settings_select=&_settings_select;
	json->hardware_select=&_hardware_select;
	json->registry_select=&_registry_select;

}
