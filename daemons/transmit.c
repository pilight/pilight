/*      $Id: transmit.c,v 5.31 2010/04/02 10:26:57 lirc Exp $      */

/****************************************************************************
 ** transmit.c **************************************************************
 ****************************************************************************
 *
 * functions that prepare IR codes for transmitting
 * 
 * Copyright (C) 1999-2004 Christoph Bartelmus <lirc@bartelmus.de>
 * Copyright (C) 2013	   CurlyMo
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* if the gap is lower than this value, we will concatenate the
   signals and send the signal chain at a single blow */
#define LIRCD_EXACT_GAP_THRESHOLD 10000

#include <stdio.h>
#include <stdlib.h>
#include "lircd.h"
#include "transmit.h"

struct sbuf send_buffer;

void init_send_buffer(void) {
	memset(&send_buffer, 0, sizeof(send_buffer));
}

inline void clear_send_buffer(void) {
	LOGPRINTF(3, "clearing transmit buffer");
	send_buffer.wptr = 0;
	send_buffer.too_long = 0;
	send_buffer.is_biphase = 0;
	send_buffer.pendingp = 0;
	send_buffer.pendings = 0;
	send_buffer.sum = 0;
}


int init_sim(struct ir_remote *remote, struct ir_ncode *code, int repeat_preset) { 
	return 1;
}

int init_send(struct ir_ncode *code) {
	clear_send_buffer();
	send_buffer.data = code->signals;
	send_buffer.wptr = code->length;
	
	return 1;
}
