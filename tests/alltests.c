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
#include <mbedtls/sha256.h>
#include <valgrind/valgrind.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"
#include "gplv3.h"

#define NRSUITS 128

static int dolist = 0;

CuSuite *suite_common(void);
CuSuite *suite_network(void);
CuSuite *suite_binary(void);
CuSuite *suite_proc(void);
CuSuite *suite_datetime(void);
CuSuite *suite_json(void);
CuSuite *suite_cast(void);
CuSuite *suite_sha256cache(void);
CuSuite *suite_strptime(void);
CuSuite *suite_dso(void);
CuSuite *suite_options(void);
CuSuite *suite_eventpool(void);
CuSuite *suite_ssdp(void);
CuSuite *suite_ntp(void);
CuSuite *suite_arp(void);
CuSuite *suite_http(void);
CuSuite *suite_ping(void);
CuSuite *suite_mail(void);
#ifdef WEBSERVER
CuSuite *suite_webserver(void);
CuSuite *suite_webserver_rest(void);
#endif
CuSuite *suite_socket(void);
CuSuite *suite_log(void);
CuSuite *suite_lua_cast(void);
CuSuite *suite_lua_datetime(void);
CuSuite *suite_lua_c_metatable(void);
CuSuite *suite_lua_common(void);
CuSuite *suite_lua_async_thread(void);
CuSuite *suite_lua_async_timer(void);
CuSuite *suite_lua_network_mail(void);
CuSuite *suite_lua_network_http(void);
CuSuite *suite_lua_config(void);
CuSuite *suite_protocols_433(void);
CuSuite *suite_protocols_api(void);
CuSuite *suite_protocols_api_openweathermap(void);
CuSuite *suite_protocols_api_wunderground(void);
CuSuite *suite_protocols_api_lirc(void);
CuSuite *suite_protocols_api_xbmc(void);
CuSuite *suite_protocols_network_ping(void);
CuSuite *suite_protocols_network_arping(void);
CuSuite *suite_protocols_core(void);
CuSuite *suite_protocols_generic(void);
CuSuite *suite_protocols_i2c(void);
CuSuite *suite_protocols_gpio_ds18x20(void);
CuSuite *suite_protocols_gpio_switch(void);
CuSuite *suite_protocols_relay(void);
CuSuite *suite_hardware_433gpio(void);
CuSuite *suite_event_operators(void);
CuSuite *suite_event_functions(void);
CuSuite *suite_event_actions_switch(void);
CuSuite *suite_event_actions_toggle(void);
CuSuite *suite_event_actions_label(void);
CuSuite *suite_event_actions_dim(void);
CuSuite *suite_event_actions_mail(void);
CuSuite *suite_event_actions_pushbullet(void);
CuSuite *suite_events(void);

CuString *output = NULL;
CuSuite *suite = NULL;
CuSuite *suites[NRSUITS];

const uv_thread_t pth_main_id;
int nr = 0;

/*void _logprintf(int prio, char *file, int line, const char *str, ...) {
	va_list ap;
	char buffer[1024];

	memset(buffer, 0, 1024);

	va_start(ap, str);
	vsnprintf(buffer, 1024, str, ap);
	va_end(ap);

	printf("(%s #%d) %s\n", file, line, buffer);
}*/

int suiteFailed(void) {
	int i = 0;
	for(i=0;i<suite->count;i++) {
		if(suite->list[i]->failed == 1) {
			return 1;
		}
	}
	return 0;
}

