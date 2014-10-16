/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2000-2005  Brian S. Dean <bsd@bsdhome.com>
 * Copyright 2007-2009 Joerg Wunsch <j@uriah.heep.sax.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* $Id: main.c 916 2010-01-15 16:36:13Z joerg_wunsch $ */

/*
 * Code to program an Atmel AVR device through one of the supported
 * programmers.
 *
 * For parallel port connected programmers, the pin definitions can be
 * changed via a config file.  See the config file for instructions on
 * how to add a programmer definition.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "avr.h"
#include "avrgpio.h"
#include "fileio.h"
#include "avrpart.h"
#include "avrconfig.h"
#include "avrupd.h"
#include "safemode.h"
#include "firmware.h"

static int mptype = FW_MP_UNKNOWN;

static void firmware_attiny25(struct avrpart **p) {
	char pgm_bits[] = "1 0 1 0 1 1 0 0 0 1 0 1 0 0 1 1 x x x x x x x x x x x x x x x x";
	char read_bits[] = "0 0 1 1 0 0 0 0 0 0 0 x x x x x x x x x x x a1 a0 o o o o o o o o";
	char erase_bits[] = "1 0 1 0 1 1 0 0 1 0 0 x x x x x x x x x x x x x x x x x x x x x";
	char writepage_bits[] = "0 1 0 0 1 1 0 0 0 0 0 0 0 0 a9 a8 a7 a6 a5 a4 x x x x x x x x x x x x";
	char readlo_bits[] = "0 0 1 0 0 0 0 0 0 0 0 0 0 0 a9 a8 a7 a6 a5 a4 a3 a2 a1 a0 o o o o o o o o";
	char readhigh_bits[] = "0 0 1 0 1 0 0 0 0 0 0 0 0 0 a9 a8 a7 a6 a5 a4 a3 a2 a1 a0 o o o o o o o o";
	char loadpagelo_bits[] = "0 1 0 0 0 0 0 0 0 0 0 x x x x x x x x x a3 a2 a1 a0 i i i i i i i i";
	char loadpagehigh_bits[] = "0 1 0 0 1 0 0 0 0 0 0 x x x x x x x x x a3 a2 a1 a0 i i i i i i i i";
	char lfuseread_bits[] = "0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 x x x x x x x x o o o o o o o o";
	char lfusewrite_bits[] = "1 0 1 0 1 1 0 0 1 0 1 0 0 0 0 0 x x x x x x x x i i i i i i i i";
	char hfuseread_bits[] = "0 1 0 1 1 0 0 0 0 0 0 0 1 0 0 0 x x x x x x x x o o o o o o o o";
	char hfusewrite_bits[] = "1 0 1 0 1 1 0 0 1 0 1 0 1 0 0 0 x x x x x x x x i i i i i i i i";

	*p = avr_new_part();
	strcpy((*p)->id, "t25");
	strcpy((*p)->desc, "ATtiny25");

	(*p)->signature[0] = 0x1e;
	(*p)->signature[1] = 0x91;
	(*p)->signature[2] = 0x08;

	(*p)->op[AVR_OP_PGM_ENABLE] = avr_new_opcode();
	parse_cmdbits((*p)->op[AVR_OP_PGM_ENABLE], pgm_bits);

	(*p)->op[AVR_OP_CHIP_ERASE] = avr_new_opcode();
	parse_cmdbits((*p)->op[AVR_OP_CHIP_ERASE], erase_bits);

	(*p)->sigmem = avr_new_memtype();
	strcpy((*p)->sigmem->desc, "signature");
	(*p)->sigmem->size = 3;
	(*p)->sigmem->buf = malloc((size_t)(*p)->sigmem->size+(size_t)1);
	(*p)->sigmem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->sigmem->op[AVR_OP_READ], read_bits);

	(*p)->flashmem = avr_new_memtype();
	strcpy((*p)->flashmem->desc, "flash");

	(*p)->flashmem->paged = 1;
	(*p)->flashmem->size = 2048;
	(*p)->flashmem->page_size = 32;
	(*p)->flashmem->num_pages = 64;
	(*p)->flashmem->min_write_delay = 4500;
	(*p)->flashmem->max_write_delay = 4500;
	(*p)->flashmem->readback[0] = 0xff;
	(*p)->flashmem->readback[1] = 0xff;

	(*p)->flashmem->op[AVR_OP_WRITEPAGE] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_WRITEPAGE], writepage_bits);

	(*p)->flashmem->op[AVR_OP_READ_LO] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_READ_LO], readlo_bits);

	(*p)->flashmem->op[AVR_OP_READ_HI] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_READ_HI], readhigh_bits);

	(*p)->flashmem->op[AVR_OP_LOADPAGE_LO] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_LOADPAGE_LO], loadpagelo_bits);

	(*p)->flashmem->op[AVR_OP_LOADPAGE_HI] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_LOADPAGE_HI], loadpagehigh_bits);

	(*p)->lfusemem = avr_new_memtype();
	strcpy((*p)->lfusemem->desc, "lfuse");

	(*p)->lfusemem->size = 1;
	(*p)->lfusemem->buf = malloc((size_t)(*p)->lfusemem->size+(size_t)1);

	(*p)->lfusemem->min_write_delay = 9000;
	(*p)->lfusemem->max_write_delay = 9000;

	(*p)->lfusemem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->lfusemem->op[AVR_OP_READ], lfuseread_bits);

	(*p)->lfusemem->op[AVR_OP_WRITE] = avr_new_opcode();
	parse_cmdbits((*p)->lfusemem->op[AVR_OP_WRITE], lfusewrite_bits);

	(*p)->hfusemem = avr_new_memtype();
	strcpy((*p)->hfusemem->desc, "hfuse");

	(*p)->hfusemem->size = 1;
	(*p)->hfusemem->buf = malloc((size_t)(*p)->hfusemem->size+(size_t)1);

	(*p)->hfusemem->min_write_delay = 9000;
	(*p)->hfusemem->max_write_delay = 9000;

	(*p)->hfusemem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->hfusemem->op[AVR_OP_READ], hfuseread_bits);

	(*p)->hfusemem->op[AVR_OP_WRITE] = avr_new_opcode();
	parse_cmdbits((*p)->hfusemem->op[AVR_OP_WRITE], hfusewrite_bits);
}

static void firmware_attiny45(struct avrpart **p) {
	char pgm_bits[] = "1 0 1 0 1 1 0 0 0 1 0 1 0 0 1 1 x x x x x x x x x x x x x x x x";
	char read_bits[] = "0 0 1 1 0 0 0 0 0 0 0 x x x x x x x x x x x a1 a0 o o o o o o o o";
	char erase_bits[] = "1 0 1 0 1 1 0 0 1 0 0 x x x x x x x x x x x x x x x x x x x x x";
	char writepage_bits[] = "0 1 0 0 1 1 0 0 0 0 0 0 0 a10 a9 a8 a7 a6 a5 x x x x x x x x x x x x x";
	char readlo_bits[] = "0 0 1 0 0 0 0 0 0 0 0 0 0 a10 a9 a8 a7 a6 a5 a4 a3 a2 a1 a0 o o o o o o o o";
	char readhigh_bits[] = "0 0 1 0 1 0 0 0 0 0 0 0 0 a10 a9 a8 a7 a6 a5 a4 a3 a2 a1 a0 o o o o o o o o";
	char loadpagelo_bits[] = "0 1 0 0 0 0 0 0 0 0 0 x x x x x x x x a4 a3 a2 a1 a0 i i i i i i i i";
	char loadpagehigh_bits[] = "0 1 0 0 1 0 0 0 0 0 0 x x x x x x x x a4 a3 a2 a1 a0 i i i i i i i i";
	char lfuseread_bits[] = "0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 x x x x x x x x o o o o o o o o";
	char lfusewrite_bits[] = "1 0 1 0 1 1 0 0 1 0 1 0 0 0 0 0 x x x x x x x x i i i i i i i i";
	char hfuseread_bits[] = "0 1 0 1 1 0 0 0 0 0 0 0 1 0 0 0 x x x x x x x x o o o o o o o o";
	char hfusewrite_bits[] = "1 0 1 0 1 1 0 0 1 0 1 0 1 0 0 0 x x x x x x x x i i i i i i i i";

	*p = avr_new_part();
	strcpy((*p)->id, "t45");
	strcpy((*p)->desc, "ATtiny45");

	(*p)->signature[0] = 0x1e;
	(*p)->signature[1] = 0x92;
	(*p)->signature[2] = 0x06;

	(*p)->op[AVR_OP_PGM_ENABLE] = avr_new_opcode();
	parse_cmdbits((*p)->op[AVR_OP_PGM_ENABLE], pgm_bits);

	(*p)->op[AVR_OP_CHIP_ERASE] = avr_new_opcode();
	parse_cmdbits((*p)->op[AVR_OP_CHIP_ERASE], erase_bits);

	(*p)->sigmem = avr_new_memtype();
	strcpy((*p)->sigmem->desc, "signature");
	(*p)->sigmem->size = 3;
	(*p)->sigmem->buf = malloc((size_t)(*p)->sigmem->size+(size_t)1);
	(*p)->sigmem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->sigmem->op[AVR_OP_READ], read_bits);

	(*p)->flashmem = avr_new_memtype();
	strcpy((*p)->flashmem->desc, "flash");

	(*p)->flashmem->paged = 1;
	(*p)->flashmem->size = 4096;
	(*p)->flashmem->page_size = 64;
	(*p)->flashmem->num_pages = 64;
	(*p)->flashmem->min_write_delay = 4500;
	(*p)->flashmem->max_write_delay = 4500;
	(*p)->flashmem->readback[0] = 0xff;
	(*p)->flashmem->readback[1] = 0xff;

	(*p)->flashmem->op[AVR_OP_WRITEPAGE] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_WRITEPAGE], writepage_bits);

	(*p)->flashmem->op[AVR_OP_READ_LO] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_READ_LO], readlo_bits);

	(*p)->flashmem->op[AVR_OP_READ_HI] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_READ_HI], readhigh_bits);

	(*p)->flashmem->op[AVR_OP_LOADPAGE_LO] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_LOADPAGE_LO], loadpagelo_bits);

	(*p)->flashmem->op[AVR_OP_LOADPAGE_HI] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_LOADPAGE_HI], loadpagehigh_bits);

	(*p)->lfusemem = avr_new_memtype();
	strcpy((*p)->lfusemem->desc, "lfuse");

	(*p)->lfusemem->size = 1;
	(*p)->lfusemem->buf = malloc((size_t)(*p)->lfusemem->size+(size_t)1);

	(*p)->lfusemem->min_write_delay = 9000;
	(*p)->lfusemem->max_write_delay = 9000;

	(*p)->lfusemem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->lfusemem->op[AVR_OP_READ], lfuseread_bits);

	(*p)->lfusemem->op[AVR_OP_WRITE] = avr_new_opcode();
	parse_cmdbits((*p)->lfusemem->op[AVR_OP_WRITE], lfusewrite_bits);

	(*p)->hfusemem = avr_new_memtype();
	strcpy((*p)->hfusemem->desc, "hfuse");

	(*p)->hfusemem->size = 1;
	(*p)->hfusemem->buf = malloc((size_t)(*p)->hfusemem->size+(size_t)1);

	(*p)->hfusemem->min_write_delay = 9000;
	(*p)->hfusemem->max_write_delay = 9000;

	(*p)->hfusemem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->hfusemem->op[AVR_OP_READ], hfuseread_bits);

	(*p)->hfusemem->op[AVR_OP_WRITE] = avr_new_opcode();
	parse_cmdbits((*p)->hfusemem->op[AVR_OP_WRITE], hfusewrite_bits);
}

static void firmware_attiny85(struct avrpart **p) {
	char pgm_bits[] = "1 0 1 0 1 1 0 0 0 1 0 1 0 0 1 1 x x x x x x x x x x x x x x x x";
	char read_bits[] = "0 0 1 1 0 0 0 0 0 0 0 x x x x x x x x x x x a1 a0 o o o o o o o o";
	char erase_bits[] = "1 0 1 0 1 1 0 0 1 0 0 x x x x x x x x x x x x x x x x x x x x x";
	char writepage_bits[] = "0 1 0 0 1 1 0 0 0 0 0 0 a11 a10 a9 a8 a7 a6 a5 x x x x x x x x x x x x x";
	char readlo_bits[] = "0 0 1 0 0 0 0 0 0 0 0 0 0 a10 a9 a8 a7 a6 a5 a4 a3 a2 a1 a0 o o o o o o o o";
	char readhigh_bits[] = "0 0 1 0 1 0 0 0 0 0 0 0 0 a10 a9 a8 a7 a6 a5 a4 a3 a2 a1 a0 o o o o o o o o";
	char loadpagelo_bits[] = "0 1 0 0 0 0 0 0 0 0 0 x x x x x x x x a4 a3 a2 a1 a0 i i i i i i i i";
	char loadpagehigh_bits[] = "0 1 0 0 1 0 0 0 0 0 0 x x x x x x x x a4 a3 a2 a1 a0 i i i i i i i i";
	char lfuseread_bits[] = "0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 x x x x x x x x o o o o o o o o";
	char lfusewrite_bits[] = "1 0 1 0 1 1 0 0 1 0 1 0 0 0 0 0 x x x x x x x x i i i i i i i i";
	char hfuseread_bits[] = "0 1 0 1 1 0 0 0 0 0 0 0 1 0 0 0 x x x x x x x x o o o o o o o o";
	char hfusewrite_bits[] = "1 0 1 0 1 1 0 0 1 0 1 0 1 0 0 0 x x x x x x x x i i i i i i i i";

	*p = avr_new_part();
	strcpy((*p)->id, "t85");
	strcpy((*p)->desc, "ATtiny85");

	(*p)->signature[0] = 0x1e;
	(*p)->signature[1] = 0x93;
	(*p)->signature[2] = 0x0b;

	(*p)->op[AVR_OP_PGM_ENABLE] = avr_new_opcode();
	parse_cmdbits((*p)->op[AVR_OP_PGM_ENABLE], pgm_bits);

	(*p)->op[AVR_OP_CHIP_ERASE] = avr_new_opcode();
	parse_cmdbits((*p)->op[AVR_OP_CHIP_ERASE], erase_bits);

	(*p)->sigmem = avr_new_memtype();
	strcpy((*p)->sigmem->desc, "signature");
	(*p)->sigmem->size = 3;
	(*p)->sigmem->buf = malloc((size_t)(*p)->sigmem->size+(size_t)1);
	(*p)->sigmem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->sigmem->op[AVR_OP_READ], read_bits);

	(*p)->flashmem = avr_new_memtype();
	strcpy((*p)->flashmem->desc, "flash");

	(*p)->flashmem->paged = 1;
	(*p)->flashmem->size = 8192;
	(*p)->flashmem->page_size = 64;
	(*p)->flashmem->num_pages = 128;
	(*p)->flashmem->min_write_delay = 4500;
	(*p)->flashmem->max_write_delay = 4500;
	(*p)->flashmem->readback[0] = 0xff;
	(*p)->flashmem->readback[1] = 0xff;

	(*p)->flashmem->op[AVR_OP_WRITEPAGE] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_WRITEPAGE], writepage_bits);

	(*p)->flashmem->op[AVR_OP_READ_LO] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_READ_LO], readlo_bits);

	(*p)->flashmem->op[AVR_OP_READ_HI] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_READ_HI], readhigh_bits);

	(*p)->flashmem->op[AVR_OP_LOADPAGE_LO] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_LOADPAGE_LO], loadpagelo_bits);

	(*p)->flashmem->op[AVR_OP_LOADPAGE_HI] = avr_new_opcode();
	parse_cmdbits((*p)->flashmem->op[AVR_OP_LOADPAGE_HI], loadpagehigh_bits);

	(*p)->lfusemem = avr_new_memtype();
	strcpy((*p)->lfusemem->desc, "lfuse");

	(*p)->lfusemem->size = 1;
	(*p)->lfusemem->buf = malloc((size_t)(*p)->lfusemem->size+(size_t)1);

	(*p)->lfusemem->min_write_delay = 9000;
	(*p)->lfusemem->max_write_delay = 9000;

	(*p)->lfusemem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->lfusemem->op[AVR_OP_READ], lfuseread_bits);

	(*p)->lfusemem->op[AVR_OP_WRITE] = avr_new_opcode();
	parse_cmdbits((*p)->lfusemem->op[AVR_OP_WRITE], lfusewrite_bits);

	(*p)->hfusemem = avr_new_memtype();
	strcpy((*p)->hfusemem->desc, "hfuse");

	(*p)->hfusemem->size = 1;
	(*p)->hfusemem->buf = malloc((size_t)(*p)->hfusemem->size+(size_t)1);

	(*p)->hfusemem->min_write_delay = 9000;
	(*p)->hfusemem->max_write_delay = 9000;

	(*p)->hfusemem->op[AVR_OP_READ] = avr_new_opcode();
	parse_cmdbits((*p)->hfusemem->op[AVR_OP_READ], hfuseread_bits);

	(*p)->hfusemem->op[AVR_OP_WRITE] = avr_new_opcode();
	parse_cmdbits((*p)->hfusemem->op[AVR_OP_WRITE], hfusewrite_bits);
}

static void firmware_init_pgm(PROGRAMMER **pgm) {
	*pgm = pgm_new();
	gpio_initpgm(*pgm);
	(*pgm)->pinno[3] = 8;
	(*pgm)->pinno[4] = 11;
	(*pgm)->pinno[5] = 10;
	(*pgm)->pinno[6] = 9;

	if((*pgm)->setup) {
		(*pgm)->setup(*pgm);
	}
}

static void firmware_process(int percent, double etime, char *hdr) {
  //printf("%d\n", percent);
}

static int firmware_identifymp(struct avrpart **p) {
	int exitrc = 0, i = 0;
	int init_ok = 0;
	PROGRAMMER *pgm = NULL;
	AVRMEM *sig = NULL;

	avr_set_update_progress(firmware_process);

	firmware_init_pgm(&pgm);

	/*
	 * set up seperate instances of the avr part, one for use in
	 * programming, one for use in verifying.  These are separate
	 * because they need separate flash and eeprom buffer space
	 */

	if(pgm->open(pgm, NULL) < 0) {
		exitrc = FW_PROG_OP_FAIL;
		pgm->ppidata = 0; /* clear all bits at exit */
		goto main_exit;
	}

	exitrc = 0;

	/*
	 * enable the programmer
	 */
	pgm->enable(pgm);

	/*
	 * initialize the chip in preperation for accepting commands
	 */
	init_ok = (pgm->initialize(pgm, *p) >= 0);

	if(!init_ok) {
		exitrc = FW_INIT_FAIL;
		goto main_exit;
	}

	logprintf(LOG_INFO, "AVR device initialized and ready to accept instructions");

  /*
   * Let's read the signature bytes to make sure there is at least a
   * chip on the other end that is responding correctly.  A check
   * against 0xffffff / 0x000000 should ensure that the signature bytes
   * are valid.
   */
	if(init_ok) {
		if(avr_signature(pgm, *p) != 0) {
			exitrc = FW_RD_SIG_FAIL;
			goto main_exit;
		}
	}

	sig = (*p)->sigmem;

	if(sig != NULL) {
		int ff = 1, zz = 1;

		for(i=0; i<sig->size; i++) {
			if(sig->buf[i] != 0xff)
				ff = 0;
			if(sig->buf[i] != 0x00)
				zz = 0;
		}

		if(ff == 1 || zz == 1) {
			exitrc = FW_INV_SIG_FAIL;
			goto main_exit;
		}
	}

	logprintf(LOG_INFO, "AVR device signature = 0x%02x 0x%02x 0x%02x", (*p)->signature[0], (*p)->signature[1], (*p)->signature[2]);

	if(sig->size != 3 ||
		sig->buf[0] != (*p)->signature[0] ||
		sig->buf[1] != (*p)->signature[1] ||
		sig->buf[2] != (*p)->signature[2]) {
		exitrc = FW_MATCH_SIG_FAIL;
		goto main_exit;
	}

