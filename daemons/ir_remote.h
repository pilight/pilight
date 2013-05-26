/*      $Id: ir_remote.h,v 5.44 2010/04/11 18:50:38 lirc Exp $      */

/****************************************************************************
 ** ir_remote.h *************************************************************
 ****************************************************************************
 *
 * ir_remote.h - describes and decodes the signals from IR remotes
 *
 * Copyright (C) 1996,97 Ralph Metzler <rjkm@thp.uni-koeln.de>
 * Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "../drivers/lirc.h"
#include "hardware.h"

#include "ir_remote_types.h"

extern struct hardware hw;

static inline ir_code get_ir_code(struct ir_ncode *ncode, struct ir_code_node *node)
{
	if (ncode->next && node != NULL)
		return node->code;
	return ncode->code;
}

static inline struct ir_code_node *get_next_ir_code_node(struct ir_ncode *ncode, struct ir_code_node *node)
{
	if (node == NULL)
		return ncode->next;
	return node->next;
}

static inline int bit_count(struct ir_remote *remote)
{
	return remote->pre_data_bits + remote->bits + remote->post_data_bits;
}

static inline int bits_set(ir_code data)
{
	int ret = 0;
	while (data) {
		if (data & 1)
			ret++;
		data >>= 1;
	}
	return ret;
}

static inline ir_code reverse(ir_code data, int bits)
{
	int i;
	ir_code c;

	c = 0;
	for (i = 0; i < bits; i++) {
		c |= (ir_code) (((data & (((ir_code) 1) << i)) ? 1 : 0))
		    << (bits - 1 - i);
	}
	return (c);
}

static inline int is_pulse(lirc_t data)
{
	return (data & PULSE_BIT ? 1 : 0);
}

static inline int is_space(lirc_t data)
{
	return (!is_pulse(data));
}

static inline int has_repeat(struct ir_remote *remote)
{
	if (remote->prepeat > 0 && remote->srepeat > 0)
		return (1);
	else
		return (0);
}

static inline void set_protocol(struct ir_remote *remote, int protocol)
{
	remote->flags &= ~(IR_PROTOCOL_MASK);
	remote->flags |= protocol;
}

static inline int is_raw(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == RAW_CODES)
		return (1);
	else
		return (0);
}

static inline int is_space_enc(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == SPACE_ENC)
		return (1);
	else
		return (0);
}

static inline int is_space_first(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == SPACE_FIRST)
		return (1);
	else
		return (0);
}

static inline int is_rc5(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == RC5)
		return (1);
	else
		return (0);
}

static inline int is_rc6(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == RC6 || remote->rc6_mask)
		return (1);
	else
		return (0);
}

static inline int is_biphase(struct ir_remote *remote)
{
	if (is_rc5(remote) || is_rc6(remote))
		return (1);
	else
		return (0);
}

static inline int is_rcmm(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == RCMM)
		return (1);
	else
		return (0);
}

static inline int is_goldstar(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == GOLDSTAR)
		return (1);
	else
		return (0);
}

static inline int is_grundig(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == GRUNDIG)
		return (1);
	else
		return (0);
}

static inline int is_bo(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == BO)
		return (1);
	else
		return (0);
}

static inline int is_serial(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == SERIAL)
		return (1);
	else
		return (0);
}

static inline int is_xmp(struct ir_remote *remote)
{
	if ((remote->flags & IR_PROTOCOL_MASK) == XMP)
		return (1);
	else
		return (0);
}

static inline int is_const(struct ir_remote *remote)
{
	if (remote->flags & CONST_LENGTH)
		return (1);
	else
		return (0);
}

static inline int has_repeat_gap(struct ir_remote *remote)
{
	if (remote->repeat_gap > 0)
		return (1);
	else
		return (0);
}

static inline int has_pre(struct ir_remote *remote)
{
	if (remote->pre_data_bits > 0)
		return (1);
	else
		return (0);
}

static inline int has_post(struct ir_remote *remote)
{
	if (remote->post_data_bits > 0)
		return (1);
	else
		return (0);
}

static inline int has_header(struct ir_remote *remote)
{
	if (remote->phead > 0 && remote->shead > 0)
		return (1);
	else
		return (0);
}

static inline int has_foot(struct ir_remote *remote)
{
	if (remote->pfoot > 0 && remote->sfoot > 0)
		return (1);
	else
		return (0);
}

static inline int has_toggle_bit_mask(struct ir_remote *remote)
{
	if (remote->toggle_bit_mask > 0)
		return (1);
	else
		return (0);
}

static inline int has_ignore_mask(struct ir_remote *remote)
{
	if (remote->ignore_mask > 0)
		return (1);
	else
		return (0);
}

static inline int has_toggle_mask(struct ir_remote *remote)
{
	if (remote->toggle_mask > 0)
		return (1);
	else
		return (0);
}

static inline lirc_t min_gap(struct ir_remote *remote)
{
	if (remote->gap2 != 0 && remote->gap2 < remote->gap) {
		return remote->gap2;
	} else {
		return remote->gap;
	}
}

static inline lirc_t max_gap(struct ir_remote *remote)
{
	if (remote->gap2 > remote->gap) {
		return remote->gap2;
	} else {
		return remote->gap;
	}
}

/* check if delta is inside exdelta +/- exdelta*eps/100 */

