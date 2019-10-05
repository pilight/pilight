/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/http.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/webserver.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/mdns.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"

#include "alltests.h"

static uv_thread_t pth;
static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;
static int mdns_socket = 0;
static int mdns_loop = 1;
static int check = 0;
static int run = 0;
static int test = 0;
static int foo = 0;
static CuTest *gtc = NULL;

static unsigned char message_qr0[] = {
	0xaa, 0xbb, 0x12, 0xa1, 0x00, 0x03, 0x00, 0x05,
	0x00, 0x00, 0x00, 0x05, 0x08, 0x74, 0x65, 0x73,
	0x74, 0x6e, 0x61, 0x6d, 0x65, 0x05, 0x6c, 0x6f,
	0x63, 0x61, 0x6c, 0x03, 0x66, 0x6f, 0x6f, 0x00,
	0x00, 0x21, 0x00, 0x01, 0x09, 0x74, 0x65, 0x73,
	0x74, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x06, 0x6c,
	0x6f, 0x63, 0x61, 0x6c, 0x31, 0xc0, 0x1b, 0x00,
	0x21, 0x00, 0x01, 0x08, 0x74, 0x65, 0x73, 0x74,
	0x6e, 0x61, 0x6d, 0x65, 0xc0, 0x2e, 0x00, 0x21,
	0x00, 0x01, 0xc0, 0x0c, 0x00, 0x0c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xc0, 0x0c,
	0x07, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74,
	0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00,
	0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x17, 0x00, 0x01, 0x00, 0x01, 0x00, 0x50, 0x07,
	0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x06,
	0x64, 0x61, 0x65, 0x6d, 0x6f, 0x6e, 0xc0, 0x60,
	0x07, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74,
	0x06, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x31, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0xc0, 0xa8, 0x01, 0x02, 0xc0, 0x88,
	0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f,
	0x70, 0x81, 0x92, 0xa3, 0xb4, 0xc5, 0xd6, 0xe7,
	0xf8, 0x09, 0xc0, 0x88, 0x00, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x03, 0x66,
	0x6f, 0x6f, 0x05, 0x6c, 0x6f, 0x72, 0x65, 0x6d,
	0xc0, 0x0c, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0xc0, 0x0c, 0xc0, 0x58,
	0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x01, 0x00, 0x01, 0x00, 0x50,
	0xc0, 0x77, 0xc0, 0x88, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xc0, 0xa8,
	0x01, 0x02, 0xc0, 0x88, 0x00, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0a, 0x1b,
	0x2c, 0x3d, 0x4e, 0x5f, 0x70, 0x81, 0x92, 0xa3,
	0xb4, 0xc5, 0xd6, 0xe7, 0xf8, 0x09, 0xc0, 0x88,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0a, 0x03, 0x66, 0x6f, 0x6f, 0x05, 0x6c,
	0x6f, 0x72, 0x65, 0x6d
};

static unsigned char message_qr1[] = {
	0xaa, 0xbb, 0x92, 0xa1, 0x00, 0x00, 0x00, 0x05,
	0x00, 0x00, 0x00, 0x05, 0x08, 0x74, 0x65, 0x73,
	0x74, 0x6e, 0x61, 0x6d, 0x65, 0x05, 0x6c, 0x6f,
	0x63, 0x61, 0x6c, 0x03, 0x66, 0x6f, 0x6f, 0x00,
	0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x02, 0xc0, 0x0c, 0x07, 0x70, 0x69, 0x6c,
	0x69, 0x67, 0x68, 0x74, 0x05, 0x6c, 0x6f, 0x63,
	0x61, 0x6c, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x50, 0x07, 0x70, 0x69, 0x6c, 0x69,
	0x67, 0x68, 0x74, 0x06, 0x64, 0x61, 0x65, 0x6d,
	0x6f, 0x6e, 0xc0, 0x34, 0x07, 0x70, 0x69, 0x6c,
	0x69, 0x67, 0x68, 0x74, 0x06, 0x6c, 0x6f, 0x63,
	0x61, 0x6c, 0x31, 0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xc0, 0xa8,
	0x01, 0x02, 0xc0, 0x5c, 0x00, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0a, 0x1b,
	0x2c, 0x3d, 0x4e, 0x5f, 0x70, 0x81, 0x92, 0xa3,
	0xb4, 0xc5, 0xd6, 0xe7, 0xf8, 0x09, 0xc0, 0x5c,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0a, 0x03, 0x66, 0x6f, 0x6f, 0x05, 0x6c,
	0x6f, 0x72, 0x65, 0x6d, 0xc0, 0x0c, 0x00, 0x0c,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
	0xc0, 0x0c, 0xc0, 0x2c, 0x00, 0x21, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x50, 0xc0, 0x4b, 0xc0, 0x5c,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0xc0, 0xa8, 0x01, 0x02, 0xc0, 0x5c,
	0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f,
	0x70, 0x81, 0x92, 0xa3, 0xb4, 0xc5, 0xd6, 0xe7,
	0xf8, 0x09, 0xc0, 0x5c, 0x00, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x03, 0x66,
	0x6f, 0x6f, 0x05, 0x6c, 0x6f, 0x72, 0x65, 0x6d
};

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);

	uv_stop(uv_default_loop());
}

