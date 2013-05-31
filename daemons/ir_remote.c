/*      $Id: ir_remote.c,v 5.49 2010/05/13 16:24:29 lirc Exp $      */

/****************************************************************************
 ** ir_remote.c *************************************************************
 ****************************************************************************
 *
 * ir_remote.c - sends and decodes the signals from IR remotes
 * 
 * Copyright (C) 1996,97 Ralph Metzler (rjkm@thp.uni-koeln.de)
 * Copyright (C) 1998 Christoph Bartelmus (lirc@bartelmus.de)
 * Copyright (C) 2013 CurlyMo
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>

#include <sys/ioctl.h>

#include "lirc.h"

#include "lircd.h"
#include "ir_remote.h"
#include "hardware.h"
#include "release.h"

struct ir_remote *decoding = NULL;

struct ir_remote *last_remote = NULL;
struct ir_remote *repeat_remote = NULL;
struct ir_ncode *repeat_code;

extern struct hardware hw;

static inline lirc_t time_left(struct timeval *current, struct timeval *last, lirc_t gap)
{
	unsigned long secs, diff;

	secs = current->tv_sec - last->tv_sec;

	diff = 1000000 * secs + current->tv_usec - last->tv_usec;

	return ((lirc_t) (diff < gap ? gap - diff : 0));
}

void get_frequency_range(struct ir_remote *remotes, unsigned int *min_freq, unsigned int *max_freq)
{
	struct ir_remote *scan;

	/* use remotes carefully, it may be changed on SIGHUP */
	scan = remotes;
	if (scan == NULL) {
		*min_freq = 0;
		*max_freq = 0;
	} else {
		*min_freq = scan->freq;
		*max_freq = scan->freq;
		scan = scan->next;
	}
	while (scan) {
		if (scan->freq != 0) {
			if (scan->freq > *max_freq) {
				*max_freq = scan->freq;
			} else if (scan->freq < *min_freq) {
				*min_freq = scan->freq;
			}
		}
		scan = scan->next;
	}
}

void get_filter_parameters(struct ir_remote *remotes, lirc_t * max_gap_lengthp, lirc_t * min_pulse_lengthp,
			   lirc_t * min_space_lengthp, lirc_t * max_pulse_lengthp, lirc_t * max_space_lengthp)
{
	struct ir_remote *scan = remotes;
	lirc_t max_gap_length = 0;
	lirc_t min_pulse_length = 0, min_space_length = 0;
	lirc_t max_pulse_length = 0, max_space_length = 0;

	while (scan) {
		lirc_t val;
		val = upper_limit(scan, scan->max_gap_length);
		if (val > max_gap_length) {
			max_gap_length = val;
		}
		val = lower_limit(scan, scan->min_pulse_length);
		if (min_pulse_length == 0 || val < min_pulse_length) {
			min_pulse_length = val;
		}
		val = lower_limit(scan, scan->min_space_length);
		if (min_space_length == 0 || val > min_space_length) {
			min_space_length = val;
		}
		val = upper_limit(scan, scan->max_pulse_length);
		if (val > max_pulse_length) {
			max_pulse_length = val;
		}
		val = upper_limit(scan, scan->max_space_length);
		if (val > max_space_length) {
			max_space_length = val;
		}
		scan = scan->next;
	}
	*max_gap_lengthp = max_gap_length;
	*min_pulse_lengthp = min_pulse_length;
	*min_space_lengthp = min_space_length;
	*max_pulse_lengthp = max_pulse_length;
	*max_space_lengthp = max_space_length;
}

struct ir_remote *is_in_remotes(struct ir_remote *remotes, struct ir_remote *remote)
{
	while (remotes != NULL) {
		if (remotes == remote) {
			return remote;
		}
		remotes = remotes->next;
	}
	return NULL;
}

struct ir_remote *get_ir_remote(struct ir_remote *remotes, char *name)
{
	struct ir_remote *all;

	/* use remotes carefully, it may be changed on SIGHUP */
	all = remotes;
	while (all) {
		if (strcasecmp(all->name, name) == 0) {
			return (all);
		}
		all = all->next;
	}
	return (NULL);
}

