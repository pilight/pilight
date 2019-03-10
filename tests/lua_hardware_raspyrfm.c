/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wiringx.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#ifndef __FreeBSD__
	#include <linux/spi/spidev.h>
#endif

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/table.h"
#include "../libs/pilight/hardware/hardware.h"

#include "alltests.h"

#define TXPWR 13 //-18..13 dBm

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))
#define REG8(adr, val) {adr, val}
#define REG16(adr, val) {adr, ((uint16_t) (val) >> 8) & 0xFF}, {(adr) + 1, (val) & 0xFF}
#define REG24(adr, val) {adr, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 2, (val) & 0xFF}
#define REG32(adr, val) {adr, ((uint32_t) (val) >> 24) & 0xFF}, {(adr) + 1, ((uint32_t) (val) >> 16) & 0xFF}, {(adr) + 2, ((uint32_t) (val) >> 8) & 0xFF}, {(adr) + 3, (val) & 0xFF}

#define FXOSC 32E6
#define FSTEP (FXOSC / (1UL<<19))
#define FREQTOFREG(F) ((uint32_t) (F * 1E6 / FSTEP + .5)) //calculate frequency word
#define BITTIMEUS 31
#define BITRATE ((uint16_t) (FXOSC * BITTIMEUS / 1E6))

static CuTest *gtc = NULL;
static int check = 0;
static int fd = 0;

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

enum {
	OPMODE_STDBY = 1,
	OPMODE_TX = 3,
	OPMODE_RX = 4
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

static const rfmreg_t rfmcfg[] = {
	REG8(REGDATAMODUL, 0<<5 | 1<<3), //packet mode, OOK
	REG16(REGBITRATE, BITRATE),
	REG8(REGAFCCTRL, 0x20), //AfcCtrl: improved routine
	REG8(REGPALEVEL, 1<<7 | (TXPWR + 18)),	//pa power
	REG8(REGLNA, 0x81), //highest gain
	REG8(REGRXBW, 2<<3), //bandwidth 167 kHz
	REG8(REGOOKPEAK, 1<<6 | 3), //OOK mode peak, slowest
	REG8(REGOOKAVG, 0x80),
	REG8(REGOOKFIX, 115),
	REG8(REGAFCFEI, 1<<1), //AFC
	REG8(REGDIOMAPPING1, 2<<6 | 2<<4 | 1<<2 | 0 ), //DIO0 DIO1 DIO2 DIO3
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

static int plua_print(lua_State* L) {
	plua_stack_dump(L);
	return 0;
}

static void plua_overwrite_print(void) {
	struct lua_state_t *state[NRLUASTATES];
	struct lua_State *L = NULL;
	int i = 0;

	for(i=0;i<NRLUASTATES;i++) {
		state[i] = plua_get_free_state();

		if(state[i] == NULL) {
			return;
		}
		if((L = state[i]->L) == NULL) {
			uv_mutex_unlock(&state[i]->lock);
			return;
		}

		lua_getglobal(L, "_G");
		lua_pushcfunction(L, plua_print);
		lua_setfield(L, -2, "print");
		lua_pop(L, 1);
	}
	for(i=0;i<NRLUASTATES;i++) {
		uv_mutex_unlock(&state[i]->lock);
	}
}

static void *reason_send_code_free(void *param) {
	plua_metatable_free(param);
	return NULL;
}

int ioctl_callback(int fd, unsigned long req, void *cmd) {
	if(req == SPI_IOC_MESSAGE(1)) {
		struct spi_ioc_transfer *tr = cmd;

		uint8_t *rxbuf = (void*) (long) tr->rx_buf;
		uint8_t *txbuf = (void*) (long) tr->tx_buf;
		if((rxbuf[0] & 0x80) == 0) { //register read
			switch(rxbuf[0]) {
				case 0x00: //FIFO
					txbuf[1] = rxtestraw[itestraw++];
					if(itestraw >= ARRAY_LEN(rxtestraw)) {
						itestraw = 0;
					}
				break;
				case 0x27: //RegIrqFlags1
					txbuf[1] = 0x80; //mode ready
				break;
				case 0x28: //RegIrqFlags2
					txbuf[1] = 1<<6 | 1<<3; //FIFO not empty & Packet sent
				break;
				default:
				break;
			}
		} else { //register write
			uint8_t reg = (rxbuf[0] & 0x7F);
			uint8_t val = rxbuf[1];
			if(reg == 0x00) {
				//write FIFO register
				if(val != txtestraw[itestraw++]) {
					itestraw = 0;
					check = 0;
				}
				if(itestraw == ARRAY_LEN(txtestraw)) {
					check = 1;
				}
			} else {
				int i = 0;
				for(i=0;i<ARRAY_LEN(rfmcfgcheck);i++) {
					if(rfmcfgcheck[i].reg == reg) {
						if(val == rfmcfg[i].val) {
							rfmcfgcheck[i].val = 1;
						}
						break;
					}
				}
			}
		}
	}
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

static void stop(uv_timer_t *req) {
	uv_stop(uv_default_loop());
}

static void accept_gpio(uv_stream_t *req, int status) {
	CuAssertTrue(gtc, status >= 0);

	uv_pipe_t *client_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, client_req);

	int r = uv_pipe_init(uv_default_loop(), client_req, 0);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_accept(req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	uv_fileno((uv_handle_t *)client_req, &fd);
}

static void connect_receive_cb(uv_stream_t *req, int status) {
	accept_gpio(req, status);

	// trigger IRQ
	CuAssertIntEquals(gtc, 1, write(fd, "a", 1));
}

static void connect_send_cb(uv_stream_t *req, int status) {
	accept_gpio(req, status);

	int rawlen = ARRAY_LEN(txtestpulses), i = 0;
	char key[255];

	struct plua_metatable_t *table = MALLOC(sizeof(struct plua_metatable_t));
	CuAssertPtrNotNull(gtc, table);

	memset(table, 0, sizeof(struct plua_metatable_t));

	memset(&key, 0, 255);

	plua_metatable_set_number(table, "rawlen", rawlen);
	plua_metatable_set_number(table, "txrpt", 0);
	plua_metatable_set_string(table, "protocol", "dummy");
	plua_metatable_set_number(table, "hwtype", 1);
	plua_metatable_set_string(table, "uuid", "0");

	for(i=0;i<rawlen;i++) {
		snprintf(key, 255, "pulses.%d", i+1);
		plua_metatable_set_number(table, key, txtestpulses[i]);
	}

	eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
}

static void *receivePulseTrain(int reason, void *param, void *userdata) {
	struct plua_metatable_t *table = param;
	double length = 0.0;
	double pulse = 0.0;
	char nr[255], *p = nr;
	int buffer[133];
	memset(&buffer, 0, 133*sizeof(int));

	memset(&nr, 0, 255);

	int i = 0;
	plua_metatable_get_number(table, "length", &length);

	for(i=1;i<length;i++) {
		snprintf(p, 254, "pulses.%d", i);
		plua_metatable_get_number(table, nr, &pulse);
		buffer[i-1] = (int)pulse;
	}

	check = 1;
	for(i=0;i<length-1;i++) {
		if(!((int)(buffer[i]*0.75) <= rxtestpulses[i] && (int)(buffer[i]*1.25) >= rxtestpulses[i])) {
			check = 0;
		}
	}

	return (void *)NULL;
}

static int call(struct lua_State *L, char *file, char *func) {
	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		return -1;
	}

	if(lua_pcall(L, 0, 0, 0) == LUA_ERRRUN) {
		logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
		lua_pop(L, 1);
		return -1;
	}

	lua_pop(L, 1);

	return 1;
}

static int start(const char *func, uv_connection_cb cb, const char *config, int ret) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return -1;
	}

