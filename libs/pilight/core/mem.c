/*
	Copyright (C) 2013 - 2015 CurlyMo

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

#if defined(DEBUG) && !defined(__mips__) && !defined(__aarch64__)

#ifdef _MSC_VER
	#define _CRT_SECURE_NO_DEPRECATE
	#define _CRT_SECURE_NO_WARNINGS
#endif

#define _GNU_SOURCE
#include <dlfcn.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <unistd.h>
	#include <pthread.h>
#else
	#include <windows.h>
	#define strdup(a) _strdup(a)
#endif
#include <mbedtls/md5.h>

#include "mem.h"
#include "log.h"

#define MAX_SIZE 64

static unsigned short memdbg = 0;
static int lockinit = 0;
static unsigned long openallocs = 0;
static unsigned long totalnrallocs = 0;
#ifdef _WIN32
	HANDLE lock;
#else
	static pthread_mutex_t lock;
	static pthread_mutexattr_t attr;
#endif

struct mallocs_t {
	void *p;
	unsigned long size;
	int line;
	char file[255];

	void *backtrace[MAX_SIZE];
	int btsize;

	struct mallocs_t *next;
} mallocs_t;

struct summary_t {
	int idx;
	char md5[33+sizeof(int)];
	struct mallocs_t *mallocs;
	unsigned int allocs;
	unsigned int calls;
} summary_t;

static struct mallocs_t *mallocs = NULL;

void heaptrack(void) {
	memdbg = 1;

	if(unw_set_caching_policy(unw_local_addr_space, UNW_CACHE_PER_THREAD)) {
		fprintf(stderr, "WARNING: Failed to enable per-thread libunwind caching.\n");
	}

	if(lockinit == 0) {
		lockinit = 1;
#ifdef _WIN32
		lock = CreateMutex(NULL, FALSE, NULL);
#else
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&lock, &attr);
#endif
	}
}

int compare(const void *c, const void *d) {
	const struct summary_t * const *a = (void *)c;
	const struct summary_t * const *b = (void *)d;

	if(a == b) {
		return 0;
	} else if((*a)->calls == 0 && (*b)->calls > 0) {
		return 1;
	} else if((*a)->calls > 0 && (*b)->calls == 0) {
		return -1;
	} else if((*a)->calls == 0 && (*b)->calls == 0) {
		return 0;
	} else if(strcmp((*a)->mallocs->file, (*b)->mallocs->file) == 0 && (*a)->mallocs->line == (*b)->mallocs->line) {
		return 0;
	} else {
		if(strcmp((*a)->mallocs->file, (*b)->mallocs->file) == 0) {
			if((*a)->mallocs->line < (*b)->mallocs->line) {
				return -1;
			} else {
				return 1;
			}
		} else {
			return strcmp((*a)->mallocs->file, (*b)->mallocs->file);
		}
	}
}

int memanalyze(void) {
	if(memdbg == 1) {
#ifdef _WIN32
		DWORD dwWaitResult = WaitForSingleObject(lock, INFINITE);
#else
		pthread_mutex_lock(&lock);
#endif

		FILE *fp = NULL;
		char buffer[255];

		time_t rawtime = time(NULL);
		sprintf(buffer,"/var/log/pilight_%d", (int)rawtime);

		if((fp = fopen(buffer, "w+")) == NULL) {
			return EXIT_FAILURE;
		}

		fseek(fp, 0L, SEEK_SET);

		char spaces[1024] = { '\0' };
		memset(&spaces, ' ', 1023);

		unsigned int totalsize = 0;
		unsigned int totalcalls = 0;
		unsigned char md5sum[17];
		char md5conv[33+sizeof(int)];
		char *p = (char *)md5sum;
		int i = 0, x = 0;

		struct summary_t **summary = malloc(sizeof(struct summary_t *)*MAX_SIZE);
		struct summary_t *sum = NULL;
		int sumlen = MAX_SIZE;

		for(i=0;i<sumlen;i++) {
			summary[i] = malloc(sizeof(struct summary_t));
			memset(summary[i], 0, sizeof(struct summary_t));
		}

		struct mallocs_t *tmp = mallocs;
		while(tmp) {
			mbedtls_md5((const unsigned char *)&tmp->backtrace, tmp->btsize, (unsigned char *)p);
			for(i = 0; i < 32; i+=2) {
				sprintf(&md5conv[i], "%02x", md5sum[i/2] );
			}
			sprintf(&md5conv[i], "%d", tmp->line);

			for(i = 0; i < sumlen; i++) {
				if(summary[i]->calls == 0) {
					sum = summary[i];
				}
				if(strcmp(summary[i]->md5, md5conv) == 0) {
					sum = summary[i];
					break;
				}
			}
			if(i == sumlen) {
				summary = realloc(summary, sizeof(struct summary_t *)*(sumlen + MAX_SIZE));
				for(i=sumlen;i<sumlen+MAX_SIZE;i++) {
					summary[i] = malloc(sizeof(struct summary_t));
					memset(summary[i], 0, sizeof(struct summary_t));
				}
				sum = summary[sumlen];
				sumlen += MAX_SIZE;
			}
			sum->allocs += tmp->size;
			totalsize += tmp->size;
			totalcalls++;
			sum->calls++;
			sum->mallocs = tmp;
			strcpy(sum->md5, md5conv);

			tmp = tmp->next;
		}

		qsort(summary, sumlen-1, sizeof(struct summary_t *), compare);

		fprintf(fp, "a total of %d calls allocated %d bytes\n\n", totalcalls, totalsize);
		for(x=0;x<sumlen;x++) {
			if(summary[x]->calls > 0) {
				fprintf(fp, "%d calls in %s at line #%d allocated %d bytes\n", summary[x]->calls, summary[x]->mallocs->file, summary[x]->mallocs->line, summary[x]->allocs);

				for(i=0;i<summary[x]->mallocs->btsize;i++) {
					Dl_info info;

					void *const addr = (void *)summary[x]->mallocs->backtrace[i];
					if(dladdr(addr, &info) != 0 && info.dli_fname != NULL) {
						if((info.dli_sname != NULL && strstr(info.dli_sname, "main") != NULL) || i > 10) {
							break;
						}
						if(info.dli_sname != NULL) {
							fprintf(fp, "%.*s [%s] %s\n", i, spaces, info.dli_fname, info.dli_sname);
						} else {
							fprintf(fp, "%.*s [%s] %s\n", i, spaces, info.dli_fname, "<unknown>");
						}
					}
				}
			}
			free(summary[x]);
		}
		free(summary);
		fclose(fp);
#ifdef _WIN32
		ReleaseMutex(lock);
#else
		pthread_mutex_unlock(&lock);
#endif

		totalnrallocs = 0;
	}
	return openallocs;
}

void *__malloc(unsigned long a, const char *file, int line) {
	if(memdbg == 1) {
		struct mallocs_t *node = malloc(sizeof(struct mallocs_t));
		if(node == NULL || ((node->p = malloc(a)) == NULL)) {
			fprintf(stderr, "out of memory in %s at line #%d\n", file, line);
			free(node);
			memanalyze();
			exit(EXIT_FAILURE);
		}
#ifdef _WIN32
		InterlockedIncrement(&openallocs);
		InterlockedIncrement(&totalnrallocs);
#else
		__sync_add_and_fetch(&openallocs, 1);
		__sync_add_and_fetch(&totalnrallocs, 1);
#endif

		node->btsize = unw_backtrace(node->backtrace, MAX_SIZE);

		node->size = a;
		node->line = line;
		strcpy(node->file, file);

#ifdef _WIN32
		DWORD dwWaitResult = WaitForSingleObject(lock, INFINITE);
#else
		pthread_mutex_lock(&lock);
#endif
		node->next = mallocs;
		mallocs = node;
#ifdef _WIN32
		ReleaseMutex(lock);
#else
		pthread_mutex_unlock(&lock);
#endif

		return node->p;
	} else {
		return malloc(a);
	}
}

char *___strdup(char *a, const char *file, int line) {
	char *d = __malloc(strlen(a) + 1, file, line);
	if(d == NULL) {
		return NULL;
	}
	strcpy(d, a);
	return d;
}

void *__realloc(void *a, unsigned long b, const char *file, int line) {
	if(memdbg == 1) {
		if(a == NULL) {
			return __malloc(b, file, line);
		} else {
#ifdef _WIN32
			DWORD dwWaitResult = WaitForSingleObject(lock, INFINITE);
#else
			pthread_mutex_lock(&lock);
#endif
			struct mallocs_t *tmp = mallocs;
			while(tmp) {
				if(tmp->p == a) {
					tmp->line = line;
					strcpy(tmp->file, file);
					tmp->size = b;
					if((a = realloc(a, b)) == NULL) {
						fprintf(stderr, "out of memory in %s at line #%d\n", file, line);
#ifdef _WIN32
						ReleaseMutex(lock);
#else
						pthread_mutex_unlock(&lock);
#endif
						memanalyze();
						exit(EXIT_FAILURE);
					}
					tmp->p = a;
					tmp->btsize = unw_backtrace(tmp->backtrace, MAX_SIZE);

					break;
				}
				tmp = tmp->next;
			}
#ifdef _WIN32
			ReleaseMutex(lock);
#else
			pthread_mutex_unlock(&lock);
#endif
			if(tmp == NULL) {
				fprintf(stderr, "ERROR: calling realloc on an unknown pointer in %s at line #%d\n", file, line);
				return __malloc(b, file, line);
			} else if(tmp != NULL && tmp->p != NULL) {
				return a;
			} else {
				return __malloc(b, file, line);
			}
		}
	} else {
		return realloc(a, b);
	}
}

void *__calloc(unsigned long a, unsigned long b, const char *file, int line) {
	if(memdbg == 1) {
		struct mallocs_t *node = malloc(sizeof(mallocs_t));
		if((node->p = malloc(a*b)) == NULL) {
			fprintf(stderr, "out of memory in %s at line #%d\n", file, line);
			free(node);
			memanalyze();
			exit(EXIT_FAILURE);
		}
#ifdef _WIN32
		InterlockedIncrement(&openallocs);
		InterlockedIncrement(&totalnrallocs);
#else
		__sync_add_and_fetch(&openallocs, 1);
		__sync_add_and_fetch(&totalnrallocs, 1);
#endif

		memset(node->p, '\0', a*b);
		node->btsize = unw_backtrace(node->backtrace, MAX_SIZE);
		node->size = a*b;
		node->line = line;
		strcpy(node->file, file);

#ifdef _WIN32
		DWORD dwWaitResult = WaitForSingleObject(lock, INFINITE);
#else
		pthread_mutex_lock(&lock);
#endif
		node->next = mallocs;
		mallocs = node;
#ifdef _WIN32
		ReleaseMutex(lock);
#else
		pthread_mutex_unlock(&lock);
#endif

		return node->p;
	} else {
		return calloc(a, b);
	}
}

void __free(void *a, const char *file, int line) {
	if(memdbg == 1) {
		if(a == NULL) {
			fprintf(stderr, "WARNING: calling free on already freed pointer in %s at line #%d\n", file, line);
		} else {
#ifdef _WIN32
			DWORD dwWaitResult = WaitForSingleObject(lock, INFINITE);
#else
			pthread_mutex_lock(&lock);
#endif
			struct mallocs_t *currP, *prevP;
			int match = 0;

			prevP = NULL;

			for(currP = mallocs; currP != NULL; prevP = currP, currP = currP->next) {
				if(currP->p == a) {
					match = 1;
					if(prevP == NULL) {
						mallocs = currP->next;
					} else {
						prevP->next = currP->next;
					}
					free(currP->p);
					free(currP);

					break;
				}
			}
#ifdef _WIN32
			ReleaseMutex(lock);
			InterlockedDecrement(&openallocs);
#else
			__sync_add_and_fetch(&openallocs, -1);
			pthread_mutex_unlock(&lock);
#endif

			if(match == 0) {
				free(a);
			}
		}
	} else {
		free(a);
	}
}

#endif