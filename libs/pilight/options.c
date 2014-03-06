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

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "common.h"
#include "options.h"

int getOptPos = 0;
char *longarg = NULL;
char *shortarg = NULL;
char *gctmp = NULL;

int options_gc(void) {
	sfree((void *)&longarg);
	sfree((void *)&shortarg);
	sfree((void *)&gctmp);

	logprintf(LOG_DEBUG, "garbage collected options library");
	return EXIT_SUCCESS;
}

/* Add a value to the specific struct id */
void options_set_value(struct options_t **opt, int id, const char *val) {
	struct options_t *temp = *opt;
	while(temp) {
		if(temp->id == id && temp->id > 0) {
			temp->value = realloc(temp->value, strlen(val)+1);
			if(!temp->value) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(temp->value, val);
			break;
		}
		temp = temp->next;
	}
}

/* Get a certain option value identified by the id */
int options_get_value(struct options_t **opt, int id, char **out) {
	struct options_t *temp = *opt;
	*out = NULL;	
	while(temp) {
		if(temp->id == id && temp->id > 0) {
			if(temp->value) {
				*out = temp->value;
				return 0;
			} else {
				return 1;
			}
		}
		temp = temp->next;
	}

	return 1;
}

/* Get a certain option argument type identified by the id */
int options_get_argtype(struct options_t **opt, int id, int *out) {
	struct options_t *temp = *opt;
	*out = 0;
	while(temp) {
		if(temp->id == id && temp->id > 0) {
			if(temp->argtype != 0) {
				*out = temp->argtype;
				return 0;
			} else {
				return 1;
			}
		}
		temp = temp->next;
	}

	return 1;
}

/* Get a certain option name identified by the id */
int options_get_name(struct options_t **opt, int id, char **out) {
	struct options_t *temp = *opt;
	*out = NULL;
	while(temp) {
		if(temp->id == id && temp->id > 0) {
			if(temp->name) {
				*out = temp->name;
				return 0;
			} else {
				return 1;
			}
		}
		temp = temp->next;
	}

	return 1;
}

/* Get a certain regex mask identified by the name */
int options_get_mask(struct options_t **opt, int id, char **out) {
	struct options_t *temp = *opt;
	*out = NULL;
	while(temp) {
		if(temp->id == id && temp->id > 0) {
			if(temp->mask) {
				*out = temp->mask;
				return 0;
			} else {
				return 1;
			}
		}
		temp = temp->next;
	}

	return 1;
}

/* Get a certain option id identified by the name */
int options_get_id(struct options_t **opt, char *name, int *out) {
	struct options_t *temp = *opt;
	*out = 0;
	while(temp) {
		if(temp->name) {
			if(strcmp(temp->name, name) == 0) {
				if(temp->id > 0) {
					*out = temp->id;
					return 0;
				} else {
					return 1;
				}
			}
		}
		temp = temp->next;
	}

	return 1;
}

