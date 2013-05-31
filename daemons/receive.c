/*      $Id: receive.c,v 5.43 2010/04/13 16:28:10 lirc Exp $      */

/****************************************************************************
 ** receive.c ***************************************************************
 ****************************************************************************
 *
 * functions that decode IR codes
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * Copyright (C) 2013 CurlyMo
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <limits.h>

#include "hardware.h"
#include "lircd.h"
#include "receive.h"

extern struct hardware hw;
extern struct ir_remote *last_remote;

struct rbuf rec_buffer;

inline lirc_t lirc_t_max(lirc_t a, lirc_t b)
{
	return (a > b ? a : b);
}

inline void set_pending_pulse(lirc_t deltap)
{
	logprintf(5, "pending pulse: %lu", deltap);
	rec_buffer.pendingp = deltap;
}

inline void set_pending_space(lirc_t deltas)
{
	logprintf(5, "pending space: %lu", deltas);
	rec_buffer.pendings = deltas;
}

static lirc_t get_next_rec_buffer_internal(lirc_t maxusec)
{
	if (rec_buffer.rptr < rec_buffer.wptr) {
		logprintf(3, "<%c%lu", rec_buffer.data[rec_buffer.rptr] & PULSE_BIT ? 'p' : 's', (__u32)
			  rec_buffer.data[rec_buffer.rptr] & (PULSE_MASK));
		rec_buffer.sum += rec_buffer.data[rec_buffer.rptr] & (PULSE_MASK);
		return (rec_buffer.data[rec_buffer.rptr++]);
	} else {
		if (rec_buffer.wptr < RBUF_SIZE) {
			lirc_t data = 0;
			unsigned long elapsed = 0;

			if (timerisset(&rec_buffer.last_signal_time)) {
				struct timeval current;

				gettimeofday(&current, NULL);
				elapsed = time_elapsed(&rec_buffer.last_signal_time, &current);
			}
			if (elapsed < maxusec) {
				data = hw.readdata(maxusec - elapsed);
			}
			if (!data) {
				logprintf(3, "timeout: %u", maxusec);
				return 0;
			}
			if (LIRC_IS_TIMEOUT(data)) {
				logprintf(1, "timeout received: %lu", (__u32) LIRC_VALUE(data));
				if (LIRC_VALUE(data) < maxusec) {
					return get_next_rec_buffer_internal(maxusec - LIRC_VALUE(data));
				}
				return 0;
			}

			rec_buffer.data[rec_buffer.wptr] = data;
			if (rec_buffer.data[rec_buffer.wptr] == 0)
				return (0);
			rec_buffer.sum += rec_buffer.data[rec_buffer.rptr]
			    & (PULSE_MASK);
			rec_buffer.wptr++;
			rec_buffer.rptr++;
			logprintf(3, "+%c%lu", rec_buffer.data[rec_buffer.rptr - 1] & PULSE_BIT ? 'p' : 's', (__u32)
				  rec_buffer.data[rec_buffer.rptr - 1]
				  & (PULSE_MASK));
			return (rec_buffer.data[rec_buffer.rptr - 1]);
		} else {
			rec_buffer.too_long = 1;
			return (0);
		}
	}
	return (0);
}

lirc_t get_next_rec_buffer(lirc_t maxusec)
{
	return get_next_rec_buffer_internal(receive_timeout(maxusec));
}

void init_rec_buffer(void)
{
	memset(&rec_buffer, 0, sizeof(rec_buffer));
}

void rewind_rec_buffer(void)
{
	rec_buffer.rptr = 0;
	rec_buffer.too_long = 0;
	set_pending_pulse(0);
	set_pending_space(0);
	rec_buffer.sum = 0;
}

int clear_rec_buffer(void)
{
	int move, i;

	timerclear(&rec_buffer.last_signal_time);
	if (hw.rec_mode == LIRC_MODE_LIRCCODE) {
		unsigned char buffer[sizeof(ir_code)];
		size_t count;

		count = hw.code_length / CHAR_BIT;
		if (hw.code_length % CHAR_BIT)
			count++;

		if (read(hw.fd, buffer, count) != count) {
			logprintf(LOG_ERR, "reading in mode LIRC_MODE_LIRCCODE failed");
			return (0);
		}
		for (i = 0, rec_buffer.decoded = 0; i < count; i++) {
			rec_buffer.decoded = (rec_buffer.decoded << CHAR_BIT) + ((ir_code) buffer[i]);
		}
	} else {
		lirc_t data;

		move = rec_buffer.wptr - rec_buffer.rptr;
		if (move > 0 && rec_buffer.rptr > 0) {
			memmove(&rec_buffer.data[0], &rec_buffer.data[rec_buffer.rptr],
				sizeof(rec_buffer.data[0]) * move);
			rec_buffer.wptr -= rec_buffer.rptr;
		} else {
			rec_buffer.wptr = 0;
			data = hw.readdata(0);

			logprintf(3, "c%lu", (__u32) data & (PULSE_MASK));

			rec_buffer.data[rec_buffer.wptr] = data;
			rec_buffer.wptr++;
		}
	}

	rewind_rec_buffer();
	rec_buffer.is_biphase = 0;

	return (1);
}

inline void unget_rec_buffer(int count)
{
	logprintf(5, "unget: %d", count);
	if (count == 1 || count == 2) {
		rec_buffer.rptr -= count;
		rec_buffer.sum -= rec_buffer.data[rec_buffer.rptr] & (PULSE_MASK);
		if (count == 2) {
			rec_buffer.sum -= rec_buffer.data[rec_buffer.rptr + 1]
			    & (PULSE_MASK);
		}
	}
}

inline void unget_rec_buffer_delta(lirc_t delta)
{
	rec_buffer.rptr--;
	rec_buffer.sum -= delta & (PULSE_MASK);
	rec_buffer.data[rec_buffer.rptr] = delta;
}

inline lirc_t get_next_pulse(lirc_t maxusec)
{
	lirc_t data;

	data = get_next_rec_buffer(maxusec);
	if (data == 0)
		return (0);
	if (!is_pulse(data)) {
		logprintf(2, "pulse expected");
		return (0);
	}
	return (data & (PULSE_MASK));
}

inline lirc_t get_next_space(lirc_t maxusec)
{
	lirc_t data;

	data = get_next_rec_buffer(maxusec);
	if (data == 0)
		return (0);
	if (!is_space(data)) {
		logprintf(2, "space expected");
		return (0);
	}
	return (data);
}

inline int sync_pending_pulse(struct ir_remote *remote)
{
	if (rec_buffer.pendingp > 0) {
		lirc_t deltap;

		deltap = get_next_pulse(rec_buffer.pendingp);
		if (deltap == 0)
			return 0;
		if (!expect(remote, deltap, rec_buffer.pendingp))
			return 0;
		set_pending_pulse(0);
	}
	return 1;
}

inline int sync_pending_space(struct ir_remote *remote)
{
	if (rec_buffer.pendings > 0) {
		lirc_t deltas;

		deltas = get_next_space(rec_buffer.pendings);
		if (deltas == 0)
			return 0;
		if (!expect(remote, deltas, rec_buffer.pendings))
			return 0;
		set_pending_space(0);
	}
	return 1;
}

int expectpulse(struct ir_remote *remote, int exdelta)
{
	lirc_t deltap;
	int retval;

	logprintf(5, "expecting pulse: %lu", exdelta);
	if (!sync_pending_space(remote))
		return 0;

	deltap = get_next_pulse(rec_buffer.pendingp + exdelta);
	if (deltap == 0)
		return (0);
	if (rec_buffer.pendingp > 0) {
		if (rec_buffer.pendingp > deltap)
			return 0;
		retval = expect(remote, deltap - rec_buffer.pendingp, exdelta);
		if (!retval)
			return (0);
		set_pending_pulse(0);
	} else {
		retval = expect(remote, deltap, exdelta);
	}
	return (retval);
}

int expectspace(struct ir_remote *remote, int exdelta)
{
	lirc_t deltas;
	int retval;

	logprintf(5, "expecting space: %lu", exdelta);
	if (!sync_pending_pulse(remote))
		return 0;

	deltas = get_next_space(rec_buffer.pendings + exdelta);
	if (deltas == 0)
		return (0);
	if (rec_buffer.pendings > 0) {
		if (rec_buffer.pendings > deltas)
			return 0;
		retval = expect(remote, deltas - rec_buffer.pendings, exdelta);
		if (!retval)
			return (0);
		set_pending_space(0);
	} else {
		retval = expect(remote, deltas, exdelta);
	}
	return (retval);
}

inline int expectone(struct ir_remote *remote, int bit)
{
	if (is_biphase(remote)) {
		int all_bits = bit_count(remote);
		ir_code mask;

		mask = ((ir_code) 1) << (all_bits - 1 - bit);
		if (mask & remote->rc6_mask) {
			if (remote->sone > 0 && !expectspace(remote, 2 * remote->sone)) {
				unget_rec_buffer(1);
				return (0);
			}
			set_pending_pulse(2 * remote->pone);
		} else {
			if (remote->sone > 0 && !expectspace(remote, remote->sone)) {
				unget_rec_buffer(1);
				return (0);
			}
			set_pending_pulse(remote->pone);
		}
	} else if (is_space_first(remote)) {
		if (remote->sone > 0 && !expectspace(remote, remote->sone)) {
			unget_rec_buffer(1);
			return (0);
		}
		if (remote->pone > 0 && !expectpulse(remote, remote->pone)) {
			unget_rec_buffer(2);
			return (0);
		}
	} else {
		if (remote->pone > 0 && !expectpulse(remote, remote->pone)) {
			unget_rec_buffer(1);
			return (0);
		}
		if (remote->ptrail > 0) {
			if (remote->sone > 0 && !expectspace(remote, remote->sone)) {
				unget_rec_buffer(2);
				return (0);
			}
		} else {
			set_pending_space(remote->sone);
		}
	}
	return (1);
}

inline int expectzero(struct ir_remote *remote, int bit)
{
	if (is_biphase(remote)) {
		int all_bits = bit_count(remote);
		ir_code mask;

		mask = ((ir_code) 1) << (all_bits - 1 - bit);
		if (mask & remote->rc6_mask) {
			if (!expectpulse(remote, 2 * remote->pzero)) {
				unget_rec_buffer(1);
				return (0);
			}
			set_pending_space(2 * remote->szero);
		} else {
			if (!expectpulse(remote, remote->pzero)) {
				unget_rec_buffer(1);
				return (0);
			}
			set_pending_space(remote->szero);
		}
	} else if (is_space_first(remote)) {
		if (remote->szero > 0 && !expectspace(remote, remote->szero)) {
			unget_rec_buffer(1);
			return (0);
		}
		if (remote->pzero > 0 && !expectpulse(remote, remote->pzero)) {
			unget_rec_buffer(2);
			return (0);
		}
	} else {
		if (!expectpulse(remote, remote->pzero)) {
			unget_rec_buffer(1);
			return (0);
		}
		if (remote->ptrail > 0) {
			if (!expectspace(remote, remote->szero)) {
				unget_rec_buffer(2);
				return (0);
			}
		} else {
			set_pending_space(remote->szero);
		}
	}
	return (1);
}

inline lirc_t sync_rec_buffer(struct ir_remote * remote)
{
	int count;
	lirc_t deltas, deltap;

	count = 0;
	deltas = get_next_space(1000000);
	if (deltas == 0)
		return (0);

	if (last_remote != NULL && !is_rcmm(remote)) {
		while (!expect_at_least(last_remote, deltas, last_remote->min_remaining_gap)) {
			deltap = get_next_pulse(1000000);
			if (deltap == 0)
				return (0);
			deltas = get_next_space(1000000);
			if (deltas == 0)
				return (0);
			count++;
			if (count > REC_SYNC) {	/* no sync found, 
						   let's try a diffrent remote */
				return (0);
			}
		}
		if (has_toggle_mask(remote)) {
			if (!expect_at_most(last_remote, deltas, last_remote->max_remaining_gap)) {
				remote->toggle_mask_state = 0;
				remote->toggle_code = NULL;
			}

		}
	}
	rec_buffer.sum = 0;
	return (deltas);
}

