/*
	Copyright (C) 2013 - 2016 CurlyMo

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
#include "../libs/pilight/lua/lua.h"
#include "../libs/pilight/lua/lualibrary.h"

static int run = 0;
static int test = 0;
static int check = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static struct tests_t {
	char *desc;
	int port;
	int ssl;
	int status;
	int recvmsgnr;
	int sendmsgnr;
	char sendmsg[12][1024];
	char recvmsg[12][1024];
} tests = {
	"gmail plain", 10025, 0, 0, 0, 0, {
		"220 smtp.gmail.com ESMTP im3sm19207876wjb.13 - gsmtp\r\n",
		"250-smtp.gmail.com at your service, [108.177.96.109]\r\n"
			"250-SIZE 35882577\r\n"
			"250-8BITMIME\r\n"
			"250-AUTH LOGIN PLAIN XOAUTH2 PLAIN-CLIENTTOKEN OAUTHBEARER XOAUTH\r\n"
			"250-ENHANCEDSTATUSCODES\r\n"
			"250-PIPELINING\r\n"
			"250-CHUNKING\r\n"
			"250 SMTPUTF8\r\n",
		"235 2.7.0 Accepted\r\n",
		"250 2.1.0 OK im3sm19207876wjb.13 - gsmtp",
		"250 2.1.5 OK im3sm19207876wjb.13 - gsmtp",
		"354  Go ahead im3sm19207876wjb.13 - gsmtp",
		"250 2.0.0 OK 1477744237 im3sm19207876wjb.13 - gsmtp",
		"250 2.1.5 Flushed im3sm19207876wjb.13 - gsmtp",
		"221 2.0.0 closing connection im3sm19207876wjb.13 - gsmtp"
	},
	{
		"EHLO pilight\r\n",
		"AUTH PLAIN AHBpbGlnaHQAdGVzdA==\r\n",
		"MAIL FROM: <order@pilight.org>\r\n",
		"RCPT TO: <info@pilight.nl>\r\n",
		"DATA\r\n",
		"Subject: foo\r\n"
			"From: <order@pilight.org>\r\n"
			"To: <info@pilight.nl>\r\n"
			"Content-Type: text/plain\r\n"
			"Mime-Version: 1.0\r\n"
			"X-Mailer: Emoticode smtp_send\r\n"
			"Content-Transfer-Encoding: 7bit\r\n"
			"\r\n"
			"bar\r\n"
			".\r\n",
		"RSET\r\n",
		"QUIT\r\n"
	}
};

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void stop(uv_timer_t *handle) {
	uv_close((uv_handle_t *)handle, close_cb);

	uv_stop(uv_default_loop());
}

static void alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	buf->base = malloc(len);
	CuAssertPtrNotNull(gtc, buf->base);
	memset(buf->base, 0, len);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

static void write_cb(uv_write_t *req, int status) {
	uv_tcp_t *client_req = req->data;
	CuAssertIntEquals(gtc, 0, status);

	if(check == 0) {
		int r = uv_read_start((uv_stream_t *)client_req, alloc, read_cb);
		CuAssertIntEquals(gtc, 0, r);
	}
	FREE(req);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	uv_read_stop(stream);
	buf->base[nread] = '\0';

	uv_os_fd_t fd = 0;
	int r = uv_fileno((uv_handle_t *)stream, &fd);
	CuAssertIntEquals(gtc, 0, r);

	CuAssertStrEquals(gtc, tests.recvmsg[tests.recvmsgnr++], buf->base);

	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	uv_buf_t buf1;
	buf1.base = tests.sendmsg[tests.sendmsgnr];
	buf1.len = strlen(tests.sendmsg[tests.sendmsgnr]);

	tests.sendmsgnr++;
	write_req->data = stream;

	r = uv_write(write_req, stream, &buf1, 1, write_cb);
	CuAssertIntEquals(gtc, 0, r);

	if(strcmp(buf->base, "QUIT\r\n") == 0) {
		uv_close((uv_handle_t *)stream, close_cb);

		check = 1;
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif

		if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		uv_timer_init(uv_default_loop(), timer_req);
		uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 100, 0);
	}
	free(buf->base);
}

static void connection_cb(uv_stream_t *server_req, int status) {
	uv_tcp_t *client_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, client_req);
	CuAssertIntEquals(gtc, 0, status);
	uv_tcp_init(uv_default_loop(), client_req);

	int r = uv_accept(server_req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	uv_buf_t buf1;
	buf1.base = tests.sendmsg[tests.sendmsgnr];
	buf1.len = strlen(tests.sendmsg[tests.sendmsgnr]);
	tests.sendmsgnr++;
	write_req->data = client_req;
	r = uv_write(write_req, (uv_stream_t *)client_req, &buf1, 1, write_cb);
	CuAssertIntEquals(gtc, 0, r);
}

static void mail_start(int port) {
	/*
	 * Bypass socket_connect
	 */
