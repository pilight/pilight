/*
	Copyright (C) 2019 CurlyMo & S. Seegel

	This file is part of pilight.

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <wiringx.h>
#include "../core/log.h"
#include "raspyrfm.h"
#include "../config/settings.h"

#define FXOSC 32E6
#define FSTEP (FXOSC / (1UL<<19))
#define FREQTOFREG(F) ((uint32_t) (F * 1E6 / FSTEP + .5)) //calculate frequency word
#define BITTIMEUS 31
#define BITRATE ((uint16_t) (FXOSC * BITTIMEUS / 1E6))

#define REG8(adr, val) {adr, val}
#define REG16(adr, val) {adr, ((uint16_t) (val) >> 8) & 0xFF}, {(adr) + 1, (val) & 0xFF}
#define REG24(adr, val) {adr, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 2, (val) & 0xFF}
#define REG32(adr, val) {adr, ((uint32_t) (val) >> 24) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 2, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 3, (val) & 0xFF}

#define TXPWR 13 //-18..13 dBm

enum {
	REGFIFO = 0x00,
	REGOPMODE = 0x01,
	REGDATAMODUL = 0x02,
	REGBITRATE = 0x03,
	REGFDEV = 0x05,
	REGFR = 0x07,
	REGAFCCTRL = 0x0B,
	REGPALEVEL = 0x11,
	REGLNA = 0x18,
	REGRXBW = 0x19,
	REGAFCBW = 0x1A,
	REGOOKPEAK = 0x1B,
	REGOOKAVG = 0x1C,
	REGOOKFIX = 0x1D,
	REGAFCFEI = 0x1E,
	REGAFC = 0x1F,
	REGFEI = 0x21,
	REGDIOMAPPING1 = 0x25,
	REGDIOMAPPING2 = 0x26,
	REGIRQFLAGS1 = 0x27,
	REGIRQFLAGS2 = 0x28,
	REGRSSITHRESH = 0x29,
	REGPREAMBLE = 0x2C,
	REGSYNCCONFIG = 0x2E,
	REGSYNCVALUE1 = 0x2F,
	REGPACKETCONFIG1 = 0x37,
	REGPAYLOADLENGTH = 0x38,
	REGFIFOTHRESH = 0x3C,
	REGPACKETCONFIG2 = 0x3D,
	REGTESTDAGC = 0x6F,
	REGTESTAFC = 0x71
};

enum {
	OPMODE_STDBY = 1,
	OPMODE_TX = 3,
	OPMODE_RX = 4
};

typedef struct {
	uint8_t reg;
	uint8_t val;
} rfmreg_t;

typedef struct {
	uint8_t spi_ch;
	double frequency;
} rfm_settings_t;

static rfm_settings_t rfmsettings = {
	-1, //SPI channel
	0 //frequency
};

static const rfmreg_t rfmcfg[] = {
	REG8(REGOPMODE, OPMODE_STDBY<<2),		//STDBY mode
	REG8(REGDATAMODUL, 0<<5 | 1<<3),		//packet mode, OOK
	REG16(REGBITRATE, BITRATE),
	REG8(REGAFCCTRL, 0x20),					//AfcCtrl: improved routine
	REG8(REGPALEVEL, 1<<7 | (TXPWR + 18)),	//pa power
	REG8(REGLNA, 0x81),						//highest gain
	REG8(REGRXBW, 2<<3),					//bandwidth 167 kHz
	REG8(REGOOKPEAK, 1<<6 | 3),				//OOK mode peak, slowest
	REG8(REGOOKAVG, 0x80),
	REG8(REGOOKFIX, 115),
	REG8(REGAFCFEI, 1<<1),					//AFC 
	REG8(REGDIOMAPPING1, 2<<6|2<<4|1<<2|0),	//DIO0 DIO1 DIO2 DIO3
	REG8(REGDIOMAPPING2, 0x07),
	REG8(REGRSSITHRESH, 0xFF),
	REG8(REGSYNCVALUE1 + 0, 0x00),
	REG8(REGSYNCVALUE1 + 1, 0x00),
	REG8(REGSYNCVALUE1 + 2, 0x00),
	REG8(REGSYNCVALUE1 + 3, 0x00),
	REG8(REGSYNCVALUE1 + 4, 0x00),
	REG8(REGSYNCVALUE1 + 5, 0x00),
	REG8(REGSYNCVALUE1 + 6, 0x00),
	REG8(REGSYNCVALUE1 + 7, 0x01),
	REG8(REGPACKETCONFIG1, 0),				//unlimited length
	REG8(REGPAYLOADLENGTH, 0),				//unlimited length
	REG8(REGFIFOTHRESH, 0<<7 | 63),			//TX start with FIFO level, level 63
	REG8(REGPACKETCONFIG2, 0),
	REG8(REGTESTDAGC, 0x20)
};

static int pollpri = UV_PRIORITIZED;

#if defined(__arm__) || defined(PILIGHT_UNITTEST)
typedef struct data_t {
	int rbuffer[1024];
	int rptr;
} data_t;

static struct data_t data;

static void *reason_received_pulsetrain_free(void *param) {
	struct reason_received_pulsetrain_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_send_code_success_free(void *param) {
	struct reason_send_code_success_free *data = param;
	FREE(data);
	return NULL;
}

static void writeReg(uint8_t reg, uint8_t value) {
	uint8_t tmp[] = {reg | 0x80, value};
	wiringXSPIDataRW(rfmsettings.spi_ch, (unsigned char*) tmp, sizeof(tmp));
}

static uint8_t readReg(uint8_t reg) {
	uint8_t tmp[] = {reg, 0x00};
	wiringXSPIDataRW(rfmsettings.spi_ch, (unsigned char*) tmp, sizeof(tmp));
	return tmp[1];
}

static void waitFlag(uint8_t reg, uint8_t flag, int condition, int wait) {
	while(1) { //wait for data in hardware FIFO
		if(((readReg(reg) & flag) != 0) == (condition != 0))
			break;
		
		if(wait != 0)
			usleep((__useconds_t) wait);
	}
}

static void startReceive() {
	writeReg(REGOPMODE, OPMODE_STDBY<<2);
	waitFlag(REGIRQFLAGS1, 1<<7, 1, 0);
	writeReg(REGSYNCCONFIG, 1<<7 | 7<<3);
	writeReg(REGOPMODE, OPMODE_RX<<2);
	waitFlag(REGIRQFLAGS1, 1<<7, 1, 0);
}

static void poll_cb(uv_poll_t *req, int status, int events) {
	int duration = 1;
	int fd = req->io_watcher.fd;
	int dio0level = 1;
	int maxreclen = 450; 

	if(events & pollpri) {	
		uint8_t c = 0;

		(void)read(fd, &c, 1);
		lseek(fd, 0, SEEK_SET);
		data.rptr = 0;
		while(maxreclen) {
			maxreclen--;
			waitFlag(REGIRQFLAGS2, 1<<6, 1, 150); //wait for data in hardware FIFO
			
			uint8_t buf = readReg(REGFIFO);
			uint8_t mask = 0x80;
			while (mask) {
				if (((buf & mask) != 0) != (dio0level != 0)) {
					if(duration > 0) {
						duration *= BITTIMEUS;
						data.rbuffer[data.rptr++] = duration;
						if(data.rptr > MAXPULSESTREAMLENGTH-1) {
							data.rptr = 0;
						}
						if(duration > raspyrfm->mingaplen) {
							/* Let's do a little filtering here as well */
							if(data.rptr >= raspyrfm->minrawlen && data.rptr <= raspyrfm->maxrawlen) {
								struct reason_received_pulsetrain_t *data1 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
								if(data1 == NULL) {
									OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
								}
								data1->length = data.rptr;
								memcpy(data1->pulses, data.rbuffer, data.rptr*sizeof(int));
								data1->hardware = raspyrfm->id;
								eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data1);
							}
							maxreclen = 0;
							mask = 0;
						}
					}
					
					dio0level = !dio0level;
					duration = 1;
				}
				else {
					duration++;
				}
				mask >>= 1;
			}
		}
		startReceive();
	};
	if(events & UV_DISCONNECT) {
		FREE(req); /*LCOV_EXCL_LINE*/
	}
	return;
}
#endif

