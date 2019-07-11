/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/coap.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"

static uv_thread_t pth;
static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;
static int coap_socket = 0;
static int coap_loop = 1;
static int run = 0;
static int test = 0;
static CuTest *gtc = NULL;

/*
 * Shelly coap://x.x.x.x/cit/d response message
 */
static unsigned char shelly_resp[] = {
	0x50, 0x45, 0x00, 0x01, 0xed, 0x0b, 0xf7, 0x02, 0x53, 0x48, 0x53, 0x57, 0x2d, 0x31,
	0x23, 0x45, 0x30, 0x38, 0x43, 0x42, 0x39, 0x23, 0x31, 0xff, 0x7b, 0x22, 0x62, 0x6c,
	0x6b, 0x22, 0x3a, 0x5b, 0x7b, 0x22, 0x49, 0x22, 0x3a, 0x30, 0x2c, 0x22, 0x44, 0x22,
	0x3a, 0x22, 0x52, 0x65, 0x6c, 0x61, 0x79, 0x30, 0x22, 0x7d, 0x5d, 0x2c, 0x22, 0x73,
	0x65, 0x6e, 0x22, 0x3a, 0x5b, 0x7b, 0x22, 0x49, 0x22, 0x3a, 0x31, 0x31, 0x32, 0x2c,
	0x22, 0x54, 0x22, 0x3a, 0x22, 0x53, 0x77, 0x69, 0x74, 0x63, 0x68, 0x22, 0x2c, 0x22,
	0x52, 0x22, 0x3a, 0x22, 0x30, 0x2f, 0x31, 0x22, 0x2c, 0x22, 0x4c, 0x22, 0x3a, 0x30,
	0x7d, 0x5d, 0x2c, 0x22, 0x61, 0x63, 0x74, 0x22, 0x3a, 0x5b, 0x7b, 0x22, 0x49, 0x22,
	0x3a, 0x32, 0x31, 0x31, 0x2c, 0x22, 0x44, 0x22, 0x3a, 0x22, 0x53, 0x77, 0x69, 0x74,
	0x63, 0x68, 0x22, 0x2c, 0x22, 0x4c, 0x22, 0x3a, 0x30, 0x2c, 0x22, 0x50, 0x22, 0x3a,
	0x5b, 0x7b, 0x22, 0x49, 0x22, 0x3a, 0x32, 0x30, 0x31, 0x31, 0x2c, 0x22, 0x44, 0x22,
	0x3a, 0x22, 0x54, 0x6f, 0x53, 0x74, 0x61, 0x74, 0x65, 0x22, 0x2c, 0x22, 0x52, 0x22,
	0x3a, 0x22, 0x30, 0x2f, 0x31, 0x22, 0x7d, 0x5d, 0x7d, 0x5d, 0x7d
};

static int plua_print(lua_State* L) {
	switch(test) {
		case 0: {
			switch(run) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 5, lua_tonumber(L, -1));
					run++;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "main", lua_tostring(L, -1));
					run++;
				} break;
				case 2: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 5: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
					run++;
				} break;
				case 7: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 205, lua_tonumber(L, -1));
					run++;
				} break;
				case 8: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 9: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 10: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "SHSW-1#E08CB9#1", lua_tostring(L, -1));
					run++;
				} break;
				case 11: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 3332, lua_tonumber(L, -1));
					run++;
				} break;
				case 12: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "{\"blk\":[{\"I\":0,\"D\":\"Relay0\"}],\"sen\":[{\"I\":112,\"T\":\"Switch\",\"R\":\"0/1\",\"L\":0}],\"act\":[{\"I\":211,\"D\":\"Switch\",\"L\":0,\"P\":[{\"I\":2011,\"D\":\"ToState\",\"R\":\"0/1\"}]}]}", lua_tostring(L, -1));
					uv_stop(uv_default_loop());
					run++;
				} break;
			}
		} break;
		case 1: {
			switch(run) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 2: {
					/*
					 * FIXME: Check ip adress of travis adapter
					 */
					// CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					// CuAssertStrEquals(gtc, "10.0.0.212", lua_tostring(L, -1));
					run++;
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 15683, lua_tonumber(L, -1));
					run++;
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 5: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
					run++;
				} break;
				case 7: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 205, lua_tonumber(L, -1));
					run++;
				} break;
				case 8: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 9: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_tonumber(L, -1));
					run++;
				} break;
				case 10: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "SHSW-1#E08CB9#1", lua_tostring(L, -1));
					run++;
				} break;
				case 11: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 3332, lua_tonumber(L, -1));
					run++;
				} break;
				case 12: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "{\"blk\":[{\"I\":0,\"D\":\"Relay0\"}],\"sen\":[{\"I\":112,\"T\":\"Switch\",\"R\":\"0/1\",\"L\":0}],\"act\":[{\"I\":211,\"D\":\"Switch\",\"L\":0,\"P\":[{\"I\":2011,\"D\":\"ToState\",\"R\":\"0/1\"}]}]}", lua_tostring(L, -1));
					uv_stop(uv_default_loop());
					run++;
				} break;
			}
		} break;
	}
	return 0;
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);

	uv_stop(uv_default_loop());
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void coap_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	int n = 0, r = 0;
	socklen_t addrlen = sizeof(addr);
	fd_set fdsread;
	char message[1025];

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(coap_socket, FIONBIO, &on);
	assert(r >= 0);
