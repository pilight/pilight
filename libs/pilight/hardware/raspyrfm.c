/*
	Copyright (C) 2017 Stefan Seegel

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <string.h>
#include <stdio.h>
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../../wiringx/wiringX.h"
#include "raspyrfm.h"

#define REG8(adr, val) {adr, val}
#define REG16(adr, val) {adr, ((uint16_t) (val) >> 8) & 0xFF}, {(adr) + 1, (val) & 0xFF}
#define REG24(adr, val) {adr, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 2, (val) & 0xFF}
#define REG32(adr, val) {adr, ((uint32_t) (val) >> 24) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 2, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 3, (val) & 0xFF}

#define TXPWR 13 //-18..13 dBm

#define REGFIFO 0x00
#define REGOPMODE 0x01
#define REGDATAMODUL 0x02
#define REGBITRATE 0x03
#define REGFDEV 0x05
#define REGFR 0x07
#define REGAFCCTRL 0x0B
#define REGPALEVEL 0x11
#define REGLNA 0x18
#define REGRXBW 0x19
#define REGAFCBW 0x1A
#define REGAFCFEI 0x1E
#define REGAFC 0x1F
#define REGFEI 0x21
#define REGDIOMAPPING1 0x25
#define REGIRQFLAGS1 0x27
#define REGIRQFLAGS2 0x28
#define REGRSSITHRESH 0x29
#define REGPREAMBLE 0x2C
#define REGSYNCCONFIG 0x2E
#define REGSYNCVALUE1 0x2F
#define REGPACKETCONFIG1 0x37
#define REGPAYLOADLENGTH 0x38
#define REGFIFOTHRESH 0x3C
#define REGPACKETCONFIG2 0x3D
#define REGTESTDAGC 0x6F
#define REGTESTAFC 0x71

#define MODE_STDBY 1
#define MODE_TX 3

#define IRQFLAGS1_MODEREADY REGIRQFLAGS1 << 8 | 7

#define IRQFLAGS2_FIFOFULL REGIRQFLAGS2 << 8 | 7
#define IRQFLAGS2_FIFONOTEMPTY 6
#define IRQFLAGS2_FIFOLEVEL 5
#define IRQFLAGS2_PACKETSENT REGIRQFLAGS2 << 8 | 3

#define WAITREGBITSET(x) while ((readReg(rfmsettings->spi_ch, (uint8_t) ((x) >> 8)) & (1<<((x) & 0x07))) == 0);
#define WAITREGBITCLEAR(x) while ((readReg(rfmsettings->spi_ch, (uint8_t) ((x) >> 8)) & (1<<((x) & 0x07))) != 0);

typedef struct {
	uint8_t reg;
	uint8_t val;
} rfmreg_t;

static const rfmreg_t rfmcfg[] = {
	REG8(REGOPMODE, MODE_STDBY<<2),             //STDBY mode
	REG8(REGDATAMODUL, 0 << 5 | 1<<3),          //packet mode, OOK
	REG8(REGPALEVEL, 1<<7 | (TXPWR + 18)),      //pa power
	REG8(REGSYNCCONFIG, 0<<7),                  //no sync
	REG8(REGPACKETCONFIG1, 0),                  //unlimited length
	REG8(REGPAYLOADLENGTH, 0),                  //unlimited length
	REG8(REGFIFOTHRESH, 1<<7 | 0x0F),           //TX start with FIFO not empty
	REG8(REGPACKETCONFIG2, 0)
};

static void writeReg(uint8_t spi_ch, uint8_t reg, uint8_t value) {
	uint8_t tmp[] = {reg | 0x80, value};
	wiringXSPIDataRW(spi_ch, (unsigned char*) tmp, sizeof(tmp));
}

static uint8_t readReg(uint8_t spi_ch, uint8_t reg) {
	uint8_t tmp[] = {reg, 0};
	wiringXSPIDataRW(spi_ch, (unsigned char*) tmp, sizeof(tmp));
	return tmp[1];
}

static int gcd(int a, int b) {
	while (b != 0) {
		a %= b;
		a ^= b;
		b ^= a;
		a ^= b;
	}
	return a;
}

static int gcd_a(int n, int a[n]) {
	if(n==1) { 
		return a[0];
	}
	
	if(n==2) {
		return gcd(a[0], a[1]);
	}
	
	int h = n / 2;
	return gcd(gcd_a(h, &a[0]), gcd_a(n - h, &a[h]));
}

unsigned short raspyrfmSettings(JsonNode *json, rfm_settings_t *rfmsettings) {
	if(strcmp(json->key, "spi_ch") == 0) {
		if(json->tag == JSON_NUMBER) {
			rfmsettings->spi_ch = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

unsigned short raspyrfmHwInit(rfm_settings_t *rfmsettings) {
	if(wiringXSupported() == 0) {
		if(wiringXSetup() == -1) {
			return EXIT_FAILURE;
		}

		if (wiringXSPISetup(rfmsettings->spi_ch, 250000) == -1) {
			logprintf(LOG_ERR, "failed to open SPI channel: %d", rfmsettings->spi_ch);
			return EXIT_FAILURE;
		}

		//check if rfm69 present
		int i;
		for (i=0; i<8; i++) {
			writeReg(rfmsettings->spi_ch, REGSYNCVALUE1 + i, 0x55);
			if (readReg(rfmsettings->spi_ch, REGSYNCVALUE1 + i) != 0x55) {
				break;
			}
			writeReg(rfmsettings->spi_ch, REGSYNCVALUE1 + i, 0xAA);
			if (readReg(rfmsettings->spi_ch, REGSYNCVALUE1 + i) != 0xAA) {
				break;
			}
		}

		if (i < 8) {
			logprintf(LOG_ERR, "RaspyRfm module not found!");
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	} else {
		logprintf(LOG_ERR, "the RaspyRfm module is not supported on this hardware", 0);
		return EXIT_FAILURE;
	}
}

int raspyrfmSend(int *code, int rawlen, int repeats, rfm_settings_t *rfmsettings) {
	int i;
	int div;
	int fifoByteCnt = 0;

	div = gcd_a(rawlen, code);

	for (i=0; i<rawlen; i++) {
		code[i] /= div;
		fifoByteCnt += code[i]; //count bits...
	}
	fifoByteCnt *= repeats;
	fifoByteCnt = fifoByteCnt / 8 + 1; //mem we need for bit-FIFO

	unsigned char *fifo = MALLOC(fifoByteCnt);
	memset(fifo, 0x00, fifoByteCnt);

	unsigned char mask = 0x80;
	unsigned char *p = fifo;
	int r = 0;
	for (r=0; r<repeats; r++) {
		for (i=0; i<rawlen; i++) {
			int c;
			for (c=0; c<code[i]; c++) {
				//add high bit if i is even, low bit if i is odd
				if ((i % 2) == 0) {
					*p |= mask;
				}
				mask >>= 1;
				if (mask == 0) {
					mask = 0x80;
					p++;
				}
			}
		}
	}

	//send config to raspyrfm
	for (i=0; i<sizeof(rfmcfg) / sizeof(rfmcfg[0]); i++) {
		writeReg(rfmsettings->spi_ch, rfmcfg[i].reg, rfmcfg[i].val);
	}
		
	//set bitrate
	uint16_t bitrate = (uint16_t) (FXOSC / 1E6) * div; //div = steptime in µS
	writeReg(rfmsettings->spi_ch, REGBITRATE, bitrate >> 8);
	writeReg(rfmsettings->spi_ch, REGBITRATE + 1, bitrate & 0xFF);
	
	//set frequency
	writeReg(rfmsettings->spi_ch, REGFR, (rfmsettings->freq >> 16) & 0xFF);
	writeReg(rfmsettings->spi_ch, REGFR + 1, (rfmsettings->freq >> 8) & 0xFF);
	writeReg(rfmsettings->spi_ch, REGFR + 2, rfmsettings->freq & 0xFF);

	WAITREGBITSET(IRQFLAGS1_MODEREADY);
	while ((readReg(rfmsettings->spi_ch, REGIRQFLAGS2) & (1<<IRQFLAGS2_FIFONOTEMPTY)) != 0) {
		(void) readReg(rfmsettings->spi_ch, REGFIFO);
	}

	writeReg(rfmsettings->spi_ch, REGOPMODE, MODE_TX<<2);
	for (i=0; i<fifoByteCnt; i++) {
		if (i > 60) {
			WAITREGBITCLEAR(IRQFLAGS2_FIFOFULL);
		}
		writeReg(rfmsettings->spi_ch, REGFIFO, fifo[i]);
	}

	WAITREGBITSET(IRQFLAGS2_PACKETSENT);
	
	FREE(fifo);

	return EXIT_SUCCESS;
}