inline int get_header(struct ir_remote *remote)
{
	if (is_rcmm(remote)) {
		lirc_t deltap, deltas, sum;

		deltap = get_next_pulse(remote->phead);
		if (deltap == 0) {
			unget_rec_buffer(1);
			return (0);
		}
		deltas = get_next_space(remote->shead);
		if (deltas == 0) {
			unget_rec_buffer(2);
			return (0);
		}
		sum = deltap + deltas;
		if (expect(remote, sum, remote->phead + remote->shead)) {
			return (1);
		}
		unget_rec_buffer(2);
		return (0);
	} else if (is_bo(remote)) {
		if (expectpulse(remote, remote->pone) && expectspace(remote, remote->sone)
		    && expectpulse(remote, remote->pone) && expectspace(remote, remote->sone)
		    && expectpulse(remote, remote->phead) && expectspace(remote, remote->shead)) {
			return 1;
		}
		return 0;
	}
	if (remote->shead == 0) {
		if (!sync_pending_space(remote))
			return 0;
		set_pending_pulse(remote->phead);
		return 1;
	}
	if (!expectpulse(remote, remote->phead)) {
		unget_rec_buffer(1);
		return (0);
	}
	/* if this flag is set I need a decision now if this is really
	   a header */
	if (remote->flags & NO_HEAD_REP) {
		lirc_t deltas;

		deltas = get_next_space(remote->shead);
		if (deltas != 0) {
			if (expect(remote, remote->shead, deltas)) {
				return (1);
			}
			unget_rec_buffer(2);
			return (0);
		}
	}

	set_pending_space(remote->shead);
	return (1);
}

