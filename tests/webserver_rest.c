/*
	Copyright (C) 2013 - 2016 CurlyMo

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
#include <mbedtls/sha256.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/webserver.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/http.h"
#include "../libs/pilight/core/ssl.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/socket.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/config/config.h"

#include "alltests.h"

#define CORE				0
#define REGISTRY		1

static int run = CORE;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static int check = 0;
static uv_async_t *async_close_req = NULL;
static int testnr = 0;

static struct tests_t {
	char *desc;
	char *url;
	int code;
	int size;
	char *type;
	char *data;
} tests[] = {
	{ "get existing key", "http://127.0.0.1:10080/registry?get=foo", 200, 48, "application/json",
		"{\"message\":\"registry\",\"value\":\"bar\",\"key\":\"foo\"}"
	},
	{ "get nonexisting key", "http://127.0.0.1:10080/registry?get=bar", 200, 20, "application/json",
		"{\"message\":\"failed\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry?set=bar&value=foo", 200, 21, "application/json",
		"{\"message\":\"success\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry?remove=foo", 200, 21, "application/json",
		"{\"message\":\"success\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry?remove=tmp", 200, 20, "application/json",
		"{\"message\":\"failed\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry?set=bar&value=foo", 200, 21, "application/json",
		"{\"message\":\"success\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry?remove=bar&bar=1", 200, 21, "application/json",
		"{\"message\":\"success\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry?foo", 200, 20, "application/json",
		"{\"message\":\"failed\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry", 200, 20, "application/json",
		"{\"message\":\"failed\"}"
	},
	{ "set key", "http://127.0.0.1:10080/registry?set=bar&value=foo", 200, 21, "application/json",
		"{\"message\":\"success\"}"
	},
};

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void stop(uv_timer_t *req) {
	uv_async_send(async_close_req);
}

static void async_close_cb(uv_async_t *handle) {
	webserver_gc();
	whitelist_free();
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_stop(uv_default_loop());
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void on_read(int fd, char *buf, ssize_t len, char **buf1, ssize_t *len1) {
	if(strstr(buf, "404 Not Found") != NULL) {
		check = 1;
	}

	return;
}

static void *socket_connected(int reason, void *param, void *userdata) {
	struct reason_socket_connected_t *data = param;

	char *out = "{\"action\": \"identify\", \"options\": {\"core\": 0, \"receiver\": 1, \"config\": 0, \"forward\": 0}}";
	socket_write(data->fd, out);

	return NULL;
}

static void callback(int code, char *data, int size, char *type, void *userdata) {
	CuAssertIntEquals(gtc, code, tests[testnr].code);
	CuAssertIntEquals(gtc, size, tests[testnr].size);
	CuAssertStrEquals(gtc, data, tests[testnr].data);
	if(tests[testnr].type != NULL) {
		CuAssertStrEquals(gtc, type, tests[testnr].type);
	}

	testnr++;

	if(testnr < sizeof(tests)/sizeof(tests[0])) {
		http_get_content(tests[testnr].url, callback, NULL);
	} else {
		check = 1;
		uv_async_send(async_close_req);
	}
}

static void test_webserver(CuTest *tc) {
	memtrack();

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	if(run == CORE) {
		timer_req = MALLOC(sizeof(uv_timer_t));
		CuAssertPtrNotNull(tc, timer_req);
		uv_timer_init(uv_default_loop(), timer_req);
		uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);
	}

	plua_init();

	test_set_plua_path(tc, __FILE__, "webserver_rest.c");

	config_init();
	CuAssertIntEquals(tc, 0, config_read("webserver.json", CONFIG_SETTINGS | CONFIG_REGISTRY));

	ssl_init();

	eventpool_init(EVENTPOOL_NO_THREADS);
	webserver_start();

	if(run == CORE) {
		eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected, NULL);
		socket_connect("127.0.0.1", 10080, on_read);
	} else if(run == REGISTRY) {
		http_get_content(tests[testnr].url, callback, NULL);
	}

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}
}

static void test_webserver_core_api(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\",\"webserver-root\":\"%s../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443},"\
		"\"hardware\":{},\"registry\":{}}";

	char *file = strdup(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("webserver_rest.c", "", &file);

	FILE *f = fopen("webserver.json", "w");
	fprintf(f, config, file, file);
	fclose(f);
	strdup(file);

	gtc = tc;
	check = 0;

	run = CORE;
	test_webserver(tc);

	ssl_gc();
	plua_gc();
	eventpool_gc();
	storage_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_webserver_rest_registry(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	if(gtc != NULL && gtc->failed == 1) {
		return;
	}

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},"\
		"\"settings\":{\"pem-file\":\"%s../res/pilight.pem\",\"webserver-root\":\"%s../libs/webgui/\","\
		"\"webserver-enable\":1,\"webserver-http-port\":10080,\"webserver-https-port\":10443},"\
		"\"hardware\":{},\"registry\":{\"foo\":\"bar\"}}";

	char *file = strdup(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("webserver_rest.c", "", &file);

	FILE *f = fopen("webserver.json", "w");
	fprintf(f, config, file, file);
	fclose(f);
	strdup(file);

	gtc = tc;
	check = 0;
	testnr = 0;

	run = REGISTRY;
	test_webserver(tc);

	char *registry = config_callback_write("registry");
	if(registry != NULL) {
		CuAssertStrEquals(tc, registry, "{\n\t\"bar\": \"foo\"\n}");
		FREE(registry);
	}

	ssl_gc();
	plua_gc();
	eventpool_gc();
	storage_gc();
	config_gc();

	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_webserver_rest(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_webserver_core_api);
	SUITE_ADD_TEST(suite, test_webserver_rest_registry);

	return suite;
}