static void *raspyrfmSend(int reason, void *data) {
	struct reason_send_code_t *data1 = data;
	int *code = data1->pulses;
	int rawlen = data1->rawlen;
	int repeats = data1->txrpt;
	uint8_t fifobuf = 0;
	int pulsebits = (code[0] + BITTIMEUS / 2) / BITTIMEUS;
	uint8_t bitmask = 0x80;
	uint8_t bitstate = 1;
	int i = 0;
	
	writeReg(REGSYNCCONFIG, 0);
	writeReg(REGOPMODE, OPMODE_TX<<2);
	
	while(pulsebits > 0) {
		if (bitstate) {
			fifobuf |= bitmask;
		}
		
		bitmask >>= 1;
		if (bitmask == 0) {
			bitmask = 0x80;
			waitFlag(REGIRQFLAGS2, 1<<7, 0, 100); //wait for space in hardware FIFO
			writeReg(REGFIFO, fifobuf);
			fifobuf = 0;
		}
		
		pulsebits--;
		if (pulsebits == 0) {
			bitstate = !bitstate;
			i++;
			
			if (i < rawlen) { //still data in source left?
				pulsebits = (code[i] + BITTIMEUS / 2) / BITTIMEUS;
			}
			else
				if (repeats > 0) {
					i = 0;
					pulsebits = (code[0] + BITTIMEUS / 2) / BITTIMEUS;
					repeats--;
				}	
		}
	}
	
	waitFlag(REGIRQFLAGS2, 1<<3, 1, 100); //wait for frame to be sent out completely
	startReceive();
	
	struct reason_code_sent_success_t *data2 = MALLOC(sizeof(struct reason_code_sent_success_t));
	strcpy(data2->message, data1->message);
	strcpy(data2->uuid, data1->uuid);
	eventpool_trigger(REASON_CODE_SEND_SUCCESS, reason_send_code_success_free, data2);
	return NULL;
}