static int validate_queries(lua_State *L, int offset) {
	switch(run-offset) {
		case 0: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 33, lua_tonumber(L, -1));
			run++;
		} break;
		case 1: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 2: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "testname.local.foo", lua_tostring(L, -1));
			run++;
		} break;
		case 3: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
			run++;
		} break;
		case 4: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 33, lua_tonumber(L, -1));
			run++;
		} break;
		case 5: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 6: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "testname1.local1.foo", lua_tostring(L, -1));
			run++;
		} break;
		case 7: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
			run++;
		} break;
		case 8: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 33, lua_tonumber(L, -1));
			run++;
		} break;
		case 9: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 10: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "testname.local1.foo", lua_tostring(L, -1));
			run++;
		} break;
		case 11: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
			run++;
		} break;
	}

	return 0;
}

static int validate_payload(lua_State *L, int offset) {
	switch(run-offset) {
		case 0: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 12, lua_tonumber(L, -1));
			run++;
		} break;
		case 1: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "testname.local.foo", lua_tostring(L, -1));
			run++;
		} break;
		case 2: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 2, lua_tonumber(L, -1));
			run++;
		} break;
		case 3: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 4: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 5: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 6: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "testname.local.foo", lua_tostring(L, -1));
			run++;
		} break;
		case 7: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 33, lua_tonumber(L, -1));
			run++;
		} break;
		case 8: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "pilight.local", lua_tostring(L, -1));
			run++;
		} break;
		case 9: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			if(offset == 30 || offset == 16) {
				CuAssertIntEquals(gtc, 23, lua_tonumber(L, -1));
			} else {
				CuAssertIntEquals(gtc, 8, lua_tonumber(L, -1));
			}
			run++;
		} break;
		case 10: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 11: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 12: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "pilight.daemon.local", lua_tostring(L, -1));
			run++;
		} break;
		case 13: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
			run++;
		} break;
		case 14: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
			run++;
		} break;
		case 15: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 80, lua_tonumber(L, -1));
			run++;
		} break;
		case 16: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
			run++;
		} break;
		case 17: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "pilight.local1", lua_tostring(L, -1));
			run++;
		} break;
		case 18: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 4, lua_tonumber(L, -1));
			run++;
		} break;
		case 19: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 20: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 21: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "192.168.1.2", lua_tostring(L, -1));
			run++;
		} break;
		case 22: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 28, lua_tonumber(L, -1));
			run++;
		} break;
		case 23: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "pilight.local1", lua_tostring(L, -1));
			run++;
		} break;
		case 24: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 16, lua_tonumber(L, -1));
			run++;
		} break;
		case 25: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 26: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 27: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "0a1b:2c3d:4e5f:7081:92a3:b4c5:d6e7:f809", lua_tostring(L, -1));
			run++;
		} break;
		case 28: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 16, lua_tonumber(L, -1));
			run++;
		} break;
		case 29: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "pilight.local1", lua_tostring(L, -1));
			run++;
		} break;
		case 30: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 10, lua_tonumber(L, -1));
			run++;
		} break;
		case 31: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 32: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 33: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "foo", lua_tostring(L, -1));
			run++;
		} break;
		case 34: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "lorem", lua_tostring(L, -1));
			run++;
		} break;
	}
	return 0;
}