	printf("[ %-48s ]\n", func);
	fflush(stdout);

	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	int i = 0;
	for(i=0;i<ARRAY_LEN(rfmcfgcheck);i++) {
		rfmcfgcheck[i].reg = rfmcfg[i].reg;
		rfmcfgcheck[i].val = 0;
	}

	plua_init();

	test_set_plua_path(gtc, __FILE__, "lua_hardware_raspyrfm.c");

	plua_overwrite_print();

	char *file = STRDUP(__FILE__);
	CuAssertPtrNotNull(gtc, file);

	str_replace("lua_hardware_raspyrfm.c", "", &file);

	config_init();

	// create dummy config
	FILE *f = fopen("hardware_raspyrfm.json", "w");
	fprintf(f, config, file);
	fclose(f);
	FREE(file);

	wiringXIOCTLCallback(ioctl_callback);

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_RECEIVED_PULSETRAIN+10000, receivePulseTrain, NULL);

	CuAssertIntEquals(gtc, 0, config_read("hardware_raspyrfm.json", CONFIG_SETTINGS | CONFIG_REGISTRY));

	// create SPI dummy
	int fd = open("/dev/spidev0.0", O_CREAT | O_RDWR, 0777);
	CuAssertTrue(gtc, fd > 0);

	// create GPIO dummy
	uv_pipe_t *pipe_req = MALLOC(sizeof(uv_pipe_t));
	CuAssertPtrNotNull(gtc, pipe_req);

	int r = uv_pipe_init(uv_default_loop(), pipe_req, 1);
	CuAssertIntEquals(gtc, 0, r);

	unlink("/dev/gpio1");

	r = uv_pipe_bind(pipe_req, "/dev/gpio1");
	CuAssertIntEquals(gtc, 0, r);

	r = uv_listen((uv_stream_t *)pipe_req, 128, cb);
	CuAssertIntEquals(gtc, 0, r);

	struct plua_metatable_t *table = config_get_metatable();
	plua_metatable_set_number(table, "registry.hardware.RF433.mingaplen", 5100);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxgaplen", 99999);
	plua_metatable_set_number(table, "registry.hardware.RF433.minrawlen", ARRAY_LEN(rxtestpulses)+1);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxrawlen", ARRAY_LEN(rxtestpulses)+1);

	hardware_init();

	CuAssertIntEquals(gtc, ret, config_read("hardware_raspyrfm.json", CONFIG_HARDWARE));

	return 0;
}