inline int get_foot(struct ir_remote *remote)
{
	if (!expectspace(remote, remote->sfoot))
		return (0);
	if (!expectpulse(remote, remote->pfoot))
		return (0);
	return (1);
}

inline int get_lead(struct ir_remote *remote)
{
	if (remote->plead == 0)
		return 1;
	if (!sync_pending_space(remote))
		return 0;
	set_pending_pulse(remote->plead);
	return 1;
}

inline int get_trail(struct ir_remote *remote)
{
	if (remote->ptrail != 0) {
		if (!expectpulse(remote, remote->ptrail))
			return (0);
	}
	if (rec_buffer.pendingp > 0) {
		if (!sync_pending_pulse(remote))
			return (0);
	}
	return (1);
}

inline int get_gap(struct ir_remote *remote, lirc_t gap)
{
	lirc_t data;

	logprintf(2, "sum: %d", rec_buffer.sum);
	data = get_next_rec_buffer(gap - gap * remote->eps / 100);
	if (data == 0)
		return (1);
	if (!is_space(data)) {
		logprintf(2, "space expected");
		return (0);
	}
	unget_rec_buffer(1);
	if (!expect_at_least(remote, data, gap)) {
		logprintf(1, "end of signal not found");
		return (0);
	}
	return (1);
}

