/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _MEM_H_
#define _MEM_H_

#define OUT_OF_MEMORY fprintf(stderr, "out of memory in %s #%d\n", __FILE__, __LINE__),exit(EXIT_FAILURE);

int xfree(void);
// void mempool_init(unsigned long, unsigned long);
void memtrack(void);


// void *_malloc(unsigned long, char *, int);
// void *_realloc(void *, unsigned long, char *, int);
// void *_calloc(unsigned long a, unsigned long b, char *file, int line);
// void _free(void *, char *, int);

/*
  We only use these functions for extensive memory debugging
*/
void *_malloc(unsigned long a, const char *file, int line);
void *_realloc(void *a, unsigned long i, const char *file, int line);
/*
 * Windows already has a _strdup
 */
void *_calloc(unsigned long a, unsigned long b, const char *file, int line);
char *__strdup(char *a, const char *file, int line);
void _free(void *a, const char *file, int line);

#define MALLOC(a) _malloc(a, __FILE__, __LINE__)
#define REALLOC(a, b) _realloc(a, b, __FILE__, __LINE__)
#define CALLOC(a, b) _calloc(a, b, __FILE__, __LINE__)
#define STRDUP(a) __strdup(a, __FILE__, __LINE__)
#define FREE(a) _free((void *)(a), __FILE__, __LINE__),(a)=NULL

#define _MALLOC malloc
#define _REALLOC realloc
#define _CALLOC calloc
#define _FREE free
#define _STRDUP strdup

// #define MALLOC(a) malloc(a)
// #define REALLOC(a, b) realloc(a, b)
// #define CALLOC(a, b) calloc(a, b)
// #define FREE(a) free((void *)(a)),(a)=NULL

#endif
