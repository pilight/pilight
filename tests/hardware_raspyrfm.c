/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wiringx.h>
#include <sys/ioctl.h>
#ifndef __FreeBSD__
	#include <linux/spi/spidev.h>
#endif

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/hardware/hardware.h"
#include "../libs/pilight/hardware/raspyrfm.h"

#include "alltests.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

static uv_thread_t pth;
static CuTest *gtc = NULL;
static uv_pipe_t *pipe_req = NULL;
static uv_timer_t *timer_req = NULL;
static int check = 0;
static uv_pipe_t *client_req = NULL;

static uint8_t rxtestraw[] = {
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF
};

static int rxtestpulses[] = {
	279, 248, 248, 248, 248, 248, 248, 248,
	248, 248, 248, 248, 248, 248, 248, 248,
	248, 248, 248, 248, 248, 248, 248, 248,
	248, 248, 248, 248, 248, 248, 248, 5952
};

static int txtestpulses[] = {
	248, 248, 248, 248, 248, 248, 248, 248,
	248, 248, 248, 248, 248, 248, 248, 248,
	248, 248, 248, 248, 248, 248, 248, 248,
	248, 248, 248, 248, 248, 248, 248, 5952
};

static uint8_t txtestraw[] = {
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

typedef struct {
	uint8_t reg;
	uint8_t val;
} rfmreg_t;

enum {
	REGFIFO = 0x00,	 REGOPMODE = 0x01, REGDATAMODUL = 0x02, REGBITRATE = 0x03,
	REGFDEV = 0x05, REGFR = 0x07, REGAFCCTRL = 0x0B, REGPALEVEL = 0x11,
	REGLNA = 0x18, REGRXBW = 0x19, REGAFCBW = 0x1A, REGOOKPEAK = 0x1B,
	REGOOKAVG = 0x1C, REGOOKFIX = 0x1D, REGAFCFEI = 0x1E, REGAFC = 0x1F,
	REGFEI = 0x21, REGDIOMAPPING1 = 0x25, REGDIOMAPPING2 = 0x26, REGIRQFLAGS1 = 0x27,
	REGIRQFLAGS2 = 0x28, REGRSSITHRESH = 0x29, REGPREAMBLE = 0x2C, REGSYNCCONFIG = 0x2E,
	REGSYNCVALUE1 = 0x2F, REGPACKETCONFIG1 = 0x37, REGPAYLOADLENGTH = 0x38, REGFIFOTHRESH = 0x3C,
	REGPACKETCONFIG2 = 0x3D, REGTESTDAGC = 0x6F, REGTESTAFC = 0x71
};

#define FXOSC 32E6
#define FSTEP (FXOSC / (1UL<<19))
#define FREQTOFREG(F) ((uint32_t) (F * 1E6 / FSTEP + .5)) //calculate frequency word
#define BITTIMEUS 31
#define BITRATE ((uint16_t) (FXOSC * BITTIMEUS / 1E6))
#define TXPWR 13 //-18..13 dBm

#define REG8(adr, val) {adr, val}
#define REG16(adr, val) {adr, ((uint16_t) (val) >> 8) & 0xFF}, {(adr) + 1, (val) & 0xFF}
#define REG24(adr, val) {adr, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 2, (val) & 0xFF}
#define REG32(adr, val) {adr, ((uint32_t) (val) >> 24) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 2, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 3, (val) & 0xFF}

static const rfmreg_t rfmcfg[] = {
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

static rfmreg_t rfmcfgcheck[ARRAY_LEN(rfmcfg)]; 

static int itestraw = 0;

void wiringXIOCTLCallback(int (*callback)(int fd, unsigned long req, void *cmd));

static void foo(int prio, char *file, int line, const char *format_str, ...) {
}

static void *reason_send_code_free(void *param) {
	struct reason_send_code_t *data = param;
	FREE(data);
	return NULL;
}

int ioctl_callback(int fd, unsigned long req, void *cmd) {
	if (req == SPI_IOC_MESSAGE(1)) {
		struct spi_ioc_transfer *tr = cmd;

		uint8_t *rxbuf = (void*) (long) tr->rx_buf;
		uint8_t *txbuf = (void*) (long) tr->tx_buf;
		if ((rxbuf[0] & 0x80) == 0) { //register read
			switch (rxbuf[0]) {
			case 0x00: //FIFO
				txbuf[1] = rxtestraw[itestraw++];
				if (itestraw >= ARRAY_LEN(rxtestraw))
					itestraw = 0;
				break;
			case 0x27: //RegIrqFlags1
				txbuf[1] = 0x80; //mode ready
				break;
			case 0x28: //RegIrqFlags2
				txbuf[1] = 1<<6 | 1<<3; //FIFO not empty & Packet sent 
				break;
			break;
			default:
				break;
			}
		}
		else { //register write
			uint8_t reg = (rxbuf[0] & 0x7F);
			uint8_t val = rxbuf[1]; 
			if (reg == 0x00) {
				//write FIFO register
				if (val != txtestraw[itestraw++]) {
					itestraw = 0;
					check = 0;
				}
				if (itestraw == ARRAY_LEN(txtestraw)) {
					check = 1;
				}	
			}
			else {
				int i;
				for (i=0; i<ARRAY_LEN(rfmcfgcheck); i++)
					if (rfmcfgcheck[i].reg == reg) {
						if (val == rfmcfg[i].val) { 
							rfmcfgcheck[i].val = 1;
						}
						break;
					}
			}	
		}
	};

	return 0;
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

void open_cb(uv_fs_t *req) {
	wiringXIOCTLCallback(ioctl_callback);
}

static void stop(uv_timer_t *req) {
	uv_stop(uv_default_loop());
}

static void initthread(void *arg) {
	char *settings = "{\"raspyrfm\":{\"spi-channel\":0,\"frequency\":433920}}";
	struct JsonNode *jsettings = json_decode(settings);
	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "raspyrfm", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jspichannel = json_first_child(jchild);
				struct JsonNode *jfrequency = jspichannel->next;
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jspichannel));
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jfrequency));
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->init());
			}
		}
	}
	json_delete(jsettings);
}