inline int get_repeat(struct ir_remote *remote)
{
	if (!get_lead(remote))
		return (0);
	if (is_biphase(remote)) {
		if (!expectspace(remote, remote->srepeat))
			return (0);
		if (!expectpulse(remote, remote->prepeat))
			return (0);
	} else {
		if (!expectpulse(remote, remote->prepeat))
			return (0);
		set_pending_space(remote->srepeat);
	}
	if (!get_trail(remote))
		return (0);
	if (!get_gap
	    (remote,
	     is_const(remote) ? (min_gap(remote) >
				 rec_buffer.sum ? min_gap(remote) -
				 rec_buffer.sum : 0) : (has_repeat_gap(remote) ? remote->repeat_gap : min_gap(remote))
	    ))
		return (0);
	return (1);
}

ir_code get_data(struct ir_remote * remote, int bits, int done)
{
	ir_code code;
	int i;

	code = 0;

	if (is_rcmm(remote)) {
		lirc_t deltap, deltas, sum;

		if (bits % 2 || done % 2) {
			logprintf(LOG_ERR, "invalid bit number.");
			return ((ir_code) - 1);
		}
		if (!sync_pending_space(remote))
			return 0;
		for (i = 0; i < bits; i += 2) {
			code <<= 2;
			deltap = get_next_pulse(remote->pzero + remote->pone + remote->ptwo + remote->pthree);
			deltas = get_next_space(remote->szero + remote->sone + remote->stwo + remote->sthree);
			if (deltap == 0 || deltas == 0) {
				logprintf(LOG_ERR, "failed on bit %d", done + i + 1);
				return ((ir_code) - 1);
			}
			sum = deltap + deltas;
			logprintf(3, "rcmm: sum %ld", (__u32) sum);
			if (expect(remote, sum, remote->pzero + remote->szero)) {
				code |= 0;
				logprintf(2, "00");
			} else if (expect(remote, sum, remote->pone + remote->sone)) {
				code |= 1;
				logprintf(2, "01");
			} else if (expect(remote, sum, remote->ptwo + remote->stwo)) {
				code |= 2;
				logprintf(2, "10");
			} else if (expect(remote, sum, remote->pthree + remote->sthree)) {
				code |= 3;
				logprintf(2, "11");
			} else {
				logprintf(2, "no match for %d+%d=%d", deltap, deltas, sum);
				return ((ir_code) - 1);
			}
		}
		return (code);
	} else if (is_grundig(remote)) {
		lirc_t deltap, deltas, sum;
		int state, laststate;

		if (bits % 2 || done % 2) {
			logprintf(LOG_ERR, "invalid bit number.");
			return ((ir_code) - 1);
		}
		if (!sync_pending_pulse(remote))
			return ((ir_code) - 1);
		for (laststate = state = -1, i = 0; i < bits;) {
			deltas = get_next_space(remote->szero + remote->sone + remote->stwo + remote->sthree);
			deltap = get_next_pulse(remote->pzero + remote->pone + remote->ptwo + remote->pthree);
			if (deltas == 0 || deltap == 0) {
				logprintf(LOG_ERR, "failed on bit %d", done + i + 1);
				return ((ir_code) - 1);
			}
			sum = deltas + deltap;
			logprintf(3, "grundig: sum %ld", (__u32) sum);
			if (expect(remote, sum, remote->szero + remote->pzero)) {
				state = 0;
				logprintf(2, "2T");
			} else if (expect(remote, sum, remote->sone + remote->pone)) {
				state = 1;
				logprintf(2, "3T");
			} else if (expect(remote, sum, remote->stwo + remote->ptwo)) {
				state = 2;
				logprintf(2, "4T");
			} else if (expect(remote, sum, remote->sthree + remote->pthree)) {
				state = 3;
				logprintf(2, "6T");
			} else {
				logprintf(2, "no match for %d+%d=%d", deltas, deltap, sum);
				return ((ir_code) - 1);
			}
			if (state == 3) {	/* 6T */
				i += 2;
				code <<= 2;
				state = -1;
				code |= 0;
			} else if (laststate == 2 && state == 0) {	/* 4T2T */
				i += 2;
				code <<= 2;
				state = -1;
				code |= 1;
			} else if (laststate == 1 && state == 1) {	/* 3T3T */
				i += 2;
				code <<= 2;
				state = -1;
				code |= 2;
			} else if (laststate == 0 && state == 2) {	/* 2T4T */
				i += 2;
				code <<= 2;
				state = -1;
				code |= 3;
			} else if (laststate == -1) {
				/* 1st bit */
			} else {
				logprintf(LOG_ERR, "invalid state %d:%d", laststate, state);
				return ((ir_code) - 1);
			}
			laststate = state;
		}
		return (code);
	}

	for (i = 0; i < bits; i++) {
		code = code << 1;
		if (is_goldstar(remote)) {
			if ((done + i) % 2) {
				logprintf(2, "$1");
				remote->pone = remote->ptwo;
				remote->sone = remote->stwo;
			} else {
				logprintf(2, "$2");
				remote->pone = remote->pthree;
				remote->sone = remote->sthree;
			}
		}

		if (expectone(remote, done + i)) {
			logprintf(2, "1");
			code |= 1;
		} else if (expectzero(remote, done + i)) {
			logprintf(2, "0");
			code |= 0;
		} else {
			logprintf(1, "failed on bit %d", done + i + 1);
			return ((ir_code) - 1);
		}
	}
	return (code);
}

ir_code get_pre(struct ir_remote * remote)
{
	ir_code pre;

	pre = get_data(remote, remote->pre_data_bits, 0);

	if (pre == (ir_code) - 1) {
		logprintf(1, "failed on pre_data");
		return ((ir_code) - 1);
	}
	if (remote->pre_p > 0 && remote->pre_s > 0) {
		if (!expectpulse(remote, remote->pre_p))
			return ((ir_code) - 1);
		set_pending_space(remote->pre_s);
	}
	return (pre);
}

ir_code get_post(struct ir_remote * remote)
{
	ir_code post;

	if (remote->post_p > 0 && remote->post_s > 0) {
		if (!expectpulse(remote, remote->post_p))
			return ((ir_code) - 1);
		set_pending_space(remote->post_s);
	}

	post = get_data(remote, remote->post_data_bits, remote->pre_data_bits + remote->bits);

	if (post == (ir_code) - 1) {
		logprintf(1, "failed on post_data");
		return ((ir_code) - 1);
	}
	return (post);
}

int receive_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		   lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp)
{
	return (1);
}