main_exit:
	pgm->close(pgm);

	sfree((void *)&pgm);

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->op[i]);
	}

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->sigmem->op[i]);
	}
	sfree((void *)&(*p)->sigmem->buf);
	sfree((void *)&(*p)->sigmem);

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->flashmem->op[i]);
	}
	sfree((void *)&(*p)->flashmem->buf);
	sfree((void *)&(*p)->flashmem);

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->hfusemem->op[i]);
	}
	sfree((void *)&(*p)->hfusemem->buf);
	sfree((void *)&(*p)->hfusemem);

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->lfusemem->op[i]);
	}
	sfree((void *)&(*p)->lfusemem->buf);
	sfree((void *)&(*p)->lfusemem);

	sfree((void *)&*p);

	return exitrc;
}

static int firmware_write(char *filename, struct avrpart **p) {
	int exitrc = 0, i = 0, erase = 1, nowrite = 0, verify = 1;
	int safemode = 1, init_ok = 0;
	unsigned char safemode_lfuse = 0xff, safemode_hfuse = 0xff;
	unsigned char safemode_efuse = 0xff, safemode_fuse = 0xff;
	int fuses_specified = 0, fuses_AVRUPDd = 0;
	struct avrpart *v = NULL;
	AVRMEM *sig = NULL;
	AVRUPD *wfile = NULL;
	AVRUPD *vfile = NULL;
	AVRUPD *wlfuse = NULL;
	AVRUPD *vlfuse = NULL;
	AVRUPD *whfuse = NULL;
	AVRUPD *vhfuse = NULL;
	PROGRAMMER *pgm = NULL;
	char lfuse[] = "lfuse:w:0xe1:m";
	char hfuse[] = "lfuse:w:0xe1:m";

	avr_set_update_progress(firmware_process);

	wfile = parse_op(filename);
	if(verify && wfile->op == DEVICE_WRITE) {
		vfile = dup_AVRUPD(wfile);
		vfile->op = DEVICE_VERIFY;
	}

	wlfuse = parse_op(lfuse);
	if(verify && wlfuse->op == DEVICE_WRITE) {
		vlfuse = dup_AVRUPD(wlfuse);
		vlfuse->op = DEVICE_VERIFY;
	}

	whfuse = parse_op(hfuse);
	if(verify && whfuse->op == DEVICE_WRITE) {
		vhfuse = dup_AVRUPD(whfuse);
		vhfuse->op = DEVICE_VERIFY;
	}

	firmware_init_pgm(&pgm);

	/*
	 * set up seperate instances of the avr part, one for use in
	 * programming, one for use in verifying.  These are separate
	 * because they need separate flash and eeprom buffer space
	 */
	v = avr_dup_part(*p);

	if(pgm->open(pgm, NULL) < 0) {
		exitrc = FW_INIT_FAIL;
		pgm->ppidata = 0; /* clear all bits at exit */
		goto main_exit;
	}

	exitrc = 0;

	/*
	 * enable the programmer
	 */
	pgm->enable(pgm);

	/*
	 * initialize the chip in preperation for accepting commands
	 */
	init_ok = (pgm->initialize(pgm, *p) >= 0);

	if(!init_ok) {
		exitrc = FW_INIT_FAIL;
		goto main_exit;
	}

	logprintf(LOG_INFO, "AVR device initialized and ready to accept instructions");

  /*
   * Let's read the signature bytes to make sure there is at least a
   * chip on the other end that is responding correctly.  A check
   * against 0xffffff / 0x000000 should ensure that the signature bytes
   * are valid.
   */
	if(init_ok) {
		if(avr_signature(pgm, *p) != 0) {
			exitrc = FW_RD_SIG_FAIL;
			goto main_exit;
		}
	}

	sig = (*p)->sigmem;

	if(sig != NULL) {
		int ff = 1, zz = 1;

		for(i=0; i<sig->size; i++) {
			if(sig->buf[i] != 0xff)
				ff = 0;
			if(sig->buf[i] != 0x00)
				zz = 0;
		}

		if(ff == 1 || zz == 1) {
			exitrc = FW_INV_SIG_FAIL;
			goto main_exit;
		}
	}

	logprintf(LOG_INFO, "AVR device signature = 0x%02x 0x%02x 0x%02x", (*p)->signature[0], (*p)->signature[1], (*p)->signature[2]);

	if(sig->size != 3 ||
		sig->buf[0] != (*p)->signature[0] ||
		sig->buf[1] != (*p)->signature[1] ||
		sig->buf[2] != (*p)->signature[2]) {
		exitrc = FW_MATCH_SIG_FAIL;
		goto main_exit;
	}

	if(sig->size != 3 ||
		sig->buf[0] != (*p)->signature[0] ||
		sig->buf[1] != (*p)->signature[1] ||
		sig->buf[2] != (*p)->signature[2]) {
		exitrc = FW_MATCH_SIG_FAIL;
		goto main_exit;
	}

	if(init_ok && safemode == 1) {
		/* If safemode is enabled, go ahead and read the current low, high,
		and extended fuse bytes as needed */
		int rc = safemode_readfuses(&safemode_lfuse, &safemode_hfuse,
								&safemode_efuse, &safemode_fuse, pgm, *p);

		if(rc != 0) {
			if(rc == -5) {
				logprintf(LOG_ERR, "AVR fuse reading not support by programmer. Safemode disabled.");
				safemode = 0;
			} else {
				logprintf(LOG_ERR, "To protect your AVR the programming will be aborted");
				exitrc = 1;
				goto main_exit;
			}
		} else {
			safemode_memfuses(1, &safemode_lfuse, &safemode_hfuse, &safemode_efuse, &safemode_fuse);
		}
	}

	logprintf(LOG_INFO, "AVR FLASH memory has been specified, an erase cycle will be performed");

	if(init_ok && erase) {
		logprintf(LOG_INFO, "AVR chip being erased");

		if(avr_chip_erase(pgm, *p)) {
			exitrc = FW_ERASE_FAIL;
			goto main_exit;
		}
	}

	/* Write high and low fuses to microprocessor
	   First low and then high */

	if(do_op(pgm, v, wlfuse, nowrite, verify)) {
		exitrc = FW_WRITE_FAIL;
		goto main_exit;
	}
	if(do_op(pgm, v, vlfuse, nowrite, verify)) {
		exitrc = FW_VERIFY_FAIL;
		goto main_exit;
	}

	if(do_op(pgm, v, whfuse, nowrite, verify)) {
		exitrc = FW_WRITE_FAIL;
		goto main_exit;
	}
	if(do_op(pgm, v, vhfuse, nowrite, verify)) {
		exitrc = FW_VERIFY_FAIL;
		goto main_exit;
	}

	/* Write hex file to microprocessor */
	if(do_op(pgm, v, wfile, nowrite, verify)) {
		exitrc = FW_WRITE_FAIL;
		goto main_exit;
	}
	if(do_op(pgm, v, vfile, nowrite, verify)) {
		exitrc = FW_VERIFY_FAIL;
		goto main_exit;
	}

	/* Right before we exit programming mode, which will make the fuse
	   bits active, check to make sure they are still correct */
	if(safemode == 1) {
		/* If safemode is enabled, go ahead and read the current low,
		* high, and extended fuse bytes as needed */
		unsigned char safemodeafter_lfuse = 0xff;
		unsigned char safemodeafter_hfuse = 0xff;
		unsigned char safemodeafter_efuse = 0xff;
		unsigned char safemodeafter_fuse  = 0xff;
		unsigned char failures = 0;

		//Restore the default fuse values
		safemode_memfuses(0, &safemode_lfuse, &safemode_hfuse, &safemode_efuse, &safemode_fuse);

		/* Try reading back fuses, make sure they are reliable to read back */
		if(safemode_readfuses(&safemodeafter_lfuse, &safemodeafter_hfuse,
			&safemodeafter_efuse, &safemodeafter_fuse, pgm, *p) != 0) {
			/* Uh-oh.. try once more to read back fuses */
			if(safemode_readfuses(&safemodeafter_lfuse, &safemodeafter_hfuse,
			   &safemodeafter_efuse, &safemodeafter_fuse, pgm, *p) != 0) {
				logprintf(LOG_ERR, "Sorry, reading back AVR fuses was unreliable. Giving Up.");
				exitrc = FW_RD_FUSE_FAIL;
				goto main_exit;
			}
		}

		/* Now check what fuses are against what they should be */
		if(safemodeafter_fuse != safemode_fuse) {
			fuses_AVRUPDd = 1;
			logprintf(LOG_ERR, "AVR fuse changed! Was %x, and is now %x", safemode_fuse, safemodeafter_fuse);

			/* Ask user - should we change them */
			/* Enough chit-chat, time to program some fuses and check them */
			if(safemode_writefuse(safemode_fuse, "fuse", pgm, *p, 10) == 0) {
				logprintf(LOG_ERR, "AVR fuse is now rescued");
			} else {
				logprintf(LOG_ERR, "AVR fuse could NOT be changed");
				failures++;
			}
		}

		/* Now check what fuses are against what they should be */
		if(safemodeafter_lfuse != safemode_lfuse) {
			fuses_AVRUPDd = 1;
			logprintf(LOG_ERR, "AVR lfuse changed! Was %x, and is now %x", safemode_lfuse, safemodeafter_lfuse);

			/* Ask user - should we change them */
			/* Enough chit-chat, time to program some fuses and check them */
			if(safemode_writefuse(safemode_lfuse, "lfuse", pgm, *p, 10) == 0) {
				logprintf(LOG_ERR, "AVR lfuse is now rescued");
			} else {
				logprintf(LOG_ERR, "AVR lfuse could NOT be changed");
				failures++;
			}
		}

		/* Now check what fuses are against what they should be */
		if(safemodeafter_hfuse != safemode_hfuse) {
			fuses_AVRUPDd = 1;
			logprintf(LOG_ERR, "AVR hfuse changed! Was %x, and is now %x", safemode_hfuse, safemodeafter_hfuse);

			/* Ask user - should we change them */
			/* Enough chit-chat, time to program some fuses and check them */
			if(safemode_writefuse(safemode_hfuse, "hfuse", pgm, *p, 10) == 0) {
				logprintf(LOG_ERR, "AVR hfuse is now rescued");
			} else {
				logprintf(LOG_ERR, "AVR hfuse could NOT be changed");
				failures++;
			}
		}

		/* Now check what fuses are against what they should be */
		if(safemodeafter_efuse != safemode_efuse) {
			fuses_AVRUPDd = 1;
			logprintf(LOG_ERR, "AVR efuse changed! Was %x, and is now %x", safemode_efuse, safemodeafter_efuse);

			/* Ask user - should we change them */
			/* Enough chit-chat, time to program some fuses and check them */
			if(safemode_writefuse(safemode_efuse, "efuse", pgm, *p, 10) == 0) {
				logprintf(LOG_ERR, "AVR efuse is now rescued");
			} else {
				logprintf(LOG_ERR, "AVR efuse could NOT be changed");
				failures++;
			}
		}

		if(failures == 0) {
			logprintf(LOG_INFO, "AVR fuses OK");
		} else {
			logprintf(LOG_INFO, "AVR fuses not recovered");
		}

		if(fuses_AVRUPDd && fuses_specified) {
			exitrc = 1;
		}
	}

main_exit:
	pgm->close(pgm);

	sfree((void *)&pgm);

	sfree((void *)&wfile->memtype);
	sfree((void *)&wfile->filename);
	sfree((void *)&wfile);

	if(vfile) {
		sfree((void *)&vfile->memtype);
		sfree((void *)&vfile->filename);
		sfree((void *)&vfile);
	}

	sfree((void *)&whfuse->memtype);
	sfree((void *)&whfuse->filename);
	sfree((void *)&whfuse);

	if(vhfuse) {
		sfree((void *)&vhfuse->memtype);
		sfree((void *)&vhfuse->filename);
		sfree((void *)&vhfuse);
	}

	sfree((void *)&wlfuse->memtype);
	sfree((void *)&wlfuse->filename);
	sfree((void *)&wlfuse);

	if(vlfuse) {
		sfree((void *)&vlfuse->memtype);
		sfree((void *)&vlfuse->filename);
		sfree((void *)&vlfuse);
	}

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->op[i]);
	}

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->sigmem->op[i]);
	}
	sfree((void *)&(*p)->sigmem->buf);
	sfree((void *)&(*p)->sigmem);

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->flashmem->op[i]);
	}
	sfree((void *)&(*p)->flashmem->buf);
	sfree((void *)&(*p)->flashmem);

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->hfusemem->op[i]);
	}
	sfree((void *)&(*p)->hfusemem->buf);
	sfree((void *)&(*p)->hfusemem);

	for(i=0;i<AVR_OP_MAX;i++) {
		sfree((void *)&(*p)->lfusemem->op[i]);
	}
	sfree((void *)&(*p)->lfusemem->buf);
	sfree((void *)&(*p)->lfusemem);

	sfree((void *)&v->flashmem->buf);
	sfree((void *)&v->flashmem);
	sfree((void *)&v->sigmem->buf);
	sfree((void *)&v->sigmem);
	sfree((void *)&v->lfusemem->buf);
	sfree((void *)&v->lfusemem);
	sfree((void *)&v->hfusemem->buf);
	sfree((void *)&v->hfusemem);

	sfree((void *)&*p);
	sfree((void *)&v);

	logprintf(LOG_INFO, "Finished updating firmware");

	return exitrc;
}

