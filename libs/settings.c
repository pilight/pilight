/*
	Copyright 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
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

#include "json.h"
#include "settings.h"
#include "log.h"

/* Add a string value to the settings struct */
void settings_add_string_node(char *name, char *value) {
	struct settings_t *snode = malloc(sizeof(struct settings_t));
	strcpy(snode->name, name);
	snode->value = malloc(sizeof(union value_t));
	snode->value->cvalue = malloc((strlen(value)*sizeof(char))+1);
	memset(snode->value->cvalue, '\0', (strlen(value)*sizeof(char))+1);
	memcpy(snode->value->cvalue, value, strlen(value));
	snode->type = 2;
	snode->next = settings;
	settings = snode;
}

/* Add an int value to the settings struct */
void settings_add_number_node(char *name, int value) {
	struct settings_t *snode = malloc(sizeof(struct settings_t));
	strcpy(snode->name, name);
	snode->value = malloc(sizeof(union value_t));
	snode->value->ivalue = value;
	snode->type = 1;
	snode->next = settings;
	settings = snode;
}

/* Retrieve a numeric value from the settings struct */
int settings_find_number(const char *name, int *out) {
	struct settings_t *tmp_settings = settings;

	while(tmp_settings != NULL) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == 1) {
			*out = tmp_settings->value->ivalue;
			return 0;
		}
		tmp_settings = tmp_settings->next;
	}
	return 1;
}

/* Retrieve a string value from the settings struct */
int settings_find_string(const char *name, char **out) {
	struct settings_t *tmp_settings = settings;

	while(tmp_settings != NULL) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == 2) {
			*out = tmp_settings->value->cvalue;
			return 0;
		}
		tmp_settings = tmp_settings->next;
	}
	return 1;
}

/* Check if a given path exists */
int settings_path_exists(char *fil) {
	struct stat s;
	char *filename = basename(fil);
	char path[1024];
	size_t i = (strlen(fil)-strlen(filename));

	memset(path, '\0', sizeof(path));
	memcpy(path, fil, i);

	if(strcmp(basename(fil), fil) != 0) {
		int err = stat(path, &s);
		if(err == -1) {
			if(ENOENT == errno) {
				return 1;
			} else {
				return 1;
			}
		} else {
			if(S_ISDIR(s.st_mode)) {
				return 0;
			} else {
				return 1;
			}
		}
	}
	return 0;
}

/* Check if a given file exists */
int settings_file_exists(char *filename) {
	struct stat sb;   
	return stat(filename, &sb);
}

int settings_parse(JsonNode *root) {
	int have_error = 0;

	JsonNode *jsettings = json_first_child(root);

	while(jsettings != NULL) {
		if(strcmp(jsettings->key, "port") == 0 || strcmp(jsettings->key, "send-repeats") == 0 || strcmp(jsettings->key, "receive-repeats") == 0) {
			if((int)jsettings->number_ == 0) {
				logprintf(LOG_ERR, "setting \"%s\" must contain a number larger than 0", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_number_node(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "mode") == 0) {
			if(jsettings->string_ == NULL) {
				logprintf(LOG_ERR, "setting \"%s\" must be \"daemon\" or \"node\"", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				if(strcmp(jsettings->string_, "daemon") == 0 || strcmp(jsettings->string_, "node") == 0) {
					settings_add_string_node(jsettings->key, jsettings->string_);
				} else {
					logprintf(LOG_ERR, "setting \"%s\" must be \"daemon\" or \"node\"", jsettings->key);
					have_error = 1;
					goto clear;
				}
			}
		} else if(strcmp(jsettings->key, "log-level") == 0) {
			if((int)jsettings->number_ == 0 || (int)jsettings->number_ > 5) {
				logprintf(LOG_ERR, "setting \"%s\" must contain a number from 0 till 5", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				settings_add_number_node(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "pid-file") == 0 || strcmp(jsettings->key, "log-file") == 0) {
			if(jsettings->string_ == NULL) {
				logprintf(LOG_ERR, "setting \"%s\" must contain an existing file path", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				if(settings_path_exists(jsettings->string_) != 0) {
					logprintf(LOG_ERR, "setting \"%s\" must point to an existing folder", jsettings->key);
					have_error = 1;
					goto clear;				
				} else {
					settings_add_string_node(jsettings->key, jsettings->string_);
				}
			}
		} else if(strcmp(jsettings->key, "config-file") == 0 || strcmp(jsettings->key, "process-file") == 0) {
			if(jsettings->string_ == NULL) {
				logprintf(LOG_ERR, "setting \"%s\" must contain an existing file path", jsettings->key);
				have_error = 1;
				goto clear;
			} else if(strlen(jsettings->string_) > 0) {
				if(settings_file_exists(jsettings->string_) == 0) {
					settings_add_string_node(jsettings->key, jsettings->string_);
				} else {
					logprintf(LOG_ERR, "setting \"%s\" must point to an existing file", jsettings->key);
					have_error = 1;
					goto clear;
				}
			}
		} else if(strcmp(jsettings->key, "socket") == 0) {
			if(jsettings->string_ == NULL) {
				logprintf(LOG_ERR, "setting \"%s\" must point an existing socket", jsettings->key);
				have_error = 1;
				goto clear;
			} else if(strlen(jsettings->string_) > 0) {
				settings_add_string_node(jsettings->key, jsettings->string_);
			}
		}
		jsettings = jsettings->next;
	}
clear:
	return have_error;
}

int settings_write(char *content) {
	FILE *fp;

	/* Overwrite config file with proper format */
	if((fp = fopen(settingsfile, "w+")) == NULL) {
		logprintf(LOG_ERR, "cannot write settings file: %s", settingsfile);
		return EXIT_FAILURE;
	}
	fseek(fp, 0L, SEEK_SET);
 	fwrite(content, sizeof(char), strlen(content), fp);
	fclose(fp);

	return EXIT_SUCCESS;
}

int settings_read(void) {
	FILE *fp;
	char *content;
	size_t bytes;
	JsonNode *root;
	struct stat st;

	/* Read JSON config file */
	if((fp = fopen(settingsfile, "rb")) == NULL) {
		logprintf(LOG_ERR, "cannot read settings file: %s", settingsfile);
		return EXIT_FAILURE;
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if((content = calloc(bytes+1, sizeof(char))) == NULL) {
		logprintf(LOG_ERR, "out of memory", settingsfile);
		return EXIT_FAILURE;
	}

	if(fread(content, sizeof(char), bytes, fp) == -1) {
		logprintf(LOG_ERR, "cannot read settings file: %s", settingsfile);
	}
	fclose(fp);

	/* Validate JSON and turn into JSON object */
	if(json_validate(content) == false) {
		logprintf(LOG_ERR, "settings are not in a valid json format", content);
		free(content);
		return EXIT_FAILURE;
	}
	root = json_decode(content);

	free(content);

	if(settings_parse(root) != 0) {
		return EXIT_FAILURE;
	}
	settings_write(json_stringify(root, "\t"));

	return EXIT_SUCCESS;
}

int settings_set_file(char *settfile) {
	if(access(settfile, R_OK | W_OK) != -1) {
		settingsfile = strdup(settfile);
	} else {
		fprintf(stderr, "%s: the settings file %s does not exists\n", progname, settfile);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
