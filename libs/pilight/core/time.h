
#ifndef _TIME_H_
#define _TIME_H_

#include <time.h>

unsigned long time_long_ns(struct timespec *ts);
unsigned long time_long_us(struct timespec *ts);
void time_add_ns(struct timespec *ts, unsigned long nanos);
void time_add_us(struct timespec *ts, unsigned long micros);
void time_diff(struct timespec *a, struct timespec *b, struct timespec *c);
int time_cmp(struct timespec *a, struct timespec *b);

#endif
