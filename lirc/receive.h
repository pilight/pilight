/*      $Id: receive.h,v 5.7 2010/05/30 11:43:35 lirc Exp $      */

/****************************************************************************
 ** receive.h ***************************************************************
 ****************************************************************************
 *
 * functions that decode IR codes
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * Copyright (C) 2013 CurlyMo
 *
 */

#ifndef _RECEIVE_H
#define _RECEIVE_H

#include "ir_remote.h"

#define RBUF_SIZE (512)

#define REC_SYNC 8

#define MIN_RECEIVE_TIMEOUT 100000

struct rbuf {
	lirc_t data[RBUF_SIZE];
	ir_code decoded;
	int rptr;
	int wptr;
	int too_long;
	int is_biphase;
	lirc_t pendingp;
	lirc_t pendings;
	lirc_t sum;
	struct timeval last_signal_time;
};

static inline lirc_t receive_timeout(lirc_t usec)
{
	return 2 * usec < MIN_RECEIVE_TIMEOUT ? MIN_RECEIVE_TIMEOUT : 2 * usec;
}

inline lirc_t lirc_t_max(lirc_t a, lirc_t b);
void init_rec_buffer();
int clear_rec_buffer(void);
int receive_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flag,
		   lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
int clear_rec_buffer(void);
void rewind_rec_buffer(void);

#endif