static int plua_print(lua_State* L) {
	switch(test) {
		case 1:
		case 2: {
			switch(run) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TTABLE, lua_type(L, -1));
					run++;
				} break;
				case 1:
				case 2: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "aabb", lua_tostring(L, -1));
					run++;
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					if(test == 2) {
						CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					} else {
						CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
					}
					run++;
				} break;
				case 5: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 2, lua_tonumber(L, -1));
					run++;
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
					run++;
				} break;
				case 7: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 8: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
					run++;
				} break;
				case 9: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 10: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
					run++;
				} break;
				case 11: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 12: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
					run++;
				} break;
				case 13: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 14: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
					run++;
				} break;
				case 15: {
					if(test == 2) {
						run++;
						goto foo;
					}
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 3, lua_tonumber(L, -1));
					run++;
				} break;
				case 16: {
					if(test == 2) {
						goto foo;
					}
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 5, lua_tonumber(L, -1));
					run++;
				} break;
				case 17: {
					if(test == 2) {
						goto foo;
					}
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 5, lua_tonumber(L, -1));
					run++;
					return 0;
				} break;
				/*
				 * Queries
				 */
			}
		} break;
	}
	if(test == 1) {
		if(run >= 65) {
			validate_payload(L, 65);
		} else if(run >= 30) {
			validate_payload(L, 30);
		} else if(run >= 18) {
			validate_queries(L, 18);
		}

		if(run >= 100) {
			struct sockaddr_in addr;

			memset((char *)&addr, '\0', sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = inet_addr("224.0.0.251");
		#ifdef PILIGHT_UNITTEST
			addr.sin_port = htons(15353);
		#else
			addr.sin_port = htons(5353);
		#endif

			int r = sendto(mdns_socket, message_qr1, sizeof(message_qr1), 0, (struct sockaddr *)&addr, sizeof(addr));
			CuAssertTrue(gtc, (r == sizeof(message_qr1)));

			test = 2;
			run = 3;
			return 0;
		}
	} else if(test == 2) {
foo:
		if(run >= 51) {
			validate_payload(L, 51);
		} else if(run >= 16) {
			validate_payload(L, 16);
		}

		if(run >= 86) {
			uv_stop(uv_default_loop());
		}
	}
	return 0;
}

static int call(struct lua_State *L, char *file, char *func) {
	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		return -1;
	}

	if(plua_pcall(L, file, 0, 0) == -1) {
		assert(plua_check_stack(L, 0) == 0);
		return -1;
	}

	lua_pop(L, 1);

	return 1;
}

static void mdns_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	int n = 0, r = 0;
	socklen_t addrlen = sizeof(addr);
	fd_set fdsread;
	char buffer[8096];

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(mdns_socket, FIONBIO, &on);
	assert(r >= 0);
#else
	long arg = fcntl(mdns_socket, F_GETFL, NULL);
	r = fcntl(mdns_socket, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, (r >= 0));