#else
	long arg = fcntl(coap_socket, F_GETFL, NULL);
	r = fcntl(coap_socket, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, (r >= 0));
#endif

	while(coap_loop) {
		FD_ZERO(&fdsread);
		FD_SET((unsigned long)coap_socket, &fdsread);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		do {
			n = select(coap_socket+1, &fdsread, NULL, NULL, &tv);
		} while(n == -1 && errno == EINTR && coap_loop);

		if(n == 0) {
			continue;
		}
		if(coap_loop == 0) {
			break;
		}
		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(FD_ISSET(coap_socket, &fdsread)) {
				memset(message, '\0', BUFFER_SIZE);
				r = recvfrom(coap_socket, message, sizeof(message), 0, (struct sockaddr *)&addr, &addrlen);
				CuAssertIntEquals(gtc, r, 10);

				struct coap_packet_t pkt;
				CuAssertIntEquals(gtc, coap_decode(&pkt, (unsigned char *)message, r), 0);
				CuAssertIntEquals(gtc, pkt.ver, 1);
				CuAssertIntEquals(gtc, pkt.t, 1);
				CuAssertIntEquals(gtc, pkt.tkl, 0);
				CuAssertIntEquals(gtc, pkt.code[0], 0);
				CuAssertIntEquals(gtc, pkt.code[1], 1);
				CuAssertIntEquals(gtc, pkt.msgid[0], 0x00);
				CuAssertIntEquals(gtc, pkt.msgid[1], 0x01);
				CuAssertIntEquals(gtc, pkt.numopt, 2);
				CuAssertStrEquals(gtc, (char *)pkt.options[0]->val, "cit");
				CuAssertIntEquals(gtc, pkt.options[0]->num, 11);
				CuAssertIntEquals(gtc, pkt.options[0]->len, 3);
				CuAssertStrEquals(gtc, (char *)pkt.options[1]->val, "d");
				CuAssertIntEquals(gtc, pkt.options[1]->num, 11);
				CuAssertIntEquals(gtc, pkt.options[1]->len, 1);
				CuAssertTrue(gtc, pkt.payload == NULL);
				CuAssertTrue(gtc, pkt.token == NULL);
				coap_free(&pkt);

				r = sendto(coap_socket, shelly_resp, 179, 0, (struct sockaddr *)&addr, sizeof(addr));
				CuAssertTrue(gtc, (r == 179));
			}
		}
	}

	close(coap_socket);
clear:
	return;
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

