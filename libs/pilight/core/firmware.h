/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _FIRMWARE_H_
#define _FIRMWARE_H_

typedef enum {
	FW_PROG_OP_FAIL = 1,
	FW_INIT_FAIL,
	FW_RD_SIG_FAIL,
	FW_INV_SIG_FAIL,
	FW_MATCH_SIG_FAIL,
	FW_ERASE_FAIL,
	FW_WRITE_FAIL,
	FW_VERIFY_FAIL,
	FW_RD_FUSE_FAIL,
	FW_INV_FUSE_FAIL
} exitrc_t;

typedef enum {
	FW_MP_UNKNOWN,
	FW_MP_ATTINY25,
	FW_MP_ATTINY45,
	FW_MP_ATTINY85,
	FW_MP_ATMEL328P,
	FW_MP_ATMEL32U4
} mptype_t;

int firmware_update(char *fwfile, char *comport);
int firmware_getmp(char *comport);

#endif
