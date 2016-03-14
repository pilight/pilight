
#include "time.h"

unsigned long time_long_ns(struct timespec *ts) {
	unsigned long result = 0;
	result += ts->tv_sec * 1000000000;
	result += ts->tv_nsec;
	return result;
}

unsigned long time_long_us(struct timespec *ts) {
	unsigned long result = 0;
	result += ts->tv_sec * 1000000;
	result += ts->tv_nsec / 1000;
	return result;
}

void time_add_ns(struct timespec *ts, unsigned long nanos) {
	ts->tv_sec += nanos / 1000000000;
	ts->tv_nsec += nanos % 1000000000;
	ts->tv_sec += ts->tv_nsec / 1000000000;
	ts->tv_nsec %= 1000000000;
}

void time_add_us(struct timespec *ts, unsigned long micros) {
	ts->tv_sec += micros / 1000000;
	ts->tv_nsec += (micros % 1000000) * 1000;
	ts->tv_sec += ts->tv_nsec / 1000000000;
	ts->tv_nsec %= 1000000000;
}

void time_diff(struct timespec *a, struct timespec *b, struct timespec *c) {
	c->tv_sec = a->tv_sec - b->tv_sec;
	if(b->tv_nsec > a->tv_nsec) {
		c->tv_sec--;
		c->tv_nsec = 1000000000 - (b->tv_nsec - a->tv_nsec);
	} else {
		c->tv_nsec = a->tv_nsec - b->tv_nsec;
	}
}

int time_cmp(struct timespec *a, struct timespec *b) {
	if(a->tv_sec > b->tv_sec) {
		return 1;
	} else if(a->tv_sec < b->tv_sec) {
		return -1;
	} else {
		if(a->tv_nsec > b->tv_nsec) {
			return 1;
		} else if(a->tv_nsec < b->tv_nsec) {
			return -1;
		} else {
			return 0;
		}
	}
}
