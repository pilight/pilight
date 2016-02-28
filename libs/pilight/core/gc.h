/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _GC_H_
#define _GC_H_

#include <signal.h>

typedef struct collectors_t {
	int (*listener)(void);
	struct collectors_t *next;
} collectors_t;

void gc_handler(int signal);
void gc_attach(int (*fp)(void));
void gc_catch(void);
int gc_run(void);
int running(void);
void gc_clear(void);

extern volatile sig_atomic_t gcloop;

#endif
