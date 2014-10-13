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
#include <sys/ioctl.h>
#include <time.h>
#include <libgen.h>
#include <dirent.h>
#include <dlfcn.h>

#include "../../pilight.h"
#include "common.h"
#include "irq.h"
#include "wiringX.h"
#include "log.h"
#include "json.h"
#include "settings.h"
#include "dso.h"
#include "hardware.h"

static char *hwfile = NULL;

#include "hardware_header.h"

void hardware_remove(char *name) {
	struct hardware_t *currP, *prevP;

	prevP = NULL;

	for(currP = hardware; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->id, name) == 0) {
			if(prevP == NULL) {
				hardware = currP->next;
			} else {
				prevP->next = currP->next;
			}

			logprintf(LOG_DEBUG, "removed hardware module %s", currP->id);
			sfree((void *)&currP->id);
			options_delete(currP->options);
			sfree((void *)&currP);

			break;
		}
	}
}

void hardware_init(void) {
	#include "hardware_init.h"
	void *handle = NULL;
	void (*init)(void);
	void (*compatibility)(struct module_t *module);
	char path[255];
	struct module_t module;
	char pilight_version[strlen(VERSION)+1];
	char pilight_commit[3];
	char *hardware_root = NULL;
	int check1 = 0, check2 = 0, valid = 1, hardware_root_free = 0;
	strcpy(pilight_version, VERSION);

	struct dirent *file = NULL;
	DIR *d = NULL;

	memset(pilight_commit, '\0', 3);

	if(settings_find_string("hardware-root", &hardware_root) != 0) {
		/* If no webserver port was set, use the default webserver port */
		if(!(hardware_root = malloc(strlen(HARDWARE_ROOT)+2))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(hardware_root, HARDWARE_ROOT);
		hardware_root_free = 1;
	}
	size_t len = strlen(hardware_root);

	if(hardware_root[len-1] != '/') {
		strcat(hardware_root, "/");
	}

	if((d = opendir(hardware_root))) {
		while((file = readdir(d)) != NULL) {
			if(file->d_type == DT_REG) {
				if(strstr(file->d_name, ".so") != NULL) {
					valid = 1;
					memset(path, '\0', 255);
					sprintf(path, "%s%s", hardware_root, file->d_name);

					if((handle = dso_load(path))) {
						init = dso_function(handle, "init");
						compatibility = dso_function(handle, "compatibility");
						if(init && compatibility) {
							compatibility(&module);
							if(module.name && module.version && module.reqversion) {
								char ver[strlen(module.reqversion)+1];
								strcpy(ver, module.reqversion);

								if((check1 = vercmp(ver, pilight_version)) > 0) {
									valid = 0;
								}
								if(check1 == 0 && module.reqcommit) {
									char com[strlen(module.reqcommit)+1];
									strcpy(com, module.reqcommit);
									sscanf(HASH, "v%*[0-9].%*[0-9]-%[0-9]-%*[0-9a-zA-Z\n\r]", pilight_commit);

									if(strlen(pilight_commit) > 0 && (check2 = vercmp(com, pilight_commit)) > 0) {
										valid = 0;
									}
								}

								if(valid) {
									char tmp[strlen(module.name)+1];
									strcpy(tmp, module.name);
									hardware_remove(tmp);
									init();
									logprintf(LOG_DEBUG, "loaded hardware module %s", file->d_name);
								} else {
									if(module.reqcommit) {
										logprintf(LOG_ERR, "hardware module %s requires at least pilight v%s (commit %s)", file->d_name, module.reqversion, module.reqcommit);
									} else {
										logprintf(LOG_ERR, "hardware module %s requires at least pilight v%s", file->d_name, module.reqversion);
									}
								}
							} else {
								logprintf(LOG_ERR, "invalid module %s", file->d_name);
							}
						}
					}
				}
			}
		}
		closedir(d);
	}
	if(hardware_root_free) {
		sfree((void *)&hardware_root);
	}
}

void hardware_register(struct hardware_t **hw) {
	*hw = malloc(sizeof(struct hardware_t));
	if(!*hw) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	(*hw)->options = NULL;

	(*hw)->init = NULL;
	(*hw)->deinit = NULL;
	(*hw)->receive = NULL;
	(*hw)->send = NULL;
	(*hw)->settings = NULL;

	(*hw)->next = hardware;
	hardware = (*hw);
}

void hardware_set_id(hardware_t *hw, const char *id) {
	hw->id = malloc(strlen(id)+1);
	if(!hw->id) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(hw->id, id);
}

int hardware_gc(void) {
	struct hardware_t *htmp;
	struct conf_hardware_t *ctmp = NULL;

	while(hardware) {
		htmp = hardware;
		sfree((void *)&htmp->id);
		options_delete(htmp->options);
		hardware = hardware->next;
		sfree((void *)&htmp);
	}
	sfree((void *)&hardware);

	while(conf_hardware) {
		ctmp = conf_hardware;
		conf_hardware = conf_hardware->next;
		sfree((void *)&ctmp);
	}

	if(hwfile) {
		sfree((void *)&hwfile);
	}

	logprintf(LOG_DEBUG, "garbage collected hardware library");
	return EXIT_SUCCESS;
}

int hardware_write(char *content) {
	FILE *fp;

	/* Overwrite config file with proper format */
	if(!(fp = fopen(hwfile, "w+"))) {
		logprintf(LOG_ERR, "cannot write hardware file: %s", hwfile);
		return EXIT_FAILURE;
	}
	fseek(fp, 0L, SEEK_SET);
 	fwrite(content, sizeof(char), strlen(content), fp);
	fclose(fp);

	return EXIT_SUCCESS;
}

int hardware_parse(JsonNode *root) {
	struct conf_hardware_t *hnode = NULL;
	struct conf_hardware_t *tmp_confhw = NULL;
	struct options_t *hw_options = NULL;
	struct hardware_t *tmp_hardware = NULL;
	struct hardware_t *hw = NULL;

	JsonNode *jvalues = NULL;
	JsonNode *jchilds = json_first_child(root);

#ifndef __FreeBSD__
	regex_t regex;
	int reti;
	char *stmp = NULL;
#endif

	int i = 0, have_error = 0, match = 0;

	while(jchilds) {
		i++;
		/* A hardware module can only be a JSON object */
		if(jchilds->tag != 5) {
			logprintf(LOG_ERR, "hardware module #%d \"%s\", invalid format", i, jchilds->key);
			have_error = 1;
			goto clear;
		} else {
			/* Check if defined hardware module exists */
			tmp_hardware = hardware;
			match = 0;
			while(tmp_hardware) {
				if(strcmp(tmp_hardware->id, jchilds->key) == 0) {
					hw = tmp_hardware;
					match = 1;
					break;
				}
				tmp_hardware = tmp_hardware->next;
			}
			if(match == 0) {
				logprintf(LOG_ERR, "hardware module #%d \"%s\" does not exist", i, jchilds->key);
				have_error = 1;
				goto clear;
			}

			/* Check for duplicate hardware modules */
			tmp_confhw = conf_hardware;
			while(tmp_confhw) {
				/* Only allow one module of the same name */
				if(strcmp(tmp_confhw->hardware->id, jchilds->key) == 0) {
					logprintf(LOG_ERR, "hardware module #%d \"%s\", duplicate", i, jchilds->key);
					have_error = 1;
					goto clear;
				}
				/* And only allow one module covering the same frequency */
				if(tmp_confhw->hardware->type == hw->type) {
					logprintf(LOG_ERR, "hardware module #%d \"%s\", duplicate freq.", i, jchilds->key);
					have_error = 1;
					goto clear;
				}
				tmp_confhw = tmp_confhw->next;
			}

			/* Check if all options required by the hardware module are present */
			hw_options = hw->options;
			while(hw_options) {
				match = 0;
				jvalues = json_first_child(jchilds);
				while(jvalues) {
					if(jvalues->tag == JSON_NUMBER || jvalues->tag == JSON_STRING) {
						if(strcmp(jvalues->key, hw_options->name) == 0 && hw_options->argtype == OPTION_HAS_VALUE) {
							match = 1;
							break;
						}
					}
					jvalues = jvalues->next;
				}
				if(!match) {
					logprintf(LOG_ERR, "hardware module #%d \"%s\", setting \"%s\" missing", i, jchilds->key, hw_options->name);
					have_error = 1;
					goto clear;
				} else {
					/* Check if setting contains a valid value */
#ifndef __FreeBSD__
					if(jvalues->tag == JSON_NUMBER) {
						stmp = realloc(stmp, sizeof(jvalues->number_));
						if(!stmp) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						sprintf(stmp, "%d", (int)jvalues->number_);
					} else if(jvalues->tag == JSON_STRING) {
						stmp = realloc(stmp, strlen(jvalues->string_)+1);
						if(!stmp) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						strcpy(stmp, jvalues->string_);
					}
					reti = regcomp(&regex, hw_options->mask, REG_EXTENDED);
					if(reti) {
						logprintf(LOG_ERR, "could not compile regex");
						exit(EXIT_FAILURE);
					}
					reti = regexec(&regex, stmp, 0, NULL, 0);
					if(reti == REG_NOMATCH || reti != 0) {
						logprintf(LOG_ERR, "hardware module #%d \"%s\", setting \"%s\" invalid", i, jchilds->key, hw_options->name);
						have_error = 1;
						regfree(&regex);
						goto clear;
					}
					regfree(&regex);
					sfree((void *)&stmp);
#endif
				}
				hw_options = hw_options->next;
			}

			/* Check for any settings that are not valid for this hardware module */
			jvalues = json_first_child(jchilds);
			while(jvalues) {
				match = 0;
				if(jvalues->tag == JSON_NUMBER || jvalues->tag == JSON_STRING) {
					hw_options = hw->options;
					while(hw_options) {
						if(strcmp(jvalues->key, hw_options->name) == 0 && jvalues->tag == hw_options->vartype) {
							match = 1;
							break;
						}
						hw_options = hw_options->next;
					}
					if(!match) {
						logprintf(LOG_ERR, "hardware module #%d \"%s\", setting \"%s\" invalid", i, jchilds->key, jvalues->key);
						have_error = 1;
						goto clear;
					}
				}
				jvalues = jvalues->next;
			}

			if(hw->settings) {
				/* Sync all settings with the hardware module */
				jvalues = json_first_child(jchilds);
				while(jvalues) {
					if(hw->settings(jvalues) == EXIT_FAILURE) {
						logprintf(LOG_ERR, "hardware module #%d \"%s\", setting \"%s\" invalid", i, jchilds->key, jvalues->key);
						have_error = 1;
						goto clear;
					}
					jvalues = jvalues->next;
				}
			}

			hnode = malloc(sizeof(struct conf_hardware_t));
			if(!hnode) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			hnode->hardware = hw;
			hnode->next = conf_hardware;
			conf_hardware = hnode;

			jchilds = jchilds->next;
		}
	}

	sfree((void *)&tmp_confhw);
	json_delete(jchilds);
clear:
	return have_error;
}

int hardware_read(void) {
	FILE *fp;
	char *content;
	size_t bytes;
	JsonNode *root;
	struct stat st;

	/* Read JSON config file */
	if(!(fp = fopen(hwfile, "rb"))) {
		logprintf(LOG_ERR, "cannot read hardware file: %s", hwfile);
		return EXIT_FAILURE;
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if(!(content = calloc(bytes+1, sizeof(char)))) {
		logprintf(LOG_ERR, "out of memory");
		fclose(fp);
		return EXIT_FAILURE;
	}

	if(fread(content, sizeof(char), bytes, fp) == -1) {
		logprintf(LOG_ERR, "cannot read hardware file: %s", hwfile);
	}
	fclose(fp);

	/* Validate JSON and turn into JSON object */
	if(json_validate(content) == false) {
		logprintf(LOG_ERR, "hardware file is not in a valid json format", content);
		sfree((void *)&content);
		return EXIT_FAILURE;
	}

	root = json_decode(content);

	if(hardware_parse(root) != 0) {
		sfree((void *)&content);
		return EXIT_FAILURE;
	}

	if(!conf_hardware) {
		json_delete(root);
		root = json_decode("{\"none\":{}}");
		hardware_parse(root);
	}

	char *output = json_stringify(root, "\t");
	hardware_write(output);
	json_delete(root);
	sfree((void *)&output);
	sfree((void *)&content);
	return EXIT_SUCCESS;
}

int hardware_set_file(char *file) {
	if(access(file, R_OK | W_OK) != -1) {
		if(!(hwfile = realloc(hwfile, strlen(file)+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(hwfile, file);
	} else {
		fprintf(stderr, "%s: the hardware file %s does not exists\n", progname, file);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