int map_code(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int pre_bits, ir_code pre,
	     int bits, ir_code code, int post_bits, ir_code post)
{
	ir_code all;

	if (pre_bits + bits + post_bits != remote->pre_data_bits + remote->bits + remote->post_data_bits) {
		return (0);
	}
	all = (pre & gen_mask(pre_bits));
	all <<= bits;
	all |= (code & gen_mask(bits));
	all <<= post_bits;
	all |= (post & gen_mask(post_bits));

	*postp = (all & gen_mask(remote->post_data_bits));
	all >>= remote->post_data_bits;
	*codep = (all & gen_mask(remote->bits));
	all >>= remote->bits;
	*prep = (all & gen_mask(remote->pre_data_bits));

	LOGPRINTF(1, "pre: %llx", (__u64) * prep);
	LOGPRINTF(1, "code: %llx", (__u64) * codep);
	LOGPRINTF(1, "post: %llx", (__u64) * postp);
	LOGPRINTF(1, "code:                   %016llx\n", code);

	return (1);
}

void map_gap(struct ir_remote *remote, struct timeval *start, struct timeval *last, lirc_t signal_length,
	     int *repeat_flagp, lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp)
{
	// Time gap (us) between a keypress on the remote control and
	// the next one.
	lirc_t gap;

	// Check the time gap between the last keypress and this one.
	if (start->tv_sec - last->tv_sec >= 2) {
		// Gap of 2 or more seconds: this is not a repeated keypress.
		*repeat_flagp = 0;
		gap = 0;
	} else {
		// Calculate the time gap in microseconds.
		gap = time_elapsed(last, start);
		if (expect_at_most(remote, gap, remote->max_remaining_gap)) {
			// The gap is shorter than a standard gap
			// (with relative or aboslute tolerance): this
			// is a repeated keypress.
			*repeat_flagp = 1;
		} else {
			// Standard gap: this is a new keypress.
			*repeat_flagp = 0;
		}
	}

	// Calculate extimated time gap remaining for the next code.
	if (is_const(remote)) {
		// The sum (signal_length + gap) is always constant
		// so the gap is shorter when the code is longer.
		if (min_gap(remote) > signal_length) {
			*min_remaining_gapp = min_gap(remote) - signal_length;
			*max_remaining_gapp = max_gap(remote) - signal_length;
		} else {
			*min_remaining_gapp = 0;
			if (max_gap(remote) > signal_length) {
				*max_remaining_gapp = max_gap(remote) - signal_length;
			} else {
				*max_remaining_gapp = 0;
			}
		}
	} else {
		// The gap after the signal is always constant.
		// This is the case of Kanam Accent serial remote.
		*min_remaining_gapp = min_gap(remote);
		*max_remaining_gapp = max_gap(remote);
	}

	LOGPRINTF(1, "repeat_flagp:           %d", *repeat_flagp);
	LOGPRINTF(1, "is_const(remote):       %d", is_const(remote));
	LOGPRINTF(1, "remote->gap range:      %lu %lu", (__u32) min_gap(remote), (__u32) max_gap(remote));
	LOGPRINTF(1, "remote->remaining_gap:  %lu %lu", (__u32) remote->min_remaining_gap,
		  (__u32) remote->max_remaining_gap);
	LOGPRINTF(1, "signal length:          %lu", (__u32) signal_length);
	LOGPRINTF(1, "gap:                    %lu", (__u32) gap);
	LOGPRINTF(1, "extim. remaining_gap:   %lu %lu", (__u32) * min_remaining_gapp, (__u32) * max_remaining_gapp);

}

struct ir_ncode *get_code_by_name(struct ir_remote *remote, char *name)
{

	return (0);
}



int write_message(char *buffer, size_t size, const char *remote_name, const char *button_name,
		  const char *button_suffix, ir_code code, int reps)
{
	int len;

	len = snprintf(buffer, size, "%016llx %02x %s%s %s\n",
		(unsigned long long)code, reps, button_name, button_suffix, remote_name);

	return len;
}

int send_ir_ncode(struct ir_ncode *code)
{
	int ret;
	ret = hw.send_func(code);
	return ret;
}