/*      $Id: hw_default.c,v 5.45 2010/05/07 22:08:16 lirc Exp $      */

/****************************************************************************
 ** hw_default.c ************************************************************
 ****************************************************************************
 *
 * routines for hardware that supports ioctl() interface
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * Copyright (C) 2013 CurlyMo
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

/* disable daemonise if maintainer mode SIM_REC / SIM_SEND defined */
#if defined(SIM_REC) || defined (SIM_SEND)
# undef DAEMONIZE
#endif

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "receive.h"
#include "transmit.h"
#include "hw_default.h"

extern struct ir_remote *repeat_remote;

static __u32 supported_send_modes[] = {
	/* LIRC_CAN_SEND_LIRCCODE, */
	/* LIRC_CAN_SEND_MODE2, this one would be very easy */
	LIRC_CAN_SEND_PULSE,
	/* LIRC_CAN_SEND_RAW, */
	0
};

static __u32 supported_rec_modes[] = {
	LIRC_CAN_REC_LIRCCODE,
	LIRC_CAN_REC_MODE2,
	/* LIRC_CAN_REC_PULSE, shouldn't be too hard */
	/* LIRC_CAN_REC_RAW, */
	0
};

struct hardware hw_default = {
	"/dev/lirc0",	/* default device */
	-1,			/* fd */
	0,			/* features */
	0,			/* send_mode */
	0,			/* rec_mode */
	0,			/* code_length */
	default_init,		/* init_func */
	default_deinit,		/* deinit_func */
	default_send,		/* send_func */
	0,		/* rec_func */
	receive_decode,		/* decode_func */
	default_ioctl,		/* ioctl_func */
	default_readdata,
	"default"
};

/**********************************************************************
 *
 * internal function prototypes
 *
 **********************************************************************/

static int write_send_buffer(int lirc);

/**********************************************************************
 *
 * decode stuff
 *
 **********************************************************************/

int default_readdata(lirc_t timeout)
{
	int data, ret;

	// if (!waitfordata((long)timeout))
		// return 0;

#if defined(SIM_REC) && !defined(DAEMONIZE)
	while (1) {
		__u32 scan;

		ret = fscanf(stdin, "space %u\n", &scan);
		if (ret == 1) {
			data = (int) scan;
			break;
		}

		ret = fscanf(stdin, "pulse %u\n", &scan);
		if (ret == 1) {
			data = (int) scan | PULSE_BIT;
			break;
		}

		ret = fscanf(stdin, "%*s\n");
		if (ret == EOF)
			dosigterm(SIGTERM);
	}
#else
	ret = read(hw.fd, &data, sizeof(data));
	if (ret != sizeof(data)) {
		logprintf(LOG_ERR, "error reading from %s (ret %d, expected %d)",
			  hw.device, ret, sizeof(data));
		logperror(LOG_ERR, NULL);
		default_deinit();

		return 0;
	}

	if (data == 0) {
		static int data_warning = 1;

		if (data_warning) {
			logprintf(LOG_WARNING, "read invalid data from device %s", hw.device);
			data_warning = 0;
		}
		data = 1;
	}
#endif
	return data ;
}

/*
  interface functions
*/