static void accept_gpio(uv_stream_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);

	client_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, client_req);

	int r = 0;
	
	r = uv_pipe_init(uv_default_loop(), client_req, 0);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_accept(req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);
}

static void connect_receive_cb(uv_stream_t *req, int status) {
	accept_gpio(req, status);

	//trigger IRQ
	uv_os_fd_t fd = 0;
	uv_fileno((uv_handle_t *)client_req, &fd);
	send(fd, "a", 1, 0);
}

static void *receivePulseTrain(int reason, void *param, void *userdata) {
	struct reason_received_pulsetrain_t *data = param;
	if(data->hardware != NULL && data->pulses != NULL && data->length == ARRAY_LEN(rxtestpulses)) {
		if (memcmp(data->pulses, rxtestpulses, sizeof(rxtestpulses)) == 0)
			check = 1;
		else
			check = 0;
	}
	return (void *)NULL;
}

static void connect_send_cb(uv_stream_t *req, int status) {
	accept_gpio(req, status);

	struct reason_send_code_t *data = MALLOC(sizeof(struct reason_send_code_t));
	CuAssertPtrNotNull(gtc, data);
	data->origin = 1;
	data->rawlen = ARRAY_LEN(txtestpulses);
	memcpy(data->pulses, txtestpulses, data->rawlen*sizeof(int));
	data->txrpt = 0;
	strncpy(data->protocol, "dummy", 255);
	data->hwtype = 1;
	memset(data->uuid, 0, UUID_LENGTH+1);
	eventpool_trigger(REASON_SEND_CODE, reason_send_code_free, data);
}

static int start(const char *func, uv_connection_cb cb) {
	int i;

	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return -1;
	}
	printf("[ %-48s ]\n", func);
	fflush(stdout);

	memtrack();

	for(i=0; i<ARRAY_LEN(rfmcfgcheck); i++) {
		rfmcfgcheck[i].reg = rfmcfg[i].reg;
		rfmcfgcheck[i].val = 0;
	} 
	
	//create SPI dummy
	uv_fs_t *req = malloc(sizeof(uv_fs_t));
	uv_fs_open(uv_default_loop(), req, "/dev/spidev0.0", O_CREAT, O_RDWR, open_cb);

	//create dummy config
	FILE *f = fopen("hardware_raspyrfm.json", "w");
	fprintf(f,
		"{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"gpio-platform\":\"gpio-stub\"},"\
		"\"hardware\":{},\"registry\":{}}"
	);
	fclose(f);

	//create GPIO dummy
	int r = 0;

	pipe_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, pipe_req);

	r = uv_pipe_init(uv_default_loop(), pipe_req, 1);
	CuAssertIntEquals(gtc, 0, r);

	uv_fs_t file_req;
	uv_fs_unlink(uv_default_loop(), &file_req, "/dev/gpio1", NULL);

	r = uv_pipe_bind(pipe_req, "/dev/gpio1");
	CuAssertIntEquals(gtc, 0, r);

	r = uv_listen((uv_stream_t *)pipe_req, 128, cb);
	CuAssertIntEquals(gtc, 0, r);

	plua_init();
	test_set_plua_path(gtc, __FILE__, "hardware_raspyrfm.c");

	storage_init();
	CuAssertIntEquals(gtc, 0, storage_read("hardware_raspyrfm.json", CONFIG_SETTINGS));

	hardware_init();
	raspyrfmInit();

	raspyrfm->minrawlen = ARRAY_LEN(rxtestpulses);
	raspyrfm->maxrawlen = ARRAY_LEN(rxtestpulses);

	setenv("PILIGHT_RASPYRFM_READ", "1", 1);
	
	return 0;
}

static void do_timed_loop() {
	timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1500, 0);

	itestraw = 0;

	uv_thread_create(&pth, initthread, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	uv_thread_join(&pth);
}