static inline int expect(struct ir_remote *remote, lirc_t delta, lirc_t exdelta)
{
	int aeps = hw.resolution > remote->aeps ? hw.resolution : remote->aeps;

	if (abs(exdelta - delta) <= exdelta * remote->eps / 100 || abs(exdelta - delta) <= aeps)
		return 1;
	return 0;
}

static inline int expect_at_least(struct ir_remote *remote, lirc_t delta, lirc_t exdelta)
{
	int aeps = hw.resolution > remote->aeps ? hw.resolution : remote->aeps;

	if (delta + exdelta * remote->eps / 100 >= exdelta || delta + aeps >= exdelta) {
		return 1;
	}
	return 0;
}

static inline int expect_at_most(struct ir_remote *remote, lirc_t delta, lirc_t exdelta)
{
	int aeps = hw.resolution > remote->aeps ? hw.resolution : remote->aeps;

	if (delta <= exdelta + exdelta * remote->eps / 100 || delta <= exdelta + aeps) {
		return 1;
	}
	return 0;
}

static inline lirc_t upper_limit(struct ir_remote *remote, lirc_t val)
{
	int aeps = hw.resolution > remote->aeps ? hw.resolution : remote->aeps;
	lirc_t eps_val = val * (100 + remote->eps) / 100;
	lirc_t aeps_val = val + aeps;
	return eps_val > aeps_val ? eps_val : aeps_val;
}

static inline lirc_t lower_limit(struct ir_remote *remote, lirc_t val)
{
	int aeps = hw.resolution > remote->aeps ? hw.resolution : remote->aeps;
	lirc_t eps_val = val * (100 - remote->eps) / 100;
	lirc_t aeps_val = val - aeps;

	if (eps_val <= 0)
		eps_val = 1;
	if (aeps_val <= 0)
		aeps_val = 1;

	return eps_val < aeps_val ? eps_val : aeps_val;
}

/* only works if last <= current */
static inline unsigned long time_elapsed(struct timeval *last, struct timeval *current)
{
	unsigned long secs, diff;

	secs = current->tv_sec - last->tv_sec;

	diff = 1000000 * secs + current->tv_usec - last->tv_usec;

	return (diff);
}

static inline ir_code gen_mask(int bits)
{
	int i;
	ir_code mask;

	mask = 0;
	for (i = 0; i < bits; i++) {
		mask <<= 1;
		mask |= 1;
	}
	return (mask);
}

static inline ir_code gen_ir_code(struct ir_remote *remote, ir_code pre, ir_code code, ir_code post)
{
	ir_code all;

	all = (pre & gen_mask(remote->pre_data_bits));
	all <<= remote->bits;
	all |= is_raw(remote) ? code : (code & gen_mask(remote->bits));
	all <<= remote->post_data_bits;
	all |= post & gen_mask(remote->post_data_bits);

	return all;
}

void get_frequency_range(struct ir_remote *remotes, unsigned int *min_freq, unsigned int *max_freq);
void get_filter_parameters(struct ir_remote *remotes, lirc_t * max_gap_lengthp, lirc_t * min_pulse_lengthp,
			   lirc_t * min_space_lengthp, lirc_t * max_pulse_lengthp, lirc_t * max_space_lengthp);
struct ir_remote *is_in_remotes(struct ir_remote *remotes, struct ir_remote *remote);
struct ir_remote *get_ir_remote(struct ir_remote *remotes, char *name);
int map_code(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int pre_bits, ir_code pre,
	     int bits, ir_code code, int post_bits, ir_code post);
void map_gap(struct ir_remote *remote, struct timeval *start, struct timeval *last, lirc_t signal_length,
	     int *repeat_flagp, lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
struct ir_ncode *get_code_by_name(struct ir_remote *remote, char *name);
struct ir_ncode *get_code(struct ir_remote *remote, ir_code pre, ir_code code, ir_code post,
			  ir_code * toggle_bit_mask_state);
__u64 set_code(struct ir_remote *remote, struct ir_ncode *found, ir_code toggle_bit_mask_state, int repeat_flag,
	       lirc_t min_remaining_gap, lirc_t max_remaining_gap);
int write_message(char *buffer, size_t size, const char *remote_name, const char *button_name,
		  const char *button_suffix, ir_code code, int reps);
char *decode_all(struct ir_remote *remotes);
int send_ir_ncode(struct ir_remote *remote, struct ir_ncode *code);

#endif