int default_init(void)
{
#if defined(SIM_SEND) && !defined(DAEMONIZE)
	hw.fd = STDOUT_FdILENO;
	hw.features = LIRC_CAN_SEND_PULSE;
	hw.send_mode = LIRC_MODE_PULSE;
	hw.rec_mode = 0;
#elif defined(SIM_REC) && !defined(DAEMONIZE)
	hw.fd = STDIN_FILENO;
	hw.features = LIRC_CAN_REC_MODE2;
	hw.send_mode = 0;
	hw.rec_mode = LIRC_MODE_MODE2;
#else
	struct stat s;
	int i;

	/* FIXME: other modules might need this, too */
	init_rec_buffer();
	init_send_buffer();

	if (stat(hw.device, &s) == -1) {
		logprintf(LOG_ERR, "could not get file information for %s", hw.device);
		logperror(LOG_ERR, "default_init()");
		return (0);
	}

	/* file could be unix socket, fifo and native lirc device */
	if (S_ISSOCK(s.st_mode)) {
		struct sockaddr_un addr;
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, hw.device, sizeof(addr.sun_path));

		hw.fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (hw.fd == -1) {
			logprintf(LOG_ERR, "could not create socket");
			logperror(LOG_ERR, "default_init()");
			return (0);
		}

		if (connect(hw.fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
			logprintf(LOG_ERR, "could not connect to unix socket %s", hw.device);
			logperror(LOG_ERR, "default_init()");
			default_deinit();
			close(hw.fd);
			return (0);
		}

		LOGPRINTF(1, "using unix socket lirc device");
		hw.features = LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE;
		hw.rec_mode = LIRC_MODE_MODE2;	/* this might change in future */
		hw.send_mode = LIRC_MODE_PULSE;
		return (1);
	}

	if ((hw.fd = open(hw.device, O_RDWR)) < 0) {
		logprintf(LOG_ERR, "could not open %s", hw.device);
		logperror(LOG_ERR, "default_init()");
		return (0);
	}
	if (S_ISFIFO(s.st_mode)) {
		LOGPRINTF(1, "using defaults for the Irman");
		hw.features = LIRC_CAN_REC_MODE2;
		hw.rec_mode = LIRC_MODE_MODE2;	/* this might change in future */
		return (1);
	} else if (!S_ISCHR(s.st_mode)) {
		default_deinit();
		logprintf(LOG_ERR, "%s is not a character device!!!", hw.device);
		logperror(LOG_ERR, "something went wrong during installation");
		return (0);
	} else if (default_ioctl(LIRC_GET_FEATURES, &hw.features) == -1) {
		logprintf(LOG_ERR, "could not get hardware features");
		logprintf(LOG_ERR, "this device driver does not support the LIRC ioctl interface");
		if (major(s.st_rdev) == 13) {
			logprintf(LOG_ERR, "did you mean to use the devinput driver instead of the %s driver?",
				  hw.name);
		} else {
			logprintf(LOG_ERR, "major number of %s is %lu", hw.device, (__u32) major(s.st_rdev));
			logprintf(LOG_ERR, "make sure %s is a LIRC device and use a current version of the driver",
				  hw.device);
		}
		default_deinit();
		return (0);
	}
#       ifdef DEBUG
	else {
		if (!(LIRC_CAN_SEND(hw.features) || LIRC_CAN_REC(hw.features))) {
			LOGPRINTF(1, "driver supports neither sending nor receiving of IR signals");
		}
		if (LIRC_CAN_SEND(hw.features) && LIRC_CAN_REC(hw.features)) {
			LOGPRINTF(1, "driver supports both sending and receiving");
		} else if (LIRC_CAN_SEND(hw.features)) {
			LOGPRINTF(1, "driver supports sending");
		} else if (LIRC_CAN_REC(hw.features)) {
			LOGPRINTF(1, "driver supports receiving");
		}
	}
#       endif

	/* set send/receive method */
	hw.send_mode = 0;
	if (LIRC_CAN_SEND(hw.features)) {
		for (i = 0; supported_send_modes[i] != 0; i++) {
			if (LIRC_CAN_SEND(hw.features) == supported_send_modes[i]) {
				hw.send_mode = LIRC_SEND2MODE(supported_send_modes[i]);
				break;
			}
		}
		if (supported_send_modes[i] == 0) {
			logprintf(LOG_NOTICE, "the send method of the driver is not yet supported by lircd");
		}
	}
	hw.rec_mode = 0;
	if (LIRC_CAN_REC(hw.features)) {
		for (i = 0; supported_rec_modes[i] != 0; i++) {
			if (LIRC_CAN_REC(hw.features) == supported_rec_modes[i]) {
				hw.rec_mode = LIRC_REC2MODE(supported_rec_modes[i]);
				break;
			}
		}
		if (supported_rec_modes[i] == 0) {
			logprintf(LOG_NOTICE, "the receive method of the driver is not yet supported by lircd");
		}
	}
	if (hw.rec_mode == LIRC_MODE_MODE2) {
		/* get resolution */
		hw.resolution = 0;
		if ((hw.features & LIRC_CAN_GET_REC_RESOLUTION)
		    && (default_ioctl(LIRC_GET_REC_RESOLUTION, &hw.resolution) != -1)) {
			LOGPRINTF(1, "resolution of receiver: %d", hw.resolution);
		}

	} else if (hw.rec_mode == LIRC_MODE_LIRCCODE) {
		if (default_ioctl(LIRC_GET_LENGTH, &hw.code_length) == -1) {
			logprintf(LOG_ERR, "could not get code length");
			logperror(LOG_ERR, "default_init()");
			default_deinit();
			return (0);
		}
		if (hw.code_length > sizeof(ir_code) * CHAR_BIT) {
			logprintf(LOG_ERR, "lircd can not handle %lu bit codes", hw.code_length);
			default_deinit();
			return (0);
		}
	}
	if (!(hw.send_mode || hw.rec_mode)) {
		default_deinit();
		return (0);
	}
#endif
	return (1);
}

int default_deinit(void)
{
#if (!defined(SIM_SEND) || !defined(SIM_SEND)) || defined(DAEMONIZE)
	if (hw.fd != -1) {
		close(hw.fd);
		hw.fd = -1;
	}
#endif
	return (1);
}

static int write_send_buffer(int lirc)
{
	if (send_buffer.wptr == 0) {
		LOGPRINTF(1, "nothing to send");
		return (0);
	}
	return (write(lirc, send_buffer.data, send_buffer.wptr * sizeof(lirc_t)));
}

int default_send(struct ir_ncode *code)
{
	if (!init_send(code))
		return (0);

	if (write_send_buffer(hw.fd) == -1) {
		logprintf(LOG_ERR, "write failed");
		logperror(LOG_ERR, NULL);
		return (0);
	}
	return (1);
}

int default_ioctl(unsigned int cmd, void *arg)
{
	return ioctl(hw.fd, cmd, arg);
}
