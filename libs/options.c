/*
	Copyright (C) 2013 CurlyMo

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

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "options.h"

/* The long CLI argument holder */
static char *longarg;
/* The abbreviated CLI argument holder */
static char *shortarg;
/* The value of the CLI argument */
extern char *optarg;

int getOptPos = 0;

/* Add a value to the specific struct id */
void setOptionValById(struct options_t **opt, int id, const char *val) {
	struct options_t *temp = *opt;
	while(temp != NULL) {
		if(temp->id == id && temp->id > 0) {
			strcpy(temp->value,val);
			break;
		}
		temp = temp->next;
	}
}

/* Get a certain option value identified by the id */
int getOptionValById(struct options_t **opt, int id, char **out) {
	struct options_t *temp = *opt;
	*out = NULL;	
	while(temp != NULL) {
		if(temp->id == id && temp->id > 0) {
			if(temp->value != NULL && strlen(temp->value) > 0) {
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
int getOptionArgTypeById(struct options_t **opt, int id, int *out) {
	struct options_t *temp = *opt;
	*out = 0;
	while(temp != NULL) {
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
int getOptionNameById(struct options_t **opt, int id, char **out) {
	struct options_t *temp = *opt;
	*out = NULL;
	while(temp != NULL) {
		if(temp->id == id && temp->id > 0) {
			if(temp->name != NULL) {
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
int getOptionMaskById(struct options_t **opt, int id, char **out) {
	struct options_t *temp = *opt;
	*out = NULL;
	while(temp != NULL) {
		if(temp->id == id && temp->id > 0) {
			if(temp->mask != NULL) {
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
int getOptionIdByName(struct options_t **opt, char *name, int *out) {
	struct options_t *temp = *opt;
	*out = 0;
	while(temp != NULL) {
		if(temp->name != NULL) {
			if(strcmp(temp->name,name) == 0) {
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

/* Get a certain option value identified by the name */
int getOptionValByName(struct options_t **opt, char *name, char **out) {
	struct options_t *temp = *opt;
	*out = NULL;
	while(temp != NULL) {
		if(temp->name != NULL) {
			if(strcmp(temp->name,name) == 0) {
				if(temp->value != NULL && strlen(temp->value) > 0) {
					*out = temp->value;
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
int getOptions(struct options_t **opt, int argc, char **argv, int error_check) {
	int c = 0;
	char *ctmp = NULL;
	int itmp;
#ifndef __FreeBSD__	
	char *mask;
	regex_t regex;
	int reti;
#endif
	
	/* Reservate enough memory to store all variables */
	longarg = malloc(sizeof(char)*255);
	shortarg = malloc(sizeof(char)*3);
	optarg = malloc(sizeof(char)*255);

	/* Clear the argument holders */
	memset(longarg, '\0', sizeof(char)*255);
	memset(shortarg, '\0', sizeof(char)*3);
	memset(optarg, '\0', sizeof(char)*255);

	/* If have readed all arguments, exit and reset */
	if(getOptPos>=(argc-1)) {
		getOptPos=0;
		return -1;
	} else {
		getOptPos++;

		/* Check if the CLI character contains an equals to (=) sign.
		   If it does, we have probably encountered a long argument */
		if(strchr(argv[getOptPos],'=') != NULL) {
			/* Copy all characters until the equals to sign.
			   This will probably be the name of the argument */
			memcpy(longarg, &argv[getOptPos][0], strcspn(argv[getOptPos],"="));
			longarg[strcspn(argv[getOptPos],"=")]='\0';

			/* Then copy everything after the equals sign.
			   This will probably be the value of the argument */
			memcpy(optarg, &argv[getOptPos][strcspn(argv[getOptPos],"=")+1], strlen(argv[getOptPos]));
			optarg[strlen(argv[getOptPos])-(strcspn(argv[getOptPos],"=")+1)]='\0';
		} else {
			/* If the argument does not contain a equals sign.
			   Store the argument to check later if it's a long argument */
			longarg=strdup(argv[getOptPos]);
		}

		/* A short argument only contains of two characters.
		   So only store the first two characters */
		memcpy(shortarg, &argv[getOptPos][0], 2);

		/* Check if the short argument and the long argument are equal,
		   then we probably encountered a short argument. Only store the
		   identifier character. If there are more CLI characters
		   after the current one, store it as the CLI value. However, only
		   do this if the first character of the argument doesn't contain*/
		if(strcmp(longarg,shortarg) == 0 && (getOptPos+1)<argc && argv[getOptPos+1][0] != '-') {
			optarg=strdup(argv[getOptPos+1]);
			c=shortarg[1];
			getOptPos++;
		} else {
			/* If the short argument and the long argument are not equal,
			    then we probably encountered a long argument. */
			ctmp = malloc(strlen(longarg));
			if(longarg[0] == '-' && longarg[1] == '-') {
				memcpy(ctmp, &longarg[2], strlen(longarg));
				ctmp[strlen(longarg)] = '\0';

				/* Retrieve the short identifier for the logn argument */
				if(getOptionIdByName(opt, ctmp, &itmp) == 0) {
					c=itmp;
				} else if(argv[getOptPos][0] == '-') {
					c=shortarg[1];
				}
			} else if(argv[getOptPos][0] == '-' && strstr(shortarg, longarg) != 0) {
				c=shortarg[1];
			}
		}

		/* Check if the argument was expected */
		if(getOptionNameById(opt, c, &ctmp) != 0 && c > 0) {
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
				exit(EXIT_FAILURE);
			} else {
				return 0;
			}
		/* Check if an argument cannot have an argument that was set */
		} else if(strlen(optarg) != 0 && getOptionArgTypeById(opt, c, &itmp) == 0 && itmp == 1) {
			if(error_check == 1) {
				if(strcmp(longarg,shortarg) == 0) {
					logprintf(LOG_ERR, "option '-%c' doesn't take an argument", c);
				} else {
					logprintf(LOG_ERR, "option '%s' doesn't take an argument", longarg);
				}
				exit(EXIT_FAILURE);
			} else {
				return 0;
			}
		/* Check if an argument required a value that wasn't set */
		} else if(strlen(optarg) == 0 && getOptionArgTypeById(opt, c, &itmp) == 0 && itmp == 2) {
			if(error_check == 1) {
				if(strcmp(longarg, shortarg) == 0) {
					logprintf(LOG_ERR, "option '-%c' requires an argument", c);
				} else {
					logprintf(LOG_ERR, "option '%s' requires an argument", longarg);
				}
				exit(EXIT_FAILURE);
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
				exit(EXIT_FAILURE);
			} else {
				return 0;
			}
		} else {
			/* If the argument didn't have a value, set it to 1 */
			if(strlen(optarg) == 0)
				setOptionValById(opt, c, "1");
			else {
#ifndef __FreeBSD__			
				/* If the argument has a regex mask, check if it passes */
				if(getOptionMaskById(opt, c, &mask) == 0) {
					reti = regcomp(&regex, mask, REG_EXTENDED);
					if(reti) {
						logprintf(LOG_ERR, "could not compile regex");
						exit(EXIT_FAILURE);
					}
					reti = regexec(&regex, optarg, 0, NULL, 0);
					if(reti == REG_NOMATCH || reti != 0) {
						if(shortarg[0] == '-') {
							logprintf(LOG_ERR, "invalid format -- '-%c'", c);
						} else {
							logprintf(LOG_ERR, "invalid format -- '%s'", longarg);
						}
						logprintf(LOG_ERR, "requires %s", mask);
						exit(EXIT_FAILURE);
					}
				}
#endif
				setOptionValById(opt, c, optarg);
			}
			return c;
		}
	}
}

/* Add a new option to the options struct */
void addOption(struct options_t **opt, int id, const char *name, int argtype, int conftype, const char *mask) {
	char *ctmp = NULL;
	int itmp;
	if(!(argtype >= 0 && argtype <= 3)) {
		logprintf(LOG_ERR, "tying to add an invalid option type");
	} else if(!(conftype >= 0 && conftype <= 5)) {
		logprintf(LOG_ERR, "trying to add an option with an invalid config type");
	} else if(name == NULL) {
		logprintf(LOG_ERR, "trying to add an option without name");
	} else if(getOptionNameById(opt, id, &ctmp) == 0) {
		logprintf(LOG_ERR, "duplicate option id: %c", id);
	} else if(getOptionIdByName(opt, strdup(name), &itmp) == 0) {
		logprintf(LOG_ERR, "duplicate option name: %s", name);
	} else {
		optnode = malloc(sizeof(struct options_t));
		optnode->id = id;
		strcpy(optnode->name, name);
		memset(optnode->value, '\0', sizeof(optnode->value));
		optnode->argtype = argtype;
		optnode->conftype = conftype;
		if(mask == NULL)
			memset(optnode->value, '\0', sizeof(optnode->mask));
		else
			strcpy(optnode->mask, mask);
		optnode->next = *opt;
		*opt = optnode;
	}
}

/* Merge two options structs */
void mergeOptions(struct options_t **a, struct options_t **b) {
	struct options_t *temp = *b;
	while(temp != NULL && temp->name != NULL) {
		optnode = malloc(sizeof(struct options_t));
		optnode->id = temp->id;
		strcpy(optnode->name, temp->name);
		if(temp->value == NULL)
			memset(optnode->value, '\0', sizeof(optnode->value));
		else
			strcpy(optnode->value, temp->value);
		if(temp->mask == NULL)
			memset(optnode->value, '\0', sizeof(optnode->mask));
		else
			strcpy(optnode->mask, temp->mask);
		optnode->argtype = temp->argtype;
		optnode->conftype = temp->conftype;
		optnode->next = *a;
		*a = optnode;
		temp = temp->next;
	}
}
