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

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/network.h"

#include "alltests.h"

static void test_inet_devs(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char **devs = NULL, line[255];
	int n = inetdevs(&devs), i = 0;
	int l = 0, len = 0;
	CuAssertTrue(tc, (n > 0));

	FILE *f = NULL;
	CuAssertPtrNotNull(tc, (f = fopen("rapport.txt", "a")));

	for(l=0;l<n;l++) {
		CuAssertTrue(tc, (len = sprintf(line, "Network device %d: %s\n", l, devs[i]) > 0));
		CuAssertIntEquals(tc, len, fwrite(line, sizeof(char), len, f));
	}
	fclose(f);

	for(i=0;i<n;i++) {
		FREE(devs[i]);
		CuAssertPtrEquals(tc, NULL, devs[i]);
	}
	FREE(devs);

	CuAssertPtrEquals(tc, NULL, devs);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_dev2mac(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char **devs = NULL, mac[ETH_ALEN], *p = mac, tmp[255], line[255];
	int n = 0, i = 0, len = 0, r = 0;
	FILE *f = NULL;
	CuAssertPtrNotNull(tc, (f = fopen("rapport.txt", "a")));

	n = inetdevs(&devs);
	CuAssertTrue(tc, (n > 0));

	for(i=0;i<n;i++) {
		r = dev2mac(devs[i], &p);
		CuAssertIntEquals(tc, 0, r);

		sprintf(tmp, "%02x:%02x:%02x:%02x:%02x:%02x",
			(unsigned char)p[0], (unsigned char)p[1], (unsigned char)p[2],
			(unsigned char)p[3], (unsigned char)p[4], (unsigned char)p[5]);

		CuAssertTrue(tc, ((len = sprintf(line, "MAC address device %d: %s\n", i, tmp)) > 0));
		CuAssertIntEquals(tc, len, fwrite(line, sizeof(char), len, f));
	}
	fclose(f);

	for(i=0;i<n;i++) {
		FREE(devs[i]);
		CuAssertPtrEquals(tc, NULL, devs[i]);
	}
	FREE(devs);

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_host2ip(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	char *ip = NULL;
	int r = 0;

	r = host2ip("localhost", &ip);
	if(r == AF_INET && ip != NULL) {
		CuAssertTrue(tc, (strcmp(ip, "127.0.1.1") == 0 ||
					strcmp(ip, "127.0.0.1") == 0 ||
					strcmp(ip, "0.0.0.0") == 0));
	} else if(r == AF_INET6) {
		CuAssertTrue(tc, strcmp(ip, "::1") == 0);
	} else {
		CuAssertTrue(tc, 0);
	}
	FREE(ip);

	r = host2ip("www.pilight.org", &ip);
	CuAssertPtrNotNull(tc, ip);
	CuAssertIntEquals(tc, AF_INET, r);
	CuAssertTrue(tc, strcmp(ip, "45.32.148.129") == 0);
	FREE(ip);

	CuAssertIntEquals(tc, 0, xfree());
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void test_whitelist_check(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	{
		FILE *f = fopen("network_whitelist.json", "w");
		fprintf(f,
			"{\"devices\":{},\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"whitelist\":\"1.1.1.1,127.0.0.1\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("network_whitelist.json", CONFIG_SETTINGS));

		/*
		 * When we are unit testing localhost is not allowed by default
		 */
		CuAssertIntEquals(tc, 0, whitelist_check("127.0.0.1"));
		whitelist_free();

		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		storage_gc();
		eventpool_gc();
	}

	{
		FILE *f = fopen("network_whitelist.json", "w");
		fprintf(f,
			"{\"devices\":{},\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"whitelist\":\"10.0.0.140\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("network_whitelist.json", CONFIG_SETTINGS));	

		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.140"));
		whitelist_free();
		CuAssertIntEquals(tc, -1, whitelist_check("10.0.0.141"));
		whitelist_free();

		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		storage_gc();
		eventpool_gc();
	}

	{
		FILE *f = fopen("network_whitelist.json", "w");
		fprintf(f,
			"{\"devices\":{},\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"whitelist\":\"10.0.0.141\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("network_whitelist.json", CONFIG_SETTINGS));	

		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.141"));
		whitelist_free();

		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		storage_gc();
		eventpool_gc();
	}

	{
		FILE *f = fopen("network_whitelist.json", "w");
		fprintf(f,
			"{\"devices\":{},\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"whitelist\":\"10.0.0.*\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("network_whitelist.json", CONFIG_SETTINGS));	

		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.141"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.100"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.254"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.0"));
		CuAssertIntEquals(tc, -1, whitelist_check("192.168.0.1"));
		whitelist_free();

		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		storage_gc();
		eventpool_gc();
	}

	{
		FILE *f = fopen("network_whitelist.json", "w");
		fprintf(f,
			"{\"devices\":{},\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"whitelist\":\"10.0.*.*\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("network_whitelist.json", CONFIG_SETTINGS));	

		CuAssertIntEquals(tc, 0, whitelist_check("10.0.1.141"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.100.100"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.254.254"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.0"));
		CuAssertIntEquals(tc, -1, whitelist_check("192.168.0.1"));
		whitelist_free();

		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		storage_gc();
		eventpool_gc();
	}

	{
		FILE *f = fopen("network_whitelist.json", "w");
		fprintf(f,
			"{\"devices\":{},\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"whitelist\":\"10.*.*.*\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("network_whitelist.json", CONFIG_SETTINGS));	

		CuAssertIntEquals(tc, 0, whitelist_check("10.1.1.141"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.100.100.100"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.254.254.254"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.0"));
		CuAssertIntEquals(tc, -1, whitelist_check("192.168.0.1"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.161.13.1"));
		whitelist_free();

		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		storage_gc();
		eventpool_gc();
	}

	{
		FILE *f = fopen("network_whitelist.json", "w");
		fprintf(f,
			"{\"devices\":{},\"gui\":{},\"rules\":{},"\
			"\"settings\":{\"whitelist\":\"*.*.*.*\"},"\
			"\"hardware\":{},\"registry\":{}}"
		);
		fclose(f);

		eventpool_init(EVENTPOOL_NO_THREADS);
		storage_init();
		CuAssertIntEquals(tc, 0, storage_read("network_whitelist.json", CONFIG_SETTINGS));	

		CuAssertIntEquals(tc, 0, whitelist_check("10.1.1.141"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.100.100.100"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.254.254.254"));
		CuAssertIntEquals(tc, 0, whitelist_check("10.0.0.0"));
		CuAssertIntEquals(tc, 0, whitelist_check("192.168.0.1"));
		CuAssertIntEquals(tc, 0, whitelist_check("11.161.13.1"));
		CuAssertIntEquals(tc, 0, whitelist_check("11.12.13.14"));
		whitelist_free();

		uv_run(uv_default_loop(), UV_RUN_NOWAIT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		storage_gc();
		eventpool_gc();
	}

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_network(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_inet_devs);
	SUITE_ADD_TEST(suite, test_dev2mac);
	SUITE_ADD_TEST(suite, test_host2ip);
	SUITE_ADD_TEST(suite, test_whitelist_check);

	return suite;
}