#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif

	uv_tcp_t *server_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, server_req);

	struct sockaddr_in addr;
	int r = uv_ip4_addr("127.0.0.1", port, &addr);
	CuAssertIntEquals(gtc, r, 0);

	uv_tcp_init(uv_default_loop(), server_req);
	uv_tcp_bind(server_req, (const struct sockaddr *)&addr, 0);

	r = uv_listen((uv_stream_t *)server_req, 128, connection_cb);
	CuAssertIntEquals(gtc, r, 0);
}

static int plua_print(lua_State* L) {
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
		case 2:
		case 4:
		case 6:
		case 8:
		case 10:
		case 12:
		case 14:
		case 16:
		case 18:
		case 23:
		case 24: {
			CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
			CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
			run++;
		} break;
		case 3: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "foo", lua_tostring(L, -1));
			run++;
		} break;
		case 5: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "order@pilight.org", lua_tostring(L, -1));
			run++;
		} break;
		case 7: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "info@pilight.nl", lua_tostring(L, -1));
			run++;
		} break;
		case 9: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "bar", lua_tostring(L, -1));
			run++;
		} break;
		case 11: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "127.0.0.1", lua_tostring(L, -1));
			run++;
		} break;
		case 13: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 10025, lua_tonumber(L, -1));
			run++;
		} break;
		case 15: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "pilight", lua_tostring(L, -1));
			run++;
		} break;
		case 17: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "test", lua_tostring(L, -1));
			run++;
		} break;
		case 19: {
			CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 20: {
			CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
			CuAssertIntEquals(gtc, 0, lua_tonumber(L, -1));
			run++;
		} break;
		case 21: {
			CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
			CuAssertStrEquals(gtc, "callback", lua_tostring(L, -1));
			run++;
		} break;
		case 22: {
			CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
			run++;
		} break;
	}

	return 0;
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

static void test_lua_network_mail_missing_parameters(CuTest *tc) {
	struct lua_state_t *state = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 0;

	gtc = tc;
	memtrack();

	plua_init();
	plua_overwrite_print();

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.callback();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setCallback(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setTo();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setTo(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setFrom();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setFrom(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setSubject();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setSubject(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setMessage();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setMessage(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setSSL();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setSSL(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setPassword();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setPassword(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setUser();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setUser(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setHost();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setHost(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setPort();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.setPort(\"a\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.send();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local mail = pilight.network.mail(); mail.send(\"foo\");"));

	uv_mutex_unlock(&state->lock);

	plua_gc();
	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_mail(CuTest *tc) {
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

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_network_mail.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%smail.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("sendmail", UNITTEST));

	uv_mutex_unlock(&state->lock);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	eventpool_init(EVENTPOOL_NO_THREADS);

	mail_start(10025);

	sprintf(name, "unittest.%s", "sendmail");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("sendmail", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));

	lua_pop(L, -1);

	uv_mutex_unlock(&state->lock);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_gc();
	CuAssertIntEquals(tc, 25, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_mail_nonexisting_callback(CuTest *tc) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	run = 0;
	test = 2;

	gtc = tc;
	memtrack();

	plua_init();
	plua_overwrite_print();

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_network_mail.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%smail1.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("sendmail", UNITTEST));

	uv_mutex_unlock(&state->lock);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "sendmail");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("sendmail", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, -1, call(L, file, "run"));

	lua_pop(L, -1);

	uv_mutex_unlock(&state->lock);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_gc();
	CuAssertIntEquals(tc, 20, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_network_mail(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_network_mail_missing_parameters);
	SUITE_ADD_TEST(suite, test_lua_network_mail);
	SUITE_ADD_TEST(suite, test_lua_network_mail_nonexisting_callback);

	return suite;
}
