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
#include "settings.h"
#include "http_lib.h"
#include "log.h"

typedef struct settings_t {
	char *name;
	int type;
	union {
		char *string_;
		int number_;
	};
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

/* Add a string value to the settings struct */
static void settings_add_string(const char *name, char *value) {
	struct settings_t *snode = malloc(sizeof(struct settings_t));
	struct settings_t *tmp = NULL;
	if(!snode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if(!(snode->name = malloc(strlen(name)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(snode->name, name);
	if(!(snode->string_ = malloc(strlen(value)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(snode->string_, value);
	snode->type = JSON_STRING;
	snode->next = NULL;

	tmp = settings;
	if(tmp) {
		while(tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp->next = snode;
	} else {
		snode->next = settings;
		settings = snode;
	}
}

/* Add an int value to the settings struct */
static void settings_add_number(const char *name, int value) {
	struct settings_t *tmp = NULL;
	struct settings_t *snode = malloc(sizeof(struct settings_t));
	if(!snode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if(!(snode->name = malloc(strlen(name)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(snode->name, name);
	snode->number_ = value;
	snode->type = JSON_NUMBER;
	snode->next = NULL;

	tmp = settings;
	if(tmp) {
		while(tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp->next = snode;
	} else {
		snode->next = settings;
		settings = snode;
	}
}

/* Retrieve a numeric value from the settings struct */
int settings_find_number(const char *name, int *out) {
	struct settings_t *tmp_settings = settings;

	while(tmp_settings) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == JSON_NUMBER) {
			*out = tmp_settings->number_;
			return EXIT_SUCCESS;
		}
		tmp_settings = tmp_settings->next;
	}
	sfree((void *)&tmp_settings);
	return EXIT_FAILURE;
}

/* Retrieve a string value from the settings struct */
int settings_find_string(const char *name, char **out) {
	struct settings_t *tmp_settings = settings;

	while(tmp_settings) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == JSON_STRING) {
			*out = tmp_settings->string_;
			return EXIT_SUCCESS;
		}
		tmp_settings = tmp_settings->next;
	}
	sfree((void *)&tmp_settings);
	return EXIT_FAILURE;
}

static int settings_parse(JsonNode *root) {
	int have_error = 0;

#ifdef WEBSERVER
	int web_port = WEBSERVER_PORT;
	int own_port = -1;

	char *webgui_tpl = malloc(strlen(WEBGUI_TEMPLATE)+1);
	if(!webgui_tpl) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(webgui_tpl, WEBGUI_TEMPLATE);
	char *webgui_root = malloc(strlen(WEBSERVER_ROOT)+1);
	if(!webgui_root) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(webgui_root, WEBSERVER_ROOT);
#endif

#ifndef __FreeBSD__
	regex_t regex;
	int reti;
#endif

	JsonNode *jsettings = json_first_child(root);

	while(jsettings) {
		if(strcmp(jsettings->key, "port") == 0
		   || strcmp(jsettings->key, "send-repeats") == 0
		   || strcmp(jsettings->key, "receive-repeats") == 0) {
			if((int)jsettings->number_ == 0) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain a number larger than 0", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
#ifdef WEBSERVER
				if(strcmp(jsettings->key, "port") == 0) {
					own_port = (int)jsettings->number_;
				}
#endif
				settings_add_number(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "standalone") == 0) {
			if(jsettings->number_ < 0 || jsettings->number_ > 1) {
				logprintf(LOG_ERR, "config setting \"%s\" must be either 0 or 1", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_number(jsettings->key, (int)jsettings->number_);
			}
		}  else if(strcmp(jsettings->key, "firmware-update") == 0) {
			if(jsettings->number_ < 0 || jsettings->number_ > 1) {
				logprintf(LOG_ERR, "config setting \"%s\" must be either 0 or 1", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_number(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "log-level") == 0) {
			if((int)jsettings->number_ < 0 || (int)jsettings->number_ > 5) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain a number from 0 till 5", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_number(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "pid-file") == 0 || strcmp(jsettings->key, "log-file") == 0) {
			if(!jsettings->string_) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain an existing file path", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				if(path_exists(jsettings->string_) != EXIT_SUCCESS) {
					logprintf(LOG_ERR, "config setting \"%s\" must point to an existing folder", jsettings->key);
					have_error = 1;
					goto clear;
				} else {
					settings_add_string(jsettings->key, jsettings->string_);
				}
			}
		} else if(strcmp(jsettings->key, "whitelist") == 0) {
			if(!jsettings->string_) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain valid ip addresses", jsettings->key);
				have_error = 1;
				goto clear;
			} else if(strlen(jsettings->string_) > 0) {
#ifndef __FreeBSD__
				char validate[] = "^((\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\\.(\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\\.(\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\\.(\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))(,[\\ ]|,|$))+$";
				reti = regcomp(&regex, validate, REG_EXTENDED);
				if(reti) {
					logprintf(LOG_ERR, "could not compile regex");
					have_error = 1;
					goto clear;
				}
				reti = regexec(&regex, jsettings->string_, 0, NULL, 0);
				if(reti == REG_NOMATCH || reti != 0) {
					logprintf(LOG_ERR, "config setting \"%s\" must contain valid ip addresses", jsettings->key);
					have_error = 1;
					regfree(&regex);
					goto clear;
				}
				regfree(&regex);
#endif
				int l = (int)strlen(jsettings->string_)-1;
				if(jsettings->string_[l] == ' ' || jsettings->string_[l] == ',') {
					logprintf(LOG_ERR, "config setting \"%s\" must contain valid ip addresses", jsettings->key);
					have_error = 1;
					goto clear;
				}
				settings_add_string(jsettings->key, jsettings->string_);
			}
#ifdef WEBSERVER
		} else if(strcmp(jsettings->key, "webserver-port") == 0) {
			if(jsettings->number_ < 0) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain a number larger than 0", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				web_port = (int)jsettings->number_;
				settings_add_number(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "webserver-root") == 0) {
			if(!jsettings->string_ || path_exists(jsettings->string_) != 0) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain a valid path", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				webgui_root = realloc(webgui_root, strlen(jsettings->string_)+1);
				if(!webgui_root) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(webgui_root, jsettings->string_);
				settings_add_string(jsettings->key, jsettings->string_);
			}
		} else if(strcmp(jsettings->key, "webserver-enable") == 0) {
			if(jsettings->number_ < 0 || jsettings->number_ > 1) {
				logprintf(LOG_ERR, "config setting \"%s\" must be either 0 or 1", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_number(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "webserver-cache") == 0 ||
		          strcmp(jsettings->key, "webgui-websockets") == 0) {
			if(jsettings->number_ < 0 || jsettings->number_ > 1) {
				logprintf(LOG_ERR, "config setting \"%s\" must be either 0 or 1", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_number(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "webserver-user") == 0) {
			if(jsettings->string_ || strlen(jsettings->string_) > 0) {
				if(name2uid(jsettings->string_) == -1) {
					logprintf(LOG_ERR, "config setting \"%s\" must contain a valid system user", jsettings->key);
					have_error = 1;
					goto clear;
				} else {
					settings_add_string(jsettings->key, jsettings->string_);
				}
			}
		} else if(strcmp(jsettings->key, "webserver-authentication") == 0 && jsettings->tag == JSON_ARRAY) {
				JsonNode *jtmp = json_first_child(jsettings);
				unsigned short i = 0;
				while(jtmp) {
					i++;
					if(jtmp->tag == JSON_STRING) {
						if(i == 1) {
							settings_add_string("webserver-authentication-username", jtmp->string_);
						} else if(i == 2) {
							settings_add_string("webserver-authentication-password", jtmp->string_);
						}
					} else {
						have_error = 1;
						break;
					}
					if(i > 2) {
						have_error = 1;
						break;
					}
					jtmp = jtmp->next;
				}
				if(i != 2 || have_error == 1) {
					logprintf(LOG_ERR, "config setting \"%s\" must be in the format of [ \"username\", \"password\" ]", jsettings->key);
					have_error = 1;
					goto clear;
				}
		}  else if(strcmp(jsettings->key, "webgui-template") == 0) {
			if(!jsettings->string_) {
				logprintf(LOG_ERR, "config setting \"%s\" must be a valid template", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				webgui_tpl = realloc(webgui_tpl, strlen(jsettings->string_)+1);
				if(!webgui_tpl) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(webgui_tpl, jsettings->string_);
				settings_add_string(jsettings->key, jsettings->string_);
			}
#endif
		} else if(strcmp(jsettings->key, "protocol-root") == 0 ||
							strcmp(jsettings->key, "hardware-root") == 0 ||
							strcmp(jsettings->key, "action-root") == 0 ||
							strcmp(jsettings->key, "operator-root") == 0) {
			if(!jsettings->string_ || path_exists(jsettings->string_) != 0) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain a valid path", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_string(jsettings->key, jsettings->string_);
			}
		} else {
			logprintf(LOG_ERR, "config setting \"%s\" is invalid", jsettings->key);
			have_error = 1;
			goto clear;
		}
		jsettings = jsettings->next;
	}

#ifdef WEBSERVER
	if(webgui_tpl) {
		char *tmp = malloc(strlen(webgui_root)+strlen(webgui_tpl)+13);
		if(!tmp) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		sprintf(tmp, "%s/%s/index.html", webgui_root, webgui_tpl);
		if(path_exists(tmp) != EXIT_SUCCESS) {
			logprintf(LOG_ERR, "config setting \"webgui-template\", template does not exists");
			have_error = 1;
			sfree((void *)&tmp);
			goto clear;
		}
		sfree((void *)&tmp);
	}

	if(web_port == own_port) {
		logprintf(LOG_ERR, "config setting \"port\" and \"webserver-port\" cannot be the same");
		have_error = 1;
		goto clear;
	}
#endif
clear:
#ifdef WEBSERVER
	if(webgui_tpl) {
		sfree((void *)&webgui_tpl);
	}
	if(webgui_root) {
		sfree((void *)&webgui_root);
	}
#endif
	return have_error;
}

static JsonNode *settings_sync(int level) {
	struct JsonNode *root = json_mkobject();
	struct settings_t *tmp = settings;
	while(tmp) {
		if(tmp->type == JSON_NUMBER) {
			json_append_member(root, tmp->name, json_mknumber((double)tmp->number_, 0));
		} else if(tmp->type == JSON_STRING) {
			json_append_member(root, tmp->name, json_mkstring(tmp->string_));
		}
		tmp = tmp->next;
	}
	return root;
}

static int settings_gc(void) {
	struct settings_t *tmp;

	while(settings) {
		tmp = settings;
		sfree((void *)&tmp->name);
		if(tmp->type == JSON_STRING) {
			sfree((void *)&tmp->string_);
		}
		settings = settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&settings);

	logprintf(LOG_DEBUG, "garbage collected config settings library");
	return 1;
}

void settings_init(void) {
	/* Request settings json object in main configuration */
	config_register(&config_settings, "settings");
	config_settings->readorder = 0;
	config_settings->writeorder = 3;
	config_settings->parse=&settings_parse;
	config_settings->sync=&settings_sync;
	config_settings->gc=&settings_gc;
}
