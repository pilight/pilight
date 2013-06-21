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

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#define no_argument			1
#define required_argument 	2
#define optional_argument 	3

#define config_required		1
#define config_value		2

typedef struct options_t options_t;

struct options_t {
	int id;
	char name[255];
	char value[255];
	char mask[255];
	int argtype;
	int conftype;
	struct options_t *next;
};

options_t *options;
options_t *optnode;

void setOptionValById(struct options_t **options, int id, char *val);
char *getOptionValById(struct options_t **options, int id);
int getOptionArgTypeById(struct options_t **options, int id);
char *getOptionNameById(struct options_t **options, int id);
int getOptionIdByName(struct options_t **options, char *name);
char *getOptionValByName(struct options_t **options, char *name);
int getOptions(struct options_t **options, int argc, char **argv, int error_check);
void addOption(struct options_t **options, int id, char *name, int argtype, int conftype, char *mask);
void mergeOptions(struct options_t **a, struct options_t **b);

#endif