static unsigned short int raspyrfmHwInit(void) {
#if defined(__arm__) || defined(PILIGHT_UNITTEST)
	uv_poll_t *poll_req = NULL;
	char *platform = GPIO_PLATFORM;
	int i = 0;
	uint32_t freqword = 0;
	int gpio_dio0 = 0;
	
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = 80;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched);

	if(config_setting_get_string("gpio-platform", 0, &platform) != 0) {
		logprintf(LOG_ERR, "no gpio-platform configured");
		return EXIT_FAILURE;
	}
	if(strcmp(platform, "none") == 0) {
		FREE(platform);
		logprintf(LOG_ERR, "no gpio-platform configured");
		return EXIT_FAILURE;
	}
	if(wiringXSetup(platform, logprintf1) < 0) {
		FREE(platform);
		return EXIT_FAILURE;
	}
	FREE(platform);

	if(rfmsettings.spi_ch == -1)
		return EXIT_FAILURE;

	if(wiringXSPISetup(rfmsettings.spi_ch, 2500000UL) < 0) {
		FREE(platform);
		return EXIT_FAILURE;
	}
	
	//send config to raspyrfm
	for(i=0; i<sizeof(rfmcfg) / sizeof(rfmcfg[0]); i++)
		writeReg(rfmcfg[i].reg, rfmcfg[i].val);
		
	//set frequency
	if (rfmsettings.frequency == 0)
		return EXIT_FAILURE;

	freqword = FREQTOFREG(rfmsettings.frequency); //MHz
 
	writeReg(REGFR, (freqword >> 16) & 0xFF);
	writeReg(REGFR + 1, (freqword >> 8) & 0xFF);
	writeReg(REGFR + 2, freqword & 0xFF);
	
	startReceive();

#ifdef PILIGHT_UNITTEST
	gpio_dio0 = 1;
#else
	if(rfmsettings.spi_ch == 0)
		gpio_dio0 = 6;
	else
		gpio_dio0 = 5;
#endif
	
	if(wiringXISR(gpio_dio0, ISR_MODE_RISING) < 0) {
		logprintf(LOG_ERR, "unable to register interrupt for pin %d", 6);
		return EXIT_FAILURE;
	}
	if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	int fd = wiringXSelectableFd(gpio_dio0);

	uv_poll_init(uv_default_loop(), poll_req, fd);

#ifdef PILIGHT_UNITTEST
	char *dev = getenv("PILIGHT_RASPYRFM_READ");
	if(dev == NULL) {
		pollpri = UV_PRIORITIZED; /*LCOV_EXCL_LINE*/
	} else {
		pollpri = UV_READABLE;
	}
#endif
	uv_poll_start(poll_req, pollpri, poll_cb);
	
	eventpool_callback(REASON_SEND_CODE, raspyrfmSend);
	return EXIT_SUCCESS;
#else
	logprintf(LOG_ERR, "the raspyrfm module is not supported on this hardware");
	return EXIT_FAILURE;
#endif
}

static unsigned short setFrequency(int freqkHz) {
	rfmsettings.frequency = freqkHz / 1000.0;
	if((rfmsettings.frequency > 800) && (rfmsettings.frequency < 900))
		raspyrfm->hwtype=RF868;
	else if ((rfmsettings.frequency > 430) && (rfmsettings.frequency < 440))
		raspyrfm->hwtype=RF433;
	else {
		raspyrfm->hwtype=NONE;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static unsigned short raspyrfmSettings(struct JsonNode *json) {
	if(strcmp(json->key, "frequency") == 0) {
		if(json->tag == JSON_NUMBER) {
			if (setFrequency((int) json->number_) != EXIT_SUCCESS)
				return EXIT_FAILURE;
		}
		else
			return EXIT_FAILURE;
	}
	
	if(strcmp(json->key, "spi-channel") == 0) {
		if(json->tag == JSON_NUMBER) {
			rfmsettings.spi_ch = (int) json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void raspyrfmInit(void) {
	hardware_register(&raspyrfm);
	hardware_set_id(raspyrfm, "raspyrfm");
	
	options_add(&raspyrfm->options, "f", "frequency", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]+$");
	options_add(&raspyrfm->options, "c", "spi-channel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");

	int frequency = 0;
	if(config_setting_get_number("frequency", 0, &frequency) != 0)
		setFrequency(frequency);
	else
		setFrequency(0);
	
	raspyrfm->minrawlen = 1000;
	raspyrfm->maxrawlen = 0;
	raspyrfm->mingaplen = 5100;
	raspyrfm->maxgaplen = 10000;
	raspyrfm->comtype=COMOOK;
	raspyrfm->hwtype=NONE;
	raspyrfm->init = &raspyrfmHwInit;
	raspyrfm->settings=&raspyrfmSettings;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "raspyrfm";
	module->version = "1.0";
	module->reqversion = "8.0";
	module->reqcommit = NULL;
}

void init(void) {
	raspyrfmInit();
}
#endif