static void gc() {
	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();
	CuAssertIntEquals(gtc, 0, xfree());
}

static void test_hardware_raspyrfm_receive(CuTest *tc) {
	gtc = tc;
	int i;
	uint8_t checksum = 1;

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain, 0);

	if (start(__FUNCTION__, connect_receive_cb) == -1)
		return;

	do_timed_loop();
	CuAssertIntEquals(gtc, 1, check);

	for (i=0; i<ARRAY_LEN(rfmcfgcheck); i++)
		checksum &= rfmcfgcheck[i].val;
	CuAssertIntEquals(gtc, 1, checksum);

	gc();
}

static void test_hardware_raspyrfm_send(CuTest *tc) {
	gtc = tc;
	int i;
	uint8_t checksum = 1;

	eventpool_init(EVENTPOOL_THREADED);

	if (start(__FUNCTION__, connect_send_cb) == -1)
		return;

	do_timed_loop();
	CuAssertIntEquals(gtc, 1, check);

	for (i=0; i<ARRAY_LEN(rfmcfgcheck); i++)
		checksum &= rfmcfgcheck[i].val;
	CuAssertIntEquals(gtc, 1, checksum);

	gc();
}

static void test_hardware_raspyrfm_param1(CuTest *tc) {
	gtc = tc;

	eventpool_init(EVENTPOOL_THREADED);

	if (start(__FUNCTION__, accept_gpio) == -1)
		return;

	char *settings = "{\"raspyrfm\":{\"spi-channel\":0,\"frequency\":433920}}";
	struct JsonNode *jsettings = json_decode(settings);
	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "raspyrfm", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jspichannel = json_first_child(jchild);
				struct JsonNode *jfrequency = jspichannel->next;
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jspichannel));
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jfrequency));
				CuAssertIntEquals(gtc, RF433, hardware->hwtype);
			}
		}
	}
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->init());
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

static void test_hardware_raspyrfm_param2(CuTest *tc) {
	gtc = tc;

	eventpool_init(EVENTPOOL_THREADED);

	if (start(__FUNCTION__, accept_gpio) == -1)
		return;

	char *settings = "{\"raspyrfm\":{\"spi-channel\":0,\"frequency\":868450}}";
	struct JsonNode *jsettings = json_decode(settings);
	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "raspyrfm", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jspichannel = json_first_child(jchild);
				struct JsonNode *jfrequency = jspichannel->next;
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jspichannel));
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jfrequency));
				CuAssertIntEquals(gtc, RF868, hardware->hwtype);
			}
		}
	}
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->init());
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

static void test_hardware_raspyrfm_param3(CuTest *tc) {
	gtc = tc;

	eventpool_init(EVENTPOOL_THREADED);

	if (start(__FUNCTION__, accept_gpio) == -1)
		return;

	char *settings = "{\"raspyrfm\":{\"spi-channel\":0,\"frequency\":\"433920\"}}";
	struct JsonNode *jsettings = json_decode(settings);
	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "raspyrfm", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jspichannel = json_first_child(jchild);
				struct JsonNode *jfrequency = jspichannel->next;
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jspichannel));
				CuAssertIntEquals(gtc, EXIT_FAILURE, hardware->settings(jfrequency));
			}
		}
	}
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	CuAssertIntEquals(gtc, EXIT_FAILURE, hardware->init());
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

static void test_hardware_raspyrfm_param4(CuTest *tc) {
	gtc = tc;

	eventpool_init(EVENTPOOL_THREADED);

	if (start(__FUNCTION__, accept_gpio) == -1)
		return;

	char *settings = "{\"raspyrfm\":{\"spi-channel\":0,\"frequency\":350000}}";
	struct JsonNode *jsettings = json_decode(settings);
	struct hardware_t *hardware = NULL;
	if(hardware_select_struct(ORIGIN_MASTER, "raspyrfm", &hardware) == 0) {
		if(hardware->init != NULL) {
			if(hardware->comtype == COMOOK) {
				struct JsonNode *jchild = json_first_child(jsettings);
				struct JsonNode *jspichannel = json_first_child(jchild);
				struct JsonNode *jfrequency = jspichannel->next;
				CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->settings(jspichannel));
				CuAssertIntEquals(gtc, EXIT_FAILURE, hardware->settings(jfrequency));
			}
		}
	}
	json_delete(jsettings);

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	CuAssertIntEquals(gtc, EXIT_SUCCESS, hardware->init());
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

CuSuite *suite_hardware_raspyrfm(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_hardware_raspyrfm_send);
	SUITE_ADD_TEST(suite, test_hardware_raspyrfm_receive);
	SUITE_ADD_TEST(suite, test_hardware_raspyrfm_param1);
	SUITE_ADD_TEST(suite, test_hardware_raspyrfm_param2);
	SUITE_ADD_TEST(suite, test_hardware_raspyrfm_param3);
	SUITE_ADD_TEST(suite, test_hardware_raspyrfm_param4);

	return suite;
}