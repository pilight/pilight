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
#include "../libs/pilight/core/http.h"
#include "../libs/pilight/lua/lua.h"
#include "../libs/pilight/lua/lualibrary.h"

static int run = 0;
static int test = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static struct tests_t {
	char sendmsg[1024];
	char recvmsg[1024];
} tests[] = {
	{ "HTTP/1.1 200 OK\r\n"
			"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
			"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
			"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
			"ETag: \"d5-4e955b12f4640\"\r\n"
			"Accept-Ranges: bytes\r\n"
			"Content-Length: 12\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html\r\n\r\n"
			"Hello World!",
		"GET / HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Connection: close\r\n\r\n"
	},
	{	"HTTP/1.1 200 OK\r\n"
			"Date: Sun, 30 Oct 2016 15:07:58 GMT\r\n"
			"Server: Apache/2.4.23 (FreeBSD) PHP/5.6.26\r\n"
			"Last-Modified: Tue, 22 Oct 2013 15:02:41 GMT\r\n"
			"ETag: \"d5-4e955b12f4640\"\r\n"
			"Accept-Ranges: bytes\r\n"
			"Content-Length: 13\r\n"
			"Connection: close\r\n"
			"Content-Type: application/json\r\n\r\n"
			"{\"bar\":\"foo\"}",
		"POST /index.php?foo=bar HTTP/1.0\r\n"
			"Host: 127.0.0.1\r\n"
			"User-Agent: pilight\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 13\r\n\r\n"
			"{\"foo\":\"bar\"}"
	}
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
				case 2:
				case 4:
				case 5: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
					run++;
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 200, lua_tonumber(L, -1));
					run++;
				} break;
				case 7: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "Hello World!", lua_tostring(L, -1));
					run++;
				} break;
				case 8: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "http://127.0.0.1:10080/", lua_tostring(L, -1));
					run++;
				} break;
				case 9: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "text/html", lua_tostring(L, -1));
					run++;
				} break;
				case 10: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 12, lua_tonumber(L, -1));
					run++;
					uv_stop(uv_default_loop());
				} break;
			}
		} break;
		case 1: {
			switch(run) {
				case 0:
				case 2:
				case 3:
				case 4:
				case 5: {
					CuAssertIntEquals(gtc, LUA_TBOOLEAN, lua_type(L, -1));
					CuAssertIntEquals(gtc, 1, lua_toboolean(L, -1));
					run++;
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TNIL, lua_type(L, -1));
					run++;
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 200, lua_tonumber(L, -1));
					run++;
				} break;
				case 7: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "{\"bar\":\"foo\"}", lua_tostring(L, -1));
					run++;
				} break;
				case 8: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "http://127.0.0.1:10080/index.php?foo=bar", lua_tostring(L, -1));
					run++;
				} break;
				case 9: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "application/json", lua_tostring(L, -1));
					run++;
				} break;
				case 10: {
					CuAssertIntEquals(gtc, LUA_TNUMBER, lua_type(L, -1));
					CuAssertIntEquals(gtc, 13, lua_tonumber(L, -1));
					run++;
					uv_stop(uv_default_loop());
				} break;
			}
		} break;
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
	FREE(req);
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	uv_read_stop(stream);
	buf->base[nread] = '\0';

	uv_os_fd_t fd = 0;
	int r = uv_fileno((uv_handle_t *)stream, &fd);
	CuAssertIntEquals(gtc, 0, r);

	CuAssertStrEquals(gtc, tests[test].recvmsg, buf->base);

	uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
	CuAssertPtrNotNull(gtc, write_req);

	uv_buf_t buf1;
	buf1.base = tests[test].sendmsg;
	buf1.len = strlen(tests[test].sendmsg);

	write_req->data = stream;

	r = uv_write(write_req, stream, &buf1, 1, write_cb);
	CuAssertIntEquals(gtc, 0, r);

	free(buf->base);
}

static void connection_cb(uv_stream_t *server_req, int status) {
	uv_tcp_t *client_req = MALLOC(sizeof(uv_tcp_t));
	CuAssertPtrNotNull(gtc, client_req);
	CuAssertIntEquals(gtc, 0, status);
	uv_tcp_init(uv_default_loop(), client_req);

	int r = uv_accept(server_req, (uv_stream_t *)client_req);
	CuAssertIntEquals(gtc, 0, r);

	r = uv_read_start((uv_stream_t *)client_req, alloc, read_cb);
	CuAssertIntEquals(gtc, 0, r);
}

static void http_start(int port) {
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

static void test_lua_network_http_missing_parameters(CuTest *tc) {
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
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.callback();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.setCallback(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.setUrl();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.setUrl(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.getCode(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.getData(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.getMimetype(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.setMimetype(1);"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.setMimetype();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.get();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.get(\"foo\");"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.post();"));
	CuAssertIntEquals(tc, 1, luaL_dostring(state->L, "local http = pilight.network.http(); http.post(\"foo\");"));

	uv_mutex_unlock(&state->lock);

	plua_gc();
	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_http_get(CuTest *tc) {
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

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_network_http.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%shttp.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("http", UNITTEST));

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

	http_start(10080);

	sprintf(name, "unittest.%s", "http");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("http", tmp->name) == 0) {
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

	http_gc();
	plua_gc();
	CuAssertIntEquals(tc, 11, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_http_post(CuTest *tc) {
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

	str_replace("lua_network_http.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%shttp1.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("http", UNITTEST));

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

	http_start(10080);

	sprintf(name, "unittest.%s", "http");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("http", tmp->name) == 0) {
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

	http_gc();
	plua_gc();
	CuAssertIntEquals(tc, 11, run);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_lua_network_http_nonexisting_callback(CuTest *tc) {
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

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);

	str_replace("lua_network_http.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%shttp2.lua", file);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("http", UNITTEST));

	uv_mutex_unlock(&state->lock);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 100, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "http");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("http", tmp->name) == 0) {
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
	CuAssertIntEquals(tc, 6, run);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_lua_network_http(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_network_http_missing_parameters);
	SUITE_ADD_TEST(suite, test_lua_network_http_get);
	SUITE_ADD_TEST(suite, test_lua_network_http_post);
	SUITE_ADD_TEST(suite, test_lua_network_http_nonexisting_callback);

	return suite;
}
