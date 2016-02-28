/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#define OPTION_NO_VALUE			1
#define OPTION_HAS_VALUE	 	2
#define OPTION_OPT_VALUE	 	3

#define DEVICES_ID					1
#define DEVICES_STATE				2
#define DEVICES_VALUE				3
#define DEVICES_SETTING			4
#define DEVICES_OPTIONAL		5
#define GUI_SETTING					6
#define NROPTIONTYPES				6


typedef struct options_t {
	int id;
	char *name;
	union {
		char *string_;
		double number_;
	};
	char *mask;
	void *def;
	int argtype;
	int conftype;
	int vartype;
	struct options_t *next;
} options_t;

int options_gc(void);
void options_set_string(struct options_t **options, int id, const char *val);
void options_set_number(struct options_t **options, int id, double val);
int options_get_string(struct options_t **options, int id, char **out);
int options_get_number(struct options_t **options, int id, double *out);
int options_get_argtype(struct options_t **options, int id, int *out);
int options_get_name(struct options_t **options, int id, char **out);
int options_get_id(struct options_t **options, char *name, int *out);
int options_get_mask(struct options_t **options, int id, char **out);
int options_parse(struct options_t **options, int argc, char **argv, int error_check, char **optarg);
void options_add(struct options_t **options, int id, const char *name, int argtype, int conftype, int vartype, void *def, const char *mask);
void options_merge(struct options_t **a, struct options_t **b);
void options_delete(struct options_t *options);

#endif