/* Parse all CLI arguments */
int options_parse(struct options_t **opt, int argc, char **argv, int error_check, char **optarg) {
	int c = 0;
	int itmp = 0;
#ifndef __FreeBSD__	
	char *mask;
	regex_t regex;
	int reti;
#endif

	char *ctmp = NULL;
	
	/* If have read all arguments, exit and reset */
	if(getOptPos>=(argc-1)) {
		getOptPos=0;
		if(*optarg) {
			sfree((void *)&*optarg);
			*optarg = NULL;
		}
		return -1;
	} else {
		getOptPos++;
		/* Reserve enough memory to store all variables */
		longarg = realloc(longarg, 4);
		shortarg = realloc(shortarg, 2);
		*optarg = realloc(*optarg, 4);
		
		if(!longarg) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		if(!shortarg) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		if(!*optarg) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		
		/* The memory to null */
		memset(*optarg, '\0', 4);
		memset(shortarg, '\0', 2);
		memset(longarg, '\0', 4);
		
		/* Check if the CLI character contains an equals to (=) sign.
		   If it does, we have probably encountered a long argument */
		if(strchr(argv[getOptPos],'=')) {
			/* Copy all characters until the equals to sign.
			   This will probably be the name of the argument */
			longarg = realloc(longarg, strcspn(argv[getOptPos],"=")+1);			
			if(!longarg) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(longarg, '\0', strcspn(argv[getOptPos],"=")+1);
			memcpy(longarg, &argv[getOptPos][0], strcspn(argv[getOptPos],"="));

			/* Then copy everything after the equals sign.
			   This will probably be the value of the argument */
			size_t i = strlen(&argv[getOptPos][strcspn(argv[getOptPos],"=")+1]);
			*optarg = realloc(*optarg, i+1);
			if(!*optarg) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(*optarg, '\0', i+1);
			memcpy(*optarg, &argv[getOptPos][strcspn(argv[getOptPos],"=")+1], i);
		} else {
			/* If the argument does not contain a equals sign.
			   Store the argument to check later if it's a long argument */
			longarg = realloc(longarg, strlen(argv[getOptPos])+1);	
			if(!longarg) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(longarg, argv[getOptPos]);
		}

		/* A short argument only contains of two characters.
		   So only store the first two characters */
		shortarg = realloc(shortarg, strlen(argv[getOptPos])+1);
		if(!shortarg) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		memset(shortarg, '\0', 3);
		strncpy(shortarg, argv[getOptPos], 2);

		/* Check if the short argument and the long argument are equal,
		   then we probably encountered a short argument. Only store the
		   identifier character. If there are more CLI characters
		   after the current one, store it as the CLI value. However, only
		   do this if the first character of the argument doesn't contain*/
		if(strcmp(longarg, shortarg) == 0 && (getOptPos+1)<argc && argv[getOptPos+1][0] != '-') {
			*optarg = realloc(*optarg, strlen(argv[getOptPos+1])+1);
			if(!*optarg) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(*optarg, argv[getOptPos+1]);
			c = shortarg[1];
			getOptPos++;
		} else {
			/* If the short argument and the long argument are not equal,
			    then we probably encountered a long argument. */
			if(longarg[0] == '-' && longarg[1] == '-') {
				gctmp = realloc(gctmp, strlen(&longarg[2])+1);
				if(!gctmp) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(gctmp, &longarg[2]);

				/* Retrieve the short identifier for the long argument */
				if(options_get_id(opt, gctmp, &itmp) == 0) {
					c = itmp;
				} else if(argv[getOptPos][0] == '-') {
					c = shortarg[1];
				}
			} else if(argv[getOptPos][0] == '-' && strstr(shortarg, longarg) != 0) {
				c = shortarg[1];
			}
		}

		/* Check if the argument was expected */
		if(options_get_name(opt, c, &ctmp) != 0 && c > 0) {
			if(error_check == 1) {
				if(strcmp(longarg,shortarg) == 0) {
					if(shortarg[0] == '-') {
						logprintf(LOG_ERR, "invalid option -- '-%c'", c);
					} else {
						logprintf(LOG_ERR, "invalid option -- '%s'", longarg);
					}
				} else {
					logprintf(LOG_ERR, "invalid option -- '%s'", longarg);
				}
				goto gc;
			} else {
				return 0;
			}
		/* Check if an argument cannot have an argument that was set */
		} else if(strlen(*optarg) != 0 && options_get_argtype(opt, c, &itmp) == 0 && itmp == 1) {
			if(error_check == 1) {
				if(strcmp(longarg,shortarg) == 0) {
					logprintf(LOG_ERR, "option '-%c' doesn't take an argument", c);
				} else {
					logprintf(LOG_ERR, "option '%s' doesn't take an argument", longarg);
				}
				goto gc;
			} else {
				return 0;
			}
		/* Check if an argument required a value that wasn't set */
		} else if(strlen(*optarg) == 0 && options_get_argtype(opt, c, &itmp) == 0 && itmp == 2) {
			if(error_check == 1) {
				if(strcmp(longarg, shortarg) == 0) {
					logprintf(LOG_ERR, "option '-%c' requires an argument", c);
				} else {
					logprintf(LOG_ERR, "option '%s' requires an argument", longarg);
				}
				goto gc;
			} else {
				return 0;
			}
		/* Check if we have a valid argument */
		} else if(c == 0) {
			if(error_check == 1) {
				if(shortarg[0] == '-' && strstr(shortarg, longarg) != 0) {
					logprintf(LOG_ERR, "invalid option -- '-%c'", c);
				} else {
					logprintf(LOG_ERR, "invalid option -- '%s'", longarg);
				}
				goto gc;
			} else {
				return 0;
			}
		} else {
			/* If the argument didn't have a value, set it to 1 */
			if(strlen(*optarg) == 0) {
				options_set_value(opt, c, "1");
			} else {
#ifndef __FreeBSD__	
				if(error_check != 2) {
					/* If the argument has a regex mask, check if it passes */
					if(options_get_mask(opt, c, &mask) == 0) {
						reti = regcomp(&regex, mask, REG_EXTENDED);
						if(reti) {
							logprintf(LOG_ERR, "could not compile regex");
							goto gc;
						}
						reti = regexec(&regex, *optarg, 0, NULL, 0);
						if(reti == REG_NOMATCH || reti != 0) {
							if(error_check == 1) {
								if(shortarg[0] == '-') {
									logprintf(LOG_ERR, "invalid format -- '-%c'", c);
								} else {
									logprintf(LOG_ERR, "invalid format -- '%s'", longarg);
								}
								logprintf(LOG_ERR, "requires %s", mask);
							}
							regfree(&regex);
							goto gc;
						}
						regfree(&regex);
					}
				}
#endif
				options_set_value(opt, c, *optarg);
			}
			return c;
		}
	}

gc:
	getOptPos=0;
	sfree((void *)&*optarg);

	return -2;
}