#endif

	while(mdns_loop) {
		FD_ZERO(&fdsread);
		FD_SET((unsigned long)mdns_socket, &fdsread);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		do {
			n = select(mdns_socket+1, &fdsread, NULL, NULL, &tv);
		} while(n == -1 && errno == EINTR && mdns_loop);

		if(n == 0) {
			continue;
		}
		if(mdns_loop == 0) {
			break;
		}
		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(FD_ISSET(mdns_socket, &fdsread)) {
				memset(buffer, '\0', sizeof(buffer));
				r = recvfrom(mdns_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);
				if(foo == 3) {
					if(test == 1) {
						CuAssertIntEquals(gtc, r, 74);
					} else {
						CuAssertIntEquals(gtc, r, 272);
						return;
					}
				}
				if(foo == 2) {
					if(test == 1) {
						CuAssertIntEquals(gtc, r, 230);
					} else {
						CuAssertIntEquals(gtc, r, 272);
						return;
					}
				}

				struct mdns_packet_t pkt;
				memset(&pkt, 0, sizeof(struct mdns_packet_t));
				CuAssertIntEquals(gtc, mdns_decode(&pkt, (unsigned char *)buffer, r), 0);

				CuAssertIntEquals(gtc, 0xAABB, pkt.id);
				if(foo == 3) {
					CuAssertIntEquals(gtc, 0, pkt.qr);
				}
				CuAssertIntEquals(gtc, 2, pkt.opcode);
				CuAssertIntEquals(gtc, 0, pkt.aa);
				CuAssertIntEquals(gtc, 1, pkt.tc);
				CuAssertIntEquals(gtc, 0, pkt.rd);
				CuAssertIntEquals(gtc, 1, pkt.ra);
				CuAssertIntEquals(gtc, 0, pkt.z);
				CuAssertIntEquals(gtc, 1, pkt.ad);
				CuAssertIntEquals(gtc, 0, pkt.cd);
				CuAssertIntEquals(gtc, 1, pkt.rcode);
				if(foo == 3) {
					CuAssertIntEquals(gtc, 3, pkt.nrqueries);
					CuAssertIntEquals(gtc, 0, pkt.nranswers);
					CuAssertIntEquals(gtc, 0, pkt.rr_add);
				} else if(test == 2) {
					CuAssertIntEquals(gtc, 0, pkt.nrqueries);
					CuAssertIntEquals(gtc, 5, pkt.nranswers);
					CuAssertIntEquals(gtc, 5, pkt.rr_add);
				}
				CuAssertIntEquals(gtc, 0, pkt.rr_auth);

				int x = 0;
				for(x=0;x<pkt.nrqueries;x++) {
					if(pkt.qr == 0) {
						switch(x) {
							case 0: {
								// CuAssertStrEquals(gtc, "testname.local.foo", pkt.queries[x]->query.name);
								CuAssertStrEquals(gtc, "testname.local.foo", pkt.queries[x]->name);
							} break;
							case 1: {
								// CuAssertStrEquals(gtc, "testname1.local1.foo", pkt.queries[x]->query.name);
								CuAssertStrEquals(gtc, "testname1.local1.foo", pkt.queries[x]->name);
							} break;
							case 2: {
								// CuAssertStrEquals(gtc, "testname.local1.foo", pkt.queries[x]->query.name);
								CuAssertStrEquals(gtc, "testname.local1.foo", pkt.queries[x]->name);
							} break;
						}
						/*CuAssertIntEquals(gtc, 0, pkt.queries[x]->query.qu);
						CuAssertIntEquals(gtc, 33, pkt.queries[x]->query.type);
						CuAssertIntEquals(gtc, 1, pkt.queries[x]->query.class);*/
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->qu);
						CuAssertIntEquals(gtc, 33, pkt.queries[x]->type);
						CuAssertIntEquals(gtc, 1, pkt.queries[x]->class);
					} else {
						/*switch(x) {
							case 0: {
								CuAssertStrEquals(gtc, "testname.local.foo", pkt.queries[x]->payload.name);
								CuAssertStrEquals(gtc, "testname.local.foo", pkt.queries[x]->payload.data.domain);
								CuAssertIntEquals(gtc, 12, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 2, pkt.queries[x]->payload.length);
							} break;
							case 1: {
								CuAssertStrEquals(gtc, "pilight.local", pkt.queries[x]->payload.name);
								CuAssertStrEquals(gtc, "pilight.daemon.local", pkt.queries[x]->payload.data.domain);
								CuAssertIntEquals(gtc, 33, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.priority);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.weight);
								CuAssertIntEquals(gtc, 80, pkt.queries[x]->payload.port);
								CuAssertIntEquals(gtc, 23, pkt.queries[x]->payload.length);
							} break;
							case 2: {
								CuAssertStrEquals(gtc, "pilight.local1", pkt.queries[x]->payload.name);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 4, pkt.queries[x]->payload.length);
								CuAssertIntEquals(gtc, 192, pkt.queries[x]->payload.data.ip4[0]);
								CuAssertIntEquals(gtc, 168, pkt.queries[x]->payload.data.ip4[1]);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.data.ip4[2]);
								CuAssertIntEquals(gtc, 2, pkt.queries[x]->payload.data.ip4[3]);
							} break;
							case 3: {
								CuAssertStrEquals(gtc, "pilight.local1", pkt.queries[x]->payload.name);
								CuAssertIntEquals(gtc, 28, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 16, pkt.queries[x]->payload.length);
								int i = 0;
								unsigned char foo = 0xF9;
								for(i=0;i<16;i++) {
									foo += 0x11;
									CuAssertIntEquals(gtc, foo, pkt.queries[x]->payload.data.ip6[i]);
								}
							} break;
							case 4: {
								CuAssertStrEquals(gtc, "pilight.local1", pkt.queries[x]->payload.name);
								CuAssertIntEquals(gtc, 16, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 10, pkt.queries[x]->payload.length);
								CuAssertStrEquals(gtc, "foo", pkt.queries[x]->payload.data.text.values[0]);
								CuAssertStrEquals(gtc, "lorem", pkt.queries[x]->payload.data.text.values[1]);
							} break;
						}
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->payload.flush);
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->payload.class);
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->payload.ttl);*/
					}
				}

				for(x=0;x<pkt.nranswers;x++) {
					switch(x) {
						case 0: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->name);
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->data.domain);
							CuAssertIntEquals(gtc, 12, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 2, pkt.answers[x]->length);
						} break;
						case 1: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->name);
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->data.domain);
							CuAssertIntEquals(gtc, 33, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->priority);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->weight);
							CuAssertIntEquals(gtc, 80, pkt.answers[x]->port);
							if(pkt.qr == 0) {
								CuAssertIntEquals(gtc, 8, pkt.answers[x]->length);
							} else {
								CuAssertIntEquals(gtc, 8, pkt.answers[x]->length);
							}
						} break;
						case 2: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->name);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 4, pkt.answers[x]->length);
							CuAssertIntEquals(gtc, 192, pkt.answers[x]->data.ip4[0]);
							CuAssertIntEquals(gtc, 168, pkt.answers[x]->data.ip4[1]);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->data.ip4[2]);
							CuAssertIntEquals(gtc, 2, pkt.answers[x]->data.ip4[3]);
						} break;
						case 3: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->name);
							CuAssertIntEquals(gtc, 28, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 16, pkt.answers[x]->length);
							int i = 0;
							unsigned char foo = 0xF9;
							for(i=0;i<16;i++) {
								foo += 0x11;
								CuAssertIntEquals(gtc, foo, pkt.answers[x]->data.ip6[i]);
							}
						} break;
						case 4: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->name);
							CuAssertIntEquals(gtc, 16, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 10, pkt.answers[x]->length);
							CuAssertStrEquals(gtc, "foo", pkt.answers[x]->data.text.values[0]);
							CuAssertStrEquals(gtc, "lorem", pkt.answers[x]->data.text.values[1]);
						} break;
					}
					CuAssertIntEquals(gtc, 1, pkt.answers[x]->flush);
					CuAssertIntEquals(gtc, 3, pkt.answers[x]->class);
					CuAssertIntEquals(gtc, 2, pkt.answers[x]->ttl);
				}

				for(x=0;x<pkt.rr_add;x++) {
					switch(x) {
						case 0: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->name);
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->data.domain);
							CuAssertIntEquals(gtc, 12, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 2, pkt.records[x]->length);
						} break;
						case 1: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->name);
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->data.domain);
							CuAssertIntEquals(gtc, 33, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->priority);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->weight);
							CuAssertIntEquals(gtc, 80, pkt.records[x]->port);
							CuAssertIntEquals(gtc, 8, pkt.records[x]->length);
						} break;
						case 2: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->name);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 4, pkt.records[x]->length);
							CuAssertIntEquals(gtc, 192, pkt.records[x]->data.ip4[0]);
							CuAssertIntEquals(gtc, 168, pkt.records[x]->data.ip4[1]);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->data.ip4[2]);
							CuAssertIntEquals(gtc, 2, pkt.records[x]->data.ip4[3]);
						} break;
						case 3: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->name);
							CuAssertIntEquals(gtc, 28, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 16, pkt.records[x]->length);
							int i = 0;
							unsigned char foo = 0xF9;
							for(i=0;i<16;i++) {
								foo += 0x11;
								CuAssertIntEquals(gtc, foo, pkt.records[x]->data.ip6[i]);
							}
						} break;
						case 4: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->name);
							CuAssertIntEquals(gtc, 16, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 10, pkt.records[x]->length);
							CuAssertStrEquals(gtc, "foo", pkt.records[x]->data.text.values[0]);
							CuAssertStrEquals(gtc, "lorem", pkt.records[x]->data.text.values[1]);
						} break;
					}
					CuAssertIntEquals(gtc, 1, pkt.records[x]->flush);
					CuAssertIntEquals(gtc, 3, pkt.records[x]->class);
					CuAssertIntEquals(gtc, 2, pkt.records[x]->ttl);
				}

				mdns_free(&pkt);

				check = 1;

				r = sendto(mdns_socket, message_qr0, sizeof(message_qr0), 0, (struct sockaddr *)&addr, sizeof(addr));
				CuAssertTrue(gtc, (r == sizeof(message_qr0)));
			}
		}
	}

