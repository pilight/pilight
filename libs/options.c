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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h> 
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
void setOptionValById(struct options_t **options, int id, char *val) {
	struct options_t *temp = *options;
	while(temp != NULL) {
		if(temp->id == id) {
			temp->value = strdup(val);
			break;
		}
		temp = temp->next;
	}
}

/* Get a certain option value identified by the id */
char *getOptionValById(struct options_t **options, int id) {
	struct options_t *temp = *options;
	while(temp != NULL) {
		if(temp->id == id) {
			if(temp->value != NULL && strlen(temp->value) > 0) {
				return temp->value;
			} else {
				return '\0';
			}
		}
		temp = temp->next;
	}

	return '\0';
}

/* Get a certain option argument type identified by the id */
int getOptionArgTypeById(struct options_t **options, int id) {
	struct options_t *temp = *options;
	while(temp != NULL) {
		if(temp->id == id) {
			if(temp->argtype != 0)
				return temp->argtype;
			else
				return -1;
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option name identified by the id */
char *getOptionNameById(struct options_t **options, int id) {
	struct options_t *temp = *options;
	while(temp != NULL) {
		if(temp->id == id) {
			if(temp->name != NULL)
				return temp->name;
			else
				return '\0';
		}
		temp = temp->next;
	}

	return '\0';
}

/* Get a certain regex mask identified by the name */
char *getOptionMaskById(struct options_t **options, int id) {
	struct options_t *temp = *options;
	while(temp != NULL) {
		if(temp->id == id) {
			if(temp->mask != NULL)
				return temp->mask;
			else
				return '\0';
		}
		temp = temp->next;
	}

	return '\0';
}

/* Get a certain option id identified by the name */
int getOptionIdByName(struct options_t **options, char *name) {
	struct options_t *temp = *options;
	while(temp != NULL) {
		if(temp->name != NULL) {
			if(strcmp(temp->name,name) == 0) {
				if(temp->id > 0)
					return temp->id;
				else
					return -1;
			}
		}
		temp = temp->next;
	}

	return -1;
}

/* Get a certain option value identified by the name */
char *getOptionValByName(struct options_t **options, char *name) {
	struct options_t *temp = *options;
	while(temp != NULL) {
		if(temp->name != NULL) {
			if(strcmp(temp->name,name) == 0) {
				if(temp->value != NULL && strlen(temp->value) > 0)
					return temp->value;
				else
					return '\0';
			}
		}
		temp = temp->next;
	}

	return '\0';
}

/* Parse all CLI arguments */
int getOptions(struct options_t **options, int argc, char **argv, int error_check) {
	int c = 0;
	regex_t regex;
	int reti;
	char *mask;
	
	/* Reservate enough memory to store all variables */
	longarg = malloc(sizeof(char)*255);
	shortarg = malloc(sizeof(char)*3);
	optarg = malloc(sizeof(char)*255);

	/* Clear the argument holders */
	memset(longarg,'\0',sizeof(longarg));
	memset(shortarg,'\0',sizeof(shortarg));	
	memset(optarg,'\0',sizeof(optarg));	
	
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
			char *tmp = malloc(strlen(longarg));
			int tmp1 = -1;
			if(longarg[0] == '-' && longarg[1] == '-') {
				memcpy(tmp,&longarg[2],strlen(longarg));
				tmp[strlen(longarg)] = '\0';

				/* Retrieve the short identifier for the logn argument */
				if((tmp1 = getOptionIdByName(options, tmp)) > -1) {
					c=tmp1;
				} else if(argv[getOptPos][0] == '-') {
					c=shortarg[1];
				}
			} else if(argv[getOptPos][0] == '-' && strstr(shortarg, longarg) != 0) {
				c=shortarg[1];
			}
		}

		/* Check if the argument was expected */
		if(getOptionNameById(options, c) == NULL && c > 0) {
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
		} else if(strlen(optarg) != 0 && getOptionArgTypeById(options, c) == 1) {
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
		} else if(strlen(optarg) == 0 && getOptionArgTypeById(options, c) == 2) {
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
				setOptionValById(options, c, "1");
			else {
				/* If the argument has a regex mask, check if it passes */
				mask = getOptionMaskById(options, c);
				if(mask != NULL) {
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
				setOptionValById(options, c, optarg);
			}
			return c;
		}
	}
	return 0;
}

/* Add a new option to the options struct */
void addOption(struct options_t **options, int id, char *name, int argtype, int conftype, int child, char *mask) {
	if(!(argtype >= 0 && argtype <= 3)) {
		logprintf(LOG_ERR, "tying to add an invalid option type");
	} else if(!(child >= 0 && child <= 1)) {
		logprintf(LOG_ERR, "trying to add an option from an invalid parent");
	} else if(!(conftype >= 0 && conftype <= 2)) {
		logprintf(LOG_ERR, "tying to add an option with an invalid config type");
	} else if(name == NULL) {
		logprintf(LOG_ERR, "tying to add an option without name");
	} else if(getOptionNameById(options, id) != NULL) {
		logprintf(LOG_ERR, "duplicate option id: %s", id);
	} else if(getOptionIdByName(options, name) != -1) {
		logprintf(LOG_ERR, "duplicate option name: %s", name);
	} else {
		optnode = malloc(sizeof(struct options_t));	
		optnode->id = id;
		optnode->name=strdup(name);
		optnode->value = '\0';
		optnode->argtype = argtype;
		optnode->conftype = conftype;
		optnode->child = child;
		if(mask == NULL)
			optnode->mask = '\0';
		else
			optnode->mask = strdup(mask);
		optnode->next = *options;
		*options = optnode;
	}
}

/* Merge two options structs */
void mergeOptions(struct options_t **a, struct options_t **b) {
	struct options_t *temp = *b;
	while(temp->name != NULL) {
		optnode = malloc(sizeof(struct options_t));	
		optnode->id = temp->id;
		optnode->name=strdup(temp->name);
		if(temp->value == NULL)
			optnode->value='\0';
		else
			optnode->value=strdup(temp->value);
		if(temp->mask == NULL)
			optnode->mask='\0';
		else
			optnode->mask=strdup(temp->mask);
		optnode->argtype = temp->argtype;
		optnode->conftype = temp->conftype;
		optnode->child = temp->child;
		optnode->next = *a;
		*a = optnode;
		temp = temp->next;
	}	
}