int firmware_getmp(void) {
	struct avrpart *p = NULL;
	unsigned int match = 0;
	firmware_attiny25(&p);
	if(!match && firmware_identifymp(&p) != 0) {
		mptype = FW_MP_ATTINY45;
		match = 0;
		firmware_attiny45(&p);
	} else {
		return mptype;
	}
	if(!match && firmware_identifymp(&p) != 0) {
		mptype = FW_MP_ATTINY85;
		match = 0;
		firmware_attiny85(&p);
	} else {
		return mptype;
	}
	if(!match && firmware_identifymp(&p) != 0) {
		mptype = FW_MP_UNKNOWN;
		logprintf(LOG_ERR, "AVR unknown");
		return -1;
	} else {
		return mptype;
	}
	return -1;
}

int firmware_check(char **output) {
	int version = 0;
#ifdef FIRMWARE_UPDATER
	struct dirent *file = NULL;
	DIR *d = NULL;
	int tmp = 0;
	int num = 0;

	if(mptype == FW_MP_UNKNOWN) {
		logprintf(LOG_INFO, "Discovering AVR the firmware is running on");
		firmware_getmp();
	}

	if(mptype != FW_MP_UNKNOWN) {
		if((d = opendir(FIRMWARE_PATH))) {
			while((file = readdir(d)) != NULL) {
				if(file->d_type == DT_REG) {
					switch(mptype) {
						case FW_MP_ATTINY25:
							num = 2;
						break;
						case FW_MP_ATTINY45:
							num = 4;
						break;
						case FW_MP_ATTINY85:
							num = 8;
						break;
						default:
						break;
					}
					char name[strlen(file->d_name)+2];
					memset(name, '\0', strlen(file->d_name)+2);
					sprintf(name, "pilight_firmware_t%d5", num);
					if(strncmp(file->d_name, name, 19) == 0) {
						sscanf(file->d_name, "pilight_firmware_t%*[0-9]_v%d.hex", &tmp);
						if(tmp > version) {
							version = tmp;
							strcpy(*output, file->d_name);
						}
					}
				}
			}
			closedir(d);
		}
	}
#endif
	if(firmware.version >= version) {
		return -1;
	} else {
		return 0;
	}
}

int firmware_update(char *fwfile) {
	struct avrpart *p = NULL;
	if(fmt_autodetect(fwfile) != FMT_IHEX) {
		logprintf(LOG_ERR, "Trying to write an invalid firmware file");
		return -1;
	} else {
		switch(mptype) {
			case FW_MP_ATTINY25:
				logprintf(LOG_INFO, "Firmware running on an ATTiny25");
				firmware_attiny25(&p);
			break;
			case FW_MP_ATTINY45:
				logprintf(LOG_INFO, "Firmware running on an ATTiny45");
				firmware_attiny45(&p);
			break;
			case FW_MP_ATTINY85:
				logprintf(LOG_INFO, "Firmware running on an ATTiny85");
				firmware_attiny85(&p);
			break;
			default:
				logprintf(LOG_INFO, "First run firmware_getmp");
				return -1;
			break;
		}

		int ret = firmware_write(fwfile, &p);
		mptype = FW_MP_UNKNOWN;
		return ret;
	}
}
