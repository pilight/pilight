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

#ifndef _EVENT_OPERATOR_H_
#define _EVENT_OPERATOR_H_

typedef struct event_operators_t {
	char *name;
	void (*callback_string)(char *a, char *b, char **ret);
	void (*callback_number)(double a, double b, char **ret);
	unsigned short type;
	struct event_operators_t *next;
} event_operators_t;

struct event_operators_t *event_operators;

void event_operator_init(void);
void event_operator_register(struct event_operators_t **op, const char *name);
int event_operator_gc(void);

#endif
