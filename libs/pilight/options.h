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

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#define OPTION_NO_VALUE				1
#define OPTION_HAS_VALUE	 		2
#define OPTION_OPT_VALUE	 	3

#define CONFIG_ID		1
#define CONFIG_STATE	2
#define CONFIG_VALUE	3
#define CONFIG_SETTING	4

typedef struct options_t {
	int id;
	char *name;
	char *value;
	char *mask;
	void *def;
	int argtype;
	int conftype;
	int vartype;
	struct options_t *next;
} options_t;

int options_gc(void);
void options_set_value(struct options_t **options, int id, const char *val);
int options_get_value(struct options_t **options, int id, char **out);
int options_get_argtype(struct options_t **options, int id, int *out);
int options_get_name(struct options_t **options, int id, char **out);
int options_get_id(struct options_t **options, char *name, int *out);
int options_get_mask(struct options_t **options, int id, char **out);
int options_parse(struct options_t **options, int argc, char **argv, int error_check, char **optarg);
void options_add(struct options_t **options, int id, const char *name, int argtype, int conftype, int vartype, void *def, const char *mask);
void options_merge(struct options_t **a, struct options_t **b);
void options_delete(struct options_t *options);

#endif
