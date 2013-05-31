/*      $Id: transmit.h,v 5.7 2010/04/02 10:26:57 lirc Exp $      */

/****************************************************************************
 ** transmit.h **************************************************************
 ****************************************************************************
 *
 * functions that prepare IR codes for transmitting
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * Copyright (C) 2013 CurlyMo
 *
 */

#ifndef _TRANSMIT_H
#define _TRANSMIT_H

#include "ir_remote.h"

#define WBUF_SIZE (256)

struct sbuf {
	lirc_t *data;
	lirc_t _data[WBUF_SIZE];
	int wptr;
	int too_long;
	int is_biphase;
	lirc_t pendingp;
	lirc_t pendings;
	lirc_t sum;
};

void init_send_buffer(void);
inline void set_bit(ir_code * code, int bit, int data);
int init_send(struct ir_ncode *code);
int init_sim(struct ir_remote *remote, struct ir_ncode *code, int repeat_preset);

extern struct sbuf send_buffer;

#endif
