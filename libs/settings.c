/*
	Copyright (C) 2013 CurlyMo

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

#include "json.h"
#include "settings.h"
#include "log.h"

/* Add a string value to the settings struct */
void settings_add_string_node(const char *name, char *value) {
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
void settings_add_number_node(const char *name, int value) {
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
	int is_node = 0;
	char *server_ip = NULL;
	int server_port = 0;
	int own_port = 0;
	int has_config = 0;
	int has_lirc = 0;
	int has_socket = 0;
	int gpio_in = -1;
	int gpio_out = -1;

	JsonNode *jsettings = json_first_child(root);

	while(jsettings != NULL) {
		if(strcmp(jsettings->key, "port") == 0 || strcmp(jsettings->key, "send-repeats") == 0 || strcmp(jsettings->key, "receive-repeats") == 0) {
			if((int)jsettings->number_ == 0) {
				logprintf(LOG_ERR, "setting \"%s\" must contain a number larger than 0", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				if(strcmp(jsettings->key, "port") == 0) {
					own_port = (int)jsettings->number_;
				}
				settings_add_number_node(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "mode") == 0) {
			if(jsettings->string_ == NULL) {
				logprintf(LOG_ERR, "setting \"%s\" must be \"server\" or \"client\"", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				if(strcmp(jsettings->string_, "client") == 0) {
					is_node = 1;
				}
				if(strcmp(jsettings->string_, "server") == 0 || strcmp(jsettings->string_, "client") == 0) {
					settings_add_string_node(jsettings->key, jsettings->string_);
				} else {
					logprintf(LOG_ERR, "setting \"%s\" must be \"server\" or \"client\"", jsettings->key);
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
					has_config = 1;
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
			} else {
				has_socket = 1;
				settings_add_string_node(jsettings->key, jsettings->string_);
			}
		} else if(strcmp(jsettings->key, "use-lirc") == 0) {
			if(jsettings->number_ < 0 || jsettings->number_ > 1) {
				logprintf(LOG_ERR, "setting \"%s\" must be either 0 or 1", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				if((int)jsettings->number_ == 1) {
					has_lirc = 1;
				}
				settings_add_number_node(jsettings->key, (int)jsettings->number_);
			}
		} else if(strcmp(jsettings->key, "gpio-reciever") == 0) {
			if(jsettings->number_ < 0 || jsettings->number_ > 7) {
				logprintf(LOG_ERR, "setting \"%s\" must be between 0 and 7", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				gpio_out = (int)jsettings->number_;
				settings_add_number_node(jsettings->key, (int)jsettings->number_);
			}			
		} else if(strcmp(jsettings->key, "gpio-sender") == 0) {
			if(jsettings->number_ < 0 || jsettings->number_ > 7) {
				logprintf(LOG_ERR, "setting \"%s\" must be between 0 and 7", jsettings->key);
				have_error = 1;
				goto clear;
			} else {
				gpio_in = (int)jsettings->number_;
				settings_add_number_node(jsettings->key, (int)jsettings->number_);
			}			
		} else if(strcmp(jsettings->key, "server") == 0) {
			JsonNode *jserver = json_find_member(root, "server");
			JsonNode *jtmp = json_first_child(jserver);
			int i = 0;
			while(jtmp != NULL) {
				i++;
				if(jtmp->tag == JSON_STRING) {
					server_ip = strdup(jtmp->string_);
				} else if(jtmp->tag == JSON_NUMBER) {
					server_port = (int)jtmp->number_;
				}
				if(i > 2) {
					i++;
					break;
				}
				jtmp = jtmp->next;
			}
			if(i > 2) {
				logprintf(LOG_ERR, "setting \"%s\" must be in the format of [ \"x.x.x.x\", xxxx ]", jsettings->key);
				have_error = 1;
				goto clear;
			} else if(server_ip != NULL && server_port > 0 && is_node == 1) {
				settings_add_string_node("server-ip", server_ip);
				settings_add_number_node("server-port", server_port);
			} else {	
				logprintf(LOG_ERR, "setting \"%s\" must be in the format of [ \"x.x.x.x\", xxxx ]", jsettings->key);
				have_error = 1;
				goto clear;
			}
		}
		jsettings = jsettings->next;
	}

	if(has_lirc == 1 && gpio_in > -1) {
		logprintf(LOG_ERR, "setting \"gpio-receiver\" cand use-lirc cannot be combined");
		have_error = 1;
		goto clear;
	}
	if(has_lirc == 1 && gpio_out > -1) {
		logprintf(LOG_ERR, "setting \"gpio-sender\" cand use-lirc cannot be combined");
		have_error = 1;
		goto clear;
	}	
	if(has_lirc == 0 && has_socket == 1) {
		logprintf(LOG_ERR, "setting \"socket\" must be combined with use-lirc");
		have_error = 1;
		goto clear;
	}
	if(server_port > 0 && server_port == own_port && is_node == 1) {
		logprintf(LOG_ERR, "setting \"port\" and server port cannot be the same");
		have_error = 1;
		goto clear;
	}
	if(is_node == 1 && has_config == 1) {
		logprintf(LOG_ERR, "a daemon running as client cannot have a config file defined");
		have_error = 1;
		goto clear;		
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