static void coap_server_tx(void) {
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

	coap_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (coap_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef PILIGHT_UNITTEST
	addr.sin_port = htons(15683);
#else
	addr.sin_port = htons(5683);
#endif

	r = setsockopt(coap_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	inet_pton(AF_INET, "224.0.1.187", &mreq.imr_multiaddr.s_addr);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	r = setsockopt(coap_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(coap_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));
}

static void coap_server_rx(void) {
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

	coap_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (coap_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("224.0.1.187");
#ifdef PILIGHT_UNITTEST
	addr.sin_port = htons(15683);
#else
	addr.sin_port = htons(5683);
#endif

	r = setsockopt(coap_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(coap_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));

	r = sendto(coap_socket, shelly_resp, 179, 0, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r == 179));

	close(coap_socket);
}

static void stop(uv_timer_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);

	uv_stop(uv_default_loop());
}

static void test_lua_network_coap_missing_parameters(CuTest *tc) {
	struct lua_state_t *state = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();

	plua_init();
	plua_overwrite_print();
	plua_pause_coverage(1);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local coap = pilight.network.coap(); coap.callback();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local coap = pilight.network.coap(); coap.setCallback(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local coap = pilight.network.coap(); coap.listen(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local coap = pilight.network.coap(); coap.send();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local coap = pilight.network.coap(); coap.send(\"foo\");"));

	while(lua_gettop(state->L) > 0) {
		lua_remove(state->L, -1);
	}
	plua_clear_state(state);

	plua_pause_coverage(0);
	plua_gc();
	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_coap_invalid_packet(CuTest *tc) {
	struct lua_state_t *state = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();

	plua_init();
	plua_overwrite_print();
	plua_pause_coverage(1);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		coap.send({}); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = '201'; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = '1'; \
		send['code'] = 201; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = '1'; \
		send['t'] = 1; \
		send['code'] = 201; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['token'] = 1; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['payload'] = 1; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['options'] = {}; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['options'] = 1; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['options'] = {}; \
		send['options'][0] = 1; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['options'] = {}; \
		send['options'][0]['val'] = '1'; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['options'] = {}; \
		send['options'][0]['num'] = '1'; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['options'] = {}; \
		send['options'][0]['num'] = '1'; \
		send['options'][0]['val'] = '1'; \
		coap.send(send); \
	"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, " \
		local coap = pilight.network.coap(); \
		local send = {}; \
		send['ver'] = 1; \
		send['t'] = 1; \
		send['code'] = 201; \
		send['options'] = {}; \
		send['options'][0]['num'] = 1; \
		send['options'][0]['val'] = 1; \
		coap.send(send); \
	"));

	while(lua_gettop(state->L) > 0) {
		lua_remove(state->L, -1);
	}
	plua_clear_state(state);

	plua_pause_coverage(0);
	plua_gc();
	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_coap_tx(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();

	plua_init();
	plua_overwrite_print();
	plua_pause_coverage(1);

	coap_server_tx();
	uv_thread_create(&pth, coap_wait, NULL);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_network_coap.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%scoap.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("coap", UNITTEST));

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
	 * timeout of the COAP library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	eventpool_init(EVENTPOOL_NO_THREADS);

	sprintf(name, "unittest.%s", "coap");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("coap", tmp->name) == 0) {
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

	coap_loop = 0;
	uv_thread_join(&pth);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_pause_coverage(0);
	plua_gc();
	coap_gc();

	CuAssertIntEquals(tc, 13, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_coap_rx(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 1;

	gtc = tc;
	memtrack();

	plua_init();
	plua_overwrite_print();
	plua_pause_coverage(1);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_network_coap.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%scoap1.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("coap", UNITTEST));

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
	 * timeout of the COAP library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	eventpool_init(EVENTPOOL_NO_THREADS);

	sprintf(name, "unittest.%s", "coap");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("coap", tmp->name) == 0) {
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

	coap_server_rx();

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_pause_coverage(0);
	plua_gc();
	coap_gc();

	CuAssertIntEquals(tc, 13, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_coap_nonexisting_callback(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();

	plua_init();
	plua_overwrite_print();
	plua_pause_coverage(1);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_network_coap.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%scoap2.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("coap", UNITTEST));

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 100, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "coap");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("coap", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, -1, call(L, file, "run"));

	lua_pop(L, -1);

	plua_clear_state(state);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_pause_coverage(0);
	plua_gc();
	CuAssertIntEquals(tc, 2, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_network_coap(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_network_coap_missing_parameters);
	SUITE_ADD_TEST(suite, test_lua_network_coap_invalid_packet);
	SUITE_ADD_TEST(suite, test_lua_network_coap_tx);
	SUITE_ADD_TEST(suite, test_lua_network_coap_rx);
	SUITE_ADD_TEST(suite, test_lua_network_coap_nonexisting_callback);

	return suite;
}