/* Add a new option to the options struct */
void options_add(struct options_t **opt, int id, const char *name, int argtype, int conftype, int vartype, void *def, const char *mask) {
	char *ctmp = NULL;
	char *nname = malloc(strlen(name)+1);
	if(!nname) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(nname, name);
	int itmp;
	if(!(argtype >= 0 && argtype <= 3)) {
		logprintf(LOG_ERR, "tying to add an invalid option type");
		sfree((void *)&nname);
		exit(EXIT_FAILURE);
	} else if(!(conftype >= 0 && conftype <= 5)) {
		logprintf(LOG_ERR, "trying to add an option with an invalid config type");
		sfree((void *)&nname);
		exit(EXIT_FAILURE);
	} else if(!name) {
		logprintf(LOG_ERR, "trying to add an option without name");
		sfree((void *)&nname);
		exit(EXIT_FAILURE);
	} else if(id != 0 && options_get_name(opt, id, &ctmp) == 0) {
		logprintf(LOG_ERR, "duplicate option id: %c", id);
		sfree((void *)&nname);
		exit(EXIT_FAILURE);
	} else if(options_get_id(opt, nname, &itmp) == 0) {
		logprintf(LOG_ERR, "duplicate option name: %s", name);
		sfree((void *)&nname);
		exit(EXIT_FAILURE);
	} else {
		struct options_t *optnode = malloc(sizeof(struct options_t));
		if(!optnode) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		optnode->id = id;
		optnode->name = malloc(strlen(name)+1);
		if(!optnode->name) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(optnode->name, name);
		optnode->argtype = argtype;
		optnode->conftype = conftype;
		optnode->vartype = vartype;
		optnode->def = def;
		optnode->value = malloc(4);
		if(!optnode->value) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		memset(optnode->value, '\0', 4);
		if(mask) {
			optnode->mask = malloc(strlen(mask)+1);
			if(!optnode->mask) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(optnode->mask, mask);
		} else {
			optnode->mask = malloc(4);
			if(!optnode->mask) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(optnode->mask, '\0', 4);
		}
		optnode->next = *opt;
		*opt = optnode;
		sfree((void *)&nname);
	}
}

/* Merge two options structs */
void options_merge(struct options_t **a, struct options_t **b) {
	struct options_t *temp = NULL;
	temp = *b;
	while(temp) {
		struct options_t *optnode = malloc(sizeof(struct options_t));
		if(!optnode) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		optnode->id = temp->id;
		if(temp->name) {
			optnode->name = malloc(strlen(temp->name)+1);
			if(!optnode->name) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(optnode->name, '\0', strlen(temp->name)+1);
			strcpy(optnode->name, temp->name);
		} else {
			optnode->name = malloc(4);
			if(!optnode->name) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(optnode->name, '\0', 4);
		}
		if(temp->value) {
			optnode->value = malloc(strlen(temp->value)+1);
			if(!optnode->value) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(optnode->value, temp->value);
		} else {
			optnode->value = malloc(4);
			if(!optnode->value) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(optnode->value, '\0', 4);
		}
		if(temp->mask) {
			optnode->mask = malloc(strlen(temp->mask)*2);
			if(!optnode->mask) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(optnode->mask, temp->mask);
		} else {
			optnode->mask = malloc(4);
			if(!optnode->mask) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(optnode->mask, '\0', 4);
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

void options_delete(struct options_t *options) {
	struct options_t *tmp;
	while(options) {
		tmp = options;
		sfree((void *)&tmp->mask);
		sfree((void *)&tmp->value);
		sfree((void *)&tmp->name);
		options = options->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&options);
	
	logprintf(LOG_DEBUG, "freed options struct");
}