clear:
	return;
}

static void mdns_server_tx(void) {
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	mdns_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (mdns_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef PILIGHT_UNITTEST
	addr.sin_port = htons(15353);
#else
	addr.sin_port = htons(5353);
#endif

	r = setsockopt(mdns_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	inet_pton(AF_INET, "224.0.0.251", &mreq.imr_multiaddr.s_addr);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	r = setsockopt(mdns_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(mdns_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));
}

static void mdns_server_rx(void) {
	struct sockaddr_in addr;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	mdns_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (mdns_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("224.0.0.251");
#ifdef PILIGHT_UNITTEST
	addr.sin_port = htons(15353);
#else
	addr.sin_port = htons(5353);
#endif

	r = setsockopt(mdns_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(mdns_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));

	r = sendto(mdns_socket, message_qr0, sizeof(message_qr0), 0, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r == sizeof(message_qr0)));
}

static void stop(void) {
	uv_async_send(async_close_req);
}

static void test_lua_network_mdns_tx(CuTest *tc, int bar) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	mdns_loop = 1;
	run = 0;
	check = 0;
	test = 1;
	foo = bar;

	gtc = tc;
	memtrack();

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);

	mdns_server_tx();
	uv_thread_create(&pth, mdns_wait, NULL);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_network_mdns.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%smdns%d.lua", file, bar);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("mdns", UNITTEST));

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	/*
	 * Don't make this too quick so we can properly test the
	 * timeout of the mdns library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	eventpool_init(EVENTPOOL_NO_THREADS);

	sprintf(name, "unittest.%s", "mdns");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("mdns", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	plua_clear_state(state);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	mdns_loop = 0;
	uv_thread_join(&pth);

	plua_pause_coverage(0);
	plua_gc();
	mdns_gc();

	CuAssertIntEquals(tc, 3, run);
	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_mdns_tx1(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	test_lua_network_mdns_tx(tc, 2);
}

static void test_lua_network_mdns_tx2(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	test_lua_network_mdns_tx(tc, 3);
}

static void test_lua_network_mdns_rx(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;
	mdns_loop = 1;
	run = 0;
	test = 1;

	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_network_mdns.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%smdns1.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("mdns", UNITTEST));

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	/*
	 * Don't make this too quick so we can properly test the
	 * timeout of the mdns library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	eventpool_init(EVENTPOOL_NO_THREADS);

	sprintf(name, "unittest.%s", "mdns");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("mdns", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	plua_clear_state(state);

	mdns_server_rx();

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_pause_coverage(0);
	eventpool_gc();
	plua_gc();
	mdns_gc();

	CuAssertIntEquals(tc, 86, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_network_mdns(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_network_mdns_tx1);
	SUITE_ADD_TEST(suite, test_lua_network_mdns_tx2);
	SUITE_ADD_TEST(suite, test_lua_network_mdns_rx);

	return suite;
}