int RunAllTests(void) {
	int i = 0;
	output = CuStringNew();
	suite = CuSuiteNew();

	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	memtrack();

	/*
	 * Logging is asynchronous. If the log is being filled
	 * when a test is already finished, xfree() is called
	 * which also garbage collects all open pointers.
	 *
	 * If the log is still trying to process everything
	 * you get free calls on already freed pointers.
	 * Logging should therefor be disabled for all tests
	 * except the log test itself. Which will enable and
	 * disable logging for proper testing.
	 */
	log_file_disable();
	log_shell_disable();

	/*
	 * Storage should be tested first
	 */
	suites[nr++] = suite_common();
	suites[nr++] = suite_network();
	suites[nr++] = suite_binary(); // Ported (Missing signed tests)
	suites[nr++] = suite_proc();
	suites[nr++] = suite_datetime(); // Ported
	suites[nr++] = suite_json(); // Ported
	suites[nr++] = suite_cast(); // Ported
	suites[nr++] = suite_sha256cache();
	suites[nr++] = suite_strptime(); // Ported
	suites[nr++] = suite_options();
	suites[nr++] = suite_dso(); // Fix the dll creation
	suites[nr++] = suite_eventpool(); // Ported
	suites[nr++] = suite_log();
	suites[nr++] = suite_ssdp();
	suites[nr++] = suite_ping();
	suites[nr++] = suite_ntp(); // ipv4 ported / ipv6 not ported
	suites[nr++] = suite_arp();
	suites[nr++] = suite_http(); // ipv4 ported / ipv6 not ported
	suites[nr++] = suite_mail(); // ipv4 ported / ipv6 not ported
	suites[nr++] = suite_lua_cast(); // Ported
	suites[nr++] = suite_lua_c_metatable(); // Ported
	suites[nr++] = suite_lua_datetime(); // Ported
	suites[nr++] = suite_lua_common(); // Ported
	suites[nr++] = suite_lua_async_thread(); // Ported
	suites[nr++] = suite_lua_async_timer(); // Ported
	suites[nr++] = suite_lua_network_mail(); // Ported
	suites[nr++] = suite_lua_network_http();
	suites[nr++] = suite_lua_config(); // Ported
#ifdef WEBSERVER
	suites[nr++] = suite_webserver(); // Ported
	suites[nr++] = suite_webserver_rest();
#endif
	suites[nr++] = suite_socket();
	suites[nr++] = suite_protocols_433();
	suites[nr++] = suite_protocols_api(); // Ported
#ifndef _WIN32
	suites[nr++] = suite_protocols_api_lirc();
#endif
	suites[nr++] = suite_protocols_api_xbmc();
	suites[nr++] = suite_protocols_api_openweathermap(); // Ported
	suites[nr++] = suite_protocols_api_wunderground(); // Ported
	suites[nr++] = suite_protocols_network_ping();
	suites[nr++] = suite_protocols_network_arping();
	suites[nr++] = suite_protocols_core();
	suites[nr++] = suite_protocols_generic();
#ifdef __linux__
	suites[nr++] = suite_protocols_i2c();
	suites[nr++] = suite_protocols_gpio_ds18x20();
	suites[nr++] = suite_protocols_gpio_switch();
	suites[nr++] = suite_hardware_433gpio(); // Ported
#endif
	suites[nr++] = suite_event_operators(); // Ported
	suites[nr++] = suite_event_functions(); // Ported
	suites[nr++] = suite_event_actions_switch(); // Ported
	suites[nr++] = suite_event_actions_toggle(); // Ported
	suites[nr++] = suite_event_actions_label(); // Ported
	suites[nr++] = suite_event_actions_dim(); // Ported
	suites[nr++] = suite_event_actions_mail(); // Ported
	suites[nr++] = suite_event_actions_pushbullet(); // Ported
	suites[nr++] = suite_events(); // Ported

	for(i=0;i<nr;i++) {
		CuSuiteAddSuite(suite, suites[i]);
	}

	int r = 0;
	if(dolist == 0) {
		CuSuiteRun(suite);
		CuSuiteSummary(suite, output);
		CuSuiteDetails(suite, output);
		printf("%s\n", output->buffer);

		r = (suite->failCount == 0) ? 0 : -1;
	} else {
		int i = 0;
		for(i = 0 ; i < suite->count; ++i) {
			CuTest *testCase = suite->list[i];
			printf("[ %-48s ]\n", testCase->name);
		}
	}
	for(i=0;i<nr;i++) {
		free(suites[i]);
	}
	CuSuiteDelete(suite);
	CuStringDelete(output);
	return r;
}

int main(int argc, char **argv) {
	remove("rapport.txt");
	remove("test.log");

	FILE *f = fopen("gplv3.txt", "w");
	fprintf(f, "%s", gplv3);
	fclose(f);

	if(argc == 2) {
		if(strcmp(argv[1], "list") == 0) {
			dolist = 1;
		} else {
			CuSetFilter(argv[1]);
		}
	} else {
		/*
		 * Testing the debug code. If this doesn't work
		 * everything else will fail as well.
		 */
		printf("[ %-48s ]\n", "test_unittest");
		fflush(stdout);
		assert(17 == test_unittest());

		if(!RUNNING_ON_VALGRIND) {
			printf("[ %-48s ]\n", "test_memory");
			fflush(stdout);
			assert(0 == test_memory());
		}
	}

	return RunAllTests();
}