static void do_timed_loop() {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char name[255];

	uv_timer_t *timer_req = MALLOC(sizeof(uv_timer_t));
	CuAssertPtrNotNull(gtc, timer_req);
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1500, 0);

	itestraw = 0;

	state = plua_get_free_state();
	CuAssertPtrNotNull(gtc, state);
	CuAssertPtrNotNull(gtc, (L = state->L));

	sprintf(name, "hardware.%s", "raspyrfm");
	lua_getglobal(L, name);
	CuAssertIntEquals(gtc, LUA_TTABLE, lua_type(L, -1));

	char *file = NULL;
	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("raspyrfm", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(gtc, file);
	CuAssertIntEquals(gtc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	uv_mutex_unlock(&state->lock);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}
}

static void gc() {
	storage_gc();
	plua_gc();
	hardware_gc();
	eventpool_gc();
	wiringXGC();

	CuAssertIntEquals(gtc, 0, xfree());
}

static void test_lua_hardware_raspyrfm_receive(CuTest *tc) {
	gtc = tc;

	check = 1;

	char *config = "{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"gpio-platform\":\"gpio-stub\"\
		},\
		\"hardware\":{},\"registry\":{}}";

	CuAssertIntEquals(tc, 0, start(__FUNCTION__, connect_receive_cb, config, 0));

	do_timed_loop();
	CuAssertIntEquals(gtc, 1, check);

	gc();
}

static void test_lua_hardware_raspyrfm_send(CuTest *tc) {
	int i = 0;
	uint8_t checksum = 1;

	gtc = tc;

	check = 1;

	char *config = "{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"gpio-platform\":\"gpio-stub\"\
		},\
		\"hardware\":{},\"registry\":{}}";

	CuAssertIntEquals(tc, 0, start(__FUNCTION__, connect_send_cb, config, 0));

	do_timed_loop();
	CuAssertIntEquals(gtc, 1, check);

	for(i=0;i<ARRAY_LEN(rfmcfgcheck);i++) {
		checksum &= rfmcfgcheck[i].val;
	}

	CuAssertIntEquals(gtc, 1, checksum);

	gc();
}

static void test_lua_hardware_raspyrfm_param1(CuTest *tc) {
	gtc = tc;

	check = 1;

	char *config = "{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"gpio-platform\":\"gpio-stub\"\
		},\
		\"hardware\":{\
			\"raspyrfm\": {\"spi-channel\": 0, \"frequency\": 433920}\
		},\"registry\":{}}";

	CuAssertIntEquals(tc, 0, start(__FUNCTION__, accept_gpio, config, 0));

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

static void test_lua_hardware_raspyrfm_param2(CuTest *tc) {
	gtc = tc;

	check = 1;

	char *config = "{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"gpio-platform\":\"gpio-stub\"\
		},\
		\"hardware\":{\
			\"raspyrfm\": {\"spi-channel\": 0, \"frequency\": 868450}\
		},\"registry\":{}}";

	CuAssertIntEquals(tc, 0, start(__FUNCTION__, accept_gpio, config, 0));

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

static void test_lua_hardware_raspyrfm_param3(CuTest *tc) {
	gtc = tc;

	check = 1;

	char *config = "{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"gpio-platform\":\"gpio-stub\"\
		},\
		\"hardware\":{\
			\"raspyrfm\": {\"spi-channel\": 0, \"frequency\": \"433920\"}\
		},\"registry\":{}}";

	CuAssertIntEquals(tc, 0, start(__FUNCTION__, accept_gpio, config, -1));

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

static void test_lua_hardware_raspyrfm_param4(CuTest *tc) {
	gtc = tc;

	check = 1;

	char *config = "{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\
				\"hardware-root\":\"%s../libs/pilight/hardware/\",\
				\"gpio-platform\":\"gpio-stub\"\
		},\
		\"hardware\":{\
			\"raspyrfm\": {\"spi-channel\": 0, \"frequency\": 350000}\
		},\"registry\":{}}";

	CuAssertIntEquals(tc, 0, start(__FUNCTION__, accept_gpio, config, -1));

	uv_run(uv_default_loop(), UV_RUN_NOWAIT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	gc();
}

CuSuite *suite_lua_hardware_raspyrfm(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_hardware_raspyrfm_send);
	SUITE_ADD_TEST(suite, test_lua_hardware_raspyrfm_receive);
	SUITE_ADD_TEST(suite, test_lua_hardware_raspyrfm_param1);
	SUITE_ADD_TEST(suite, test_lua_hardware_raspyrfm_param2);
	SUITE_ADD_TEST(suite, test_lua_hardware_raspyrfm_param3);
	SUITE_ADD_TEST(suite, test_lua_hardware_raspyrfm_param4);

	return suite;
}
