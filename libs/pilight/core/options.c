/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _WIN32
	#include <regex.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "common.h"
#include "mem.h"
#include "options.h"
#include "json.h"

int options_gc(void) {
	logprintf(LOG_DEBUG, "garbage collected options library");
	return EXIT_SUCCESS;
}

/* Add a value to the specific struct id */
void options_set_string(struct options_t *opt, char *id, const char *val) {

	struct options_t *temp = opt;
	while(temp) {
		if(temp->id != NULL && id != NULL && strcmp(temp->id, id) == 0) {
			if((temp->string_ = REALLOC(temp->string_, strlen(val)+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			temp->set = 1;
			temp->vartype = JSON_STRING;
			strcpy(temp->string_, val);
			break;
		}
		temp = temp->next;
	}
}

void options_set_null(struct options_t *opt, char *id) {

	struct options_t *temp = opt;
	while(temp) {
		if(temp->id != NULL && id != NULL && strcmp(temp->id, id) == 0) {
			temp->vartype = JSON_NULL;
			temp->string_ = NULL;
			temp->set = 1;
			break;
		}
		temp = temp->next;
	}
}

/* Add a value to the specific struct id */
void options_set_number(struct options_t *opt, char *id, int val) {

	struct options_t *temp = opt;
	while(temp) {
		if(temp->id != NULL && id != NULL && strcmp(temp->id, id) == 0) {
			temp->number_ = val;
			temp->vartype = JSON_NUMBER;
			temp->set = 1;
			break;
		}
		temp = temp->next;
	}
}

/* Get a certain option value identified by the id */
int options_get_string(struct options_t *opt, char *id, char **out) {

	struct options_t *temp = opt;
	*out = NULL;
	while(temp) {
		if(temp->id != NULL && id != NULL && strcmp(temp->id, id) == 0) {
			if(temp->string_ != NULL && temp->vartype == JSON_STRING) {
				*out = temp->string_;
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option value identified by the id */
int options_get_number(struct options_t *opt, char *id, int *out) {

	struct options_t *temp = opt;
	*out = 0;
	while(temp) {
		if(temp->id != NULL && id != NULL && strcmp(temp->id, id) == 0) {
			if(temp->vartype == JSON_NUMBER) {
				*out = temp->number_;
			}
			return 0;
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option value identified by the id */
int options_exists(struct options_t *opt, char *id) {

	struct options_t *temp = opt;
	while(temp) {
		if(temp->id != NULL && id != NULL && strcmp(temp->id, id) == 0) {
			if(temp->set == 1) {
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option argument type identified by the id */
static int options_get_argtype(struct options_t *opt, char *id, int is_long, int *out) {
	if(id == NULL) {
		return -1;
	}

	struct options_t *temp = opt;
	*out = 0;
	while(temp) {
		if((is_long == 0 && temp->id != NULL && strcmp(temp->id, id) == 0) ||
			 (is_long == 1 && temp->name != NULL && strcmp(temp->name, id) == 0)) {
			if(temp->argtype != 0) {
				*out = temp->argtype;
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option argument type identified by the id */
static int options_get_conftype(struct options_t *opt, char *id, int is_long, int *out) {

	struct options_t *temp = opt;
	*out = 0;
	while(temp) {
		if((is_long == 0 && temp->id != NULL && strcmp(temp->id, id) == 0) ||
			 (is_long == 1 && temp->name != NULL && strcmp(temp->name, id) == 0)) {
			if(temp->conftype != 0) {
				*out = temp->conftype;
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option argument type identified by the id */
// static int options_get_vartype(struct options_t *opt, char *id, int is_long, int *out) {

	// struct options_t *temp = opt;
	// *out = 0;
	// while(temp) {
		// if((is_long == 0 && temp->id != NULL && strcmp(temp->id, id) == 0) ||
			 // (is_long == 1 && temp->name != NULL && strcmp(temp->name, id) == 0)) {
			// if(temp->vartype != 0) {
				// *out = temp->vartype;
				// return 0;
			// } else {
				// return -1;
			// }
		// }
		// temp = temp->next;
	// }

	// return -1;
// }

/* Get a certain option name identified by the id */
int options_get_name(struct options_t *opt, char *name, char **out) {

	struct options_t *temp = opt;
	*out = NULL;
	while(temp) {
		if(temp->name != NULL && name != NULL && strcmp(temp->name, name) == 0) {
			if(temp->name) {
				*out = temp->name;
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain regex mask identified by the name */
int options_get_mask(struct options_t *opt, char *id, int is_long, char **out) {
	if(id == NULL) {
		return -1;
	}

	struct options_t *temp = opt;
	*out = NULL;
	while(temp) {
		if((is_long == 0 && temp->id != NULL && strcmp(temp->id, id) == 0) ||
			 (is_long == 1 && temp->name != NULL && strcmp(temp->name, id) == 0)) {
			if(temp->mask) {
				*out = temp->mask;
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option id identified by the name */
int options_get_id(struct options_t *opt, char *id, char **out) {
	struct options_t *temp = opt;
	*out = 0;
	while(temp) {
		if(strcmp(temp->id, id) == 0) {
			if(temp->id != NULL) {
				*out = temp->id;
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option id identified by the name */
int options_get_id_by_name(struct options_t *opt, char *name, char **out) {
	struct options_t *temp = opt;
	*out = 0;
	while(temp) {
		if(strcmp(temp->name, name) == 0) {
			if(temp->id != NULL) {
				*out = temp->id;
				return 0;
			} else {
				return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

int options_parse1(struct options_t **opt, int argc, char **argv, int error_check, char **optarg, char **ret) {
	return 0;
}

/* Parse all CLI arguments */
int options_parse(struct options_t *opt, int argc, char **argv) {
	char *str = NULL, *key = NULL, *val = NULL, *name = NULL;
	int len = 0, x = 0, i = 0, is_long = 0, quote = 0, itmp = 0, tmp = 0;

#if !defined(__FreeBSD__) && !defined(_WIN32)
	char *mask;
	regex_t regex;
	int reti;
#endif

	for(i=1;i<argc;i++) {
		x = strlen(argv[i]);
		if((str = REALLOC(str, len+x+1)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(&str[len], 0, x);
		memcpy(&str[len], argv[i], x);
		str[len+x] = '\0';
		len += x+1;
	}
	if(argc > 1) {
		str[len-1] = '\0';
	}

	i = 0;
	while(i < len) {
		if((is_long = (strncmp(str, "--", 2) == 0)) || strncmp(str, "-", 1) == 0) {
			if(is_long == 1) {
				memmove(&str[0], &str[2], len-2);
				len-=2;
			} else {
				memmove(&str[0], &str[1], len-1);
				len-=1;
			}
			i = -1;
			str[len] = '\0';
			x = 0;
			while(x <= len) {
				if((str[x] == '=' || str[x] == '\0' || x == len)) {
					tmp = str[x];
					str[x] = '\0';
					if((key = STRDUP(str)) == NULL) {
						OUT_OF_MEMORY
					}
					str[x] = tmp;
					x++;
					break;
				}
				x++;
			}

			if(x < len) {
				memmove(&str[0], &str[x], len-x);
				i = -1;
			}
			len -= x;
		}
		if((((strncmp(str, "--", 2) == 0 || strncmp(str, "-", 1) == 0) && tmp == '=') ||
				(strncmp(str, "--", 2) != 0 && strncmp(str, "-", 1) != 0 && (tmp == '\0' || tmp == '='))) && len > 0) {
			x = 0;
			while(x <= len) {
				if((str[x] == '\0' || x == len)) {
					if((val = STRDUP(str)) == NULL) {
						OUT_OF_MEMORY
					}
					int len1 = strlen(val);
					if(((quote = str[0]) == '"' || str[0] == '\'') &&
						val[len1-1] == quote) {
						memmove(&val[0], &val[1], len1-1);
						val[len1-2] = '\0';
					}
					x++;
					break;
				}
				x++;
			}
			if(x < len) {
				memmove(&str[0], &str[x], len-x);
				i = -1;
			}
			len -= x;
		}

		if(key == NULL) {
			logprintf(LOG_ERR, "invalid option -- '%s'", str);
			if(val != NULL) {
				FREE(val);
			}
			FREE(str);
			return -1;
		} else if(is_long == 1 && key != NULL && options_get_name(opt, key, &name) != 0) {
			logprintf(LOG_ERR, "invalid option -- '--%s'", key);
			FREE(key);
			if(val != NULL) {
				FREE(val);
			}
			FREE(str);
			return -1;
		} else if(is_long == 0 && key != NULL && options_get_id(opt, key, &name) != 0) {
			logprintf(LOG_ERR, "invalid option -- '-%s'", key);
			FREE(key);
			if(val != NULL) {
				FREE(val);
			}
			FREE(str);
			return -1;
		} else if(val != NULL && options_get_argtype(opt, key, is_long, &itmp) == 0 && itmp == OPTION_NO_VALUE) {
			if(is_long == 1) {
				logprintf(LOG_ERR, "option '--%s' does not take an argument", key);
			} else {
				logprintf(LOG_ERR, "option '-%s' does not take an argument", key);
			}
			FREE(key);
			if(val != NULL) {
				FREE(val);
			}
			FREE(str);
			return -1;
		} else if(val == NULL && options_get_argtype(opt, key, is_long, &itmp) == 0 && itmp == OPTION_HAS_VALUE) {
			if(is_long == 1) {
				logprintf(LOG_ERR, "option '--%s' requires an argument", key);
			} else {
				logprintf(LOG_ERR, "option '-%s' requires an argument", key);
			}
			FREE(key);
			if(val != NULL) {
				FREE(val);
			}
			FREE(str);
			return -1;
		}

#if !defined(__FreeBSD__) && !defined(_WIN32)
		if(val != NULL) {
			/* If the argument has a regex mask, check if it passes */
			if(options_get_mask(opt, key, is_long, &mask) == 0) {
				reti = regcomp(&regex, mask, REG_EXTENDED);
				if(reti) {
					logprintf(LOG_ERR, "could not compile regex");
					FREE(val);
					FREE(key);
					FREE(str);
					return -1;
				}

				reti = regexec(&regex, val, 0, NULL, 0);
				if(reti == REG_NOMATCH || reti != 0) {
					if(is_long == 1) {
						logprintf(LOG_ERR, "invalid format -- '--%c'", key);
					} else {
						logprintf(LOG_ERR, "invalid format -- '-%s'", key);
					}
					logprintf(LOG_ERR, "requires %s", mask);

					FREE(val);
					FREE(key);
					FREE(str);
					regfree(&regex);
					return -1;
				}
				regfree(&regex);
			}
		}
#endif
		if(options_get_id_by_name(opt, key, &name) == 0) {
			FREE(key);
			if(val != NULL) {
				if(isNumeric(val) == 0) {
					options_set_number(opt, name, atoi(val));
				} else {
					options_set_string(opt, name, val);
				}
				FREE(val);
			} else {
				options_set_null(opt, name);
			}
		} else {
			if(val != NULL) {
				if(isNumeric(val) == 0) {
					options_set_number(opt, key, atoi(val));
				} else {
					options_set_string(opt, key, val);
				}
				FREE(val);
			} else {
				options_set_null(opt, key);
			}
			FREE(key);
		}
		i++;
	}
	if(str != NULL) {
		FREE(str);
	}
	return 0;
}

/* Add a new option to the options struct */
void options_add(struct options_t **opt, char *id, const char *name, int argtype, int conftype, int vartype, void *def, const char *mask) {

	char *ctmp = NULL;
	char *nname = MALLOC(strlen(name)+1);
	char *sid = NULL;
	if(nname == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(nname, name);
	int itmp = 0;
	if(!(argtype >= 0 && argtype <= 3)) {
		logprintf(LOG_CRIT, "tying to add an invalid option type");
		FREE(nname);
		exit(EXIT_FAILURE);
	} else if(!(conftype >= 0 && conftype <= NROPTIONTYPES)) {
		logprintf(LOG_CRIT, "trying to add an option of an invalid type");
		FREE(nname);
		exit(EXIT_FAILURE);
	} else if(!name) {
		logprintf(LOG_CRIT, "trying to add an option without name");
		FREE(nname);
		exit(EXIT_FAILURE);
	} else if(id == NULL) {
		logprintf(LOG_CRIT, "option id cannot be null: %s", name);
		FREE(nname);
	} else if(options_get_name(*opt, id, &ctmp) == 0) {
		logprintf(LOG_CRIT, "duplicate option id: %s", id);
		FREE(nname);
		exit(EXIT_FAILURE);
	} else if(options_get_id(*opt, nname, &sid) == 0 &&
			((options_get_conftype(*opt, sid, 0, &itmp) == 0 && itmp == conftype) ||
			(options_get_conftype(*opt, sid, 0, &itmp) != 0))) {
		logprintf(LOG_CRIT, "duplicate option name: %s", name);
		FREE(nname);
		exit(EXIT_FAILURE);
	} else {
		struct options_t *optnode = MALLOC(sizeof(struct options_t));
		if(optnode == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		if((optnode->id = STRDUP(id)) == NULL) {
			OUT_OF_MEMORY
		}
		if((optnode->name = MALLOC(strlen(name)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(optnode->name, name);
		optnode->argtype = argtype;
		optnode->conftype = conftype;
		optnode->vartype = vartype;
		optnode->def = def;
		optnode->string_ = NULL;
		optnode->set = 0;
		if(mask) {
			if((optnode->mask = MALLOC(strlen(mask)+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(optnode->mask, mask);
		} else {
			optnode->mask = NULL;
		}
		optnode->next = *opt;
		*opt = optnode;
		FREE(nname);
	}
}

/* Merge two options structs */
void options_merge(struct options_t **a, struct options_t **b) {

	struct options_t *temp = NULL;
	temp = *b;
	while(temp) {
		struct options_t *optnode = MALLOC(sizeof(struct options_t));
		if(optnode == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		if((optnode->id = STRDUP(temp->id)) == NULL) {
			OUT_OF_MEMORY
		}
		if(temp->name) {
			if((optnode->name = MALLOC(strlen(temp->name)+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			memset(optnode->name, '\0', strlen(temp->name)+1);
			strcpy(optnode->name, temp->name);
		} else {
			optnode->name = NULL;
		}
		if(temp->string_) {
			if((optnode->string_ = MALLOC(strlen(temp->string_)+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			optnode->vartype = JSON_STRING;
			strcpy(optnode->string_, temp->string_);
		} else {
			optnode->string_ = NULL;
		}
		if(temp->mask) {
			if((optnode->mask = MALLOC(strlen(temp->mask)+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(optnode->mask, temp->mask);
		} else {
			optnode->mask = NULL;
		}
		optnode->argtype = temp->argtype;
		optnode->conftype = temp->conftype;
		optnode->vartype = temp->vartype;
		optnode->def = temp->def;
		optnode->next = *a;
		*a = optnode;
		temp = temp->next;
	}
}

int options_list(struct options_t *options, int i, char **name) {
	struct options_t *tmp = options;
	int x = 0;
	while(tmp) {
		if(i == x) {
			*name = tmp->id;
			return 0;
		}
		x++;
		tmp = tmp->next;
	}
	return -1;
}

void options_delete(struct options_t *options) {

	struct options_t *tmp;
	while(options) {
		tmp = options;
		if(tmp->mask) {
			FREE(tmp->mask);
		}
		if(tmp->vartype == JSON_STRING && tmp->string_ != NULL) {
			FREE(tmp->string_);
		}
		if(tmp->id != NULL) {
			FREE(tmp->id);
		}
		if(tmp->name != NULL) {
			FREE(tmp->name);
		}
		options = options->next;
		FREE(tmp);
	}
	if(options != NULL) {
		FREE(options);
	}

	if(pilight.debuglevel >= 2) {
		fprintf(stderr, "freed options struct\n");
	}
}
