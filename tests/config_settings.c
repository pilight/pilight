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
#include "../libs/pilight/core/binary.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/config/settings.h"

#include "alltests.h"

#include <wiringx.h>

struct settings_t {
	struct {
		char *key;
		char value[1024];
		int haspath;
		int type;
	} pairs[10];
	int nrpairs;
	int status;
} settings[] = {
	{
		{
			{ "port", "5000", 0, JSON_NUMBER },
		}, 1, 0
	},
	{
		{
			{ "port", "-1", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "port", "a", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "port", "5001", 0, JSON_NUMBER },
			{ "webserver-http-port", "5001", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "port", "5001", 0, JSON_NUMBER },
			{ "webserver-https-port", "5001", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "webserver-http-port", "5001", 0, JSON_NUMBER },
			{ "webserver-https-port", "5001", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "port", "5001", 0, JSON_NUMBER },
			{ "webserver-http-port", "80", 0, JSON_NUMBER },
			{ "webserver-https-port", "443", 0, JSON_NUMBER },
		}, 3, 0
	},
	{
		{
			{ "arp-interval", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "arp-interval", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "arp-interval", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "arp-interval", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "arp-timeout", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "arp-timeout", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "arp-timeout", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "arp-timeout", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "standalone", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "standalone", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "standalone", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "standalone", "2", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "standalone", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "watchdog-enable", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "watchdog-enable", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "watchdog-enable", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "watchdog-enable", "2", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "watchdog-enable", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "stats-enable", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "stats-enable", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "stats-enable", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "stats-enable", "2", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "stats-enable", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "loopback", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "loopback", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "loopback", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "loopback", "2", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "loopback", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "webserver-enable", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "webserver-enable", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "webserver-enable", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "webserver-enable", "2", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "webserver-enable", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "webserver-cache", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "webserver-cache", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "webserver-cache", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "webserver-cache", "2", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "webgui-websockets", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "webgui-websockets", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "webgui-websockets", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "webgui-websockets", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "webgui-websockets", "2", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "webgui-websockets", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "webserver-authentication", "[\"foo\", \"bar\"]", 0, JSON_ARRAY }
		}, 1, 0
	},
	{
		{
			{ "webserver-authentication", "[\"bar\"]", 0, JSON_ARRAY }
		}, 1, -1
	},
	{
		{
			{ "webserver-authentication", "[\"foo\", \"bar\", \"foo\"]", 0, JSON_ARRAY }
		}, 1, -1
	},
	{
		{
			{ "webserver-authentication", "[1, \"bar\"]", 0, JSON_ARRAY }
		}, 1, -1
	},
	{
		{
			{ "webserver-authentication", "[\"foo\", 2]", 0, JSON_ARRAY }
		}, 1, -1
	},
	{
		{
			{ "webserver-authentication", "1", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "webserver-authentication", "foo, bar", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "log-level", "1", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "log-level", "0", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "log-level", "-1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "log-level", "2", 0, JSON_NUMBER }
		}, 1, 0
	},
	{
		{
			{ "log-level", "a", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "ntp-servers", "[\"foo\", \"bar\"]", 0, JSON_ARRAY }
		}, 1, 0
	},
	{
		{
			{ "ntp-servers", "[\"bar\"]", 0, JSON_ARRAY }
		}, 1, 0
	},
	{
		{
			{ "ntp-servers", "[\"foo\", \"bar\", \"foo\"]", 0, JSON_ARRAY }
		}, 1, 0
	},
	{
		{
			{ "ntp-servers", "[1, \"bar\"]", 0, JSON_ARRAY }
		}, 1, -1
	},
	{
		{
			{ "ntp-servers", "[\"foo\", 2]", 0, JSON_ARRAY }
		}, 1, -1
	},
	{
		{
			{ "ntp-servers", "1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "smtp-host", "foo", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "smtp-host", "1", 0, JSON_NUMBER }
		}, 1, -1
	},
	{
		{
			{ "whitelist", "10.0.0.138, 192.168.0.12", 0, JSON_STRING }
		}, 1, 0
	},
	{
		{
			{ "whitelist", "10.0.0.138", 0, JSON_STRING }
		}, 1, 0
	},
	{
		{
			{ "whitelist", "10.0.0.138, 192.168.0.12, 1050:0:0:0:5:600:300c:326b", 0, JSON_STRING }
		}, 1, 0
	},
	{
		{
			{ "whitelist", "fe80::202:b3ff::fe1e:8329", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "whitelist", "fe80:0000:0000:0000:0202:b3ff:fe1e:8329:abcd", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "whitelist", "1, bar", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "whitelist", "foo, 2", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "whitelist", "foo, bar", 0, JSON_STRING }
		}, 1, -1
	},
	{
		{
			{ "storage-root", "../libs/pilight/config/", 1, JSON_STRING }
		}, 1,	0
	},
	{
		{
			{ "storage-root", "/foo/", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "protocol-root", "../libs/pilight/config/", 1, JSON_STRING }
		}, 1,	0
	},
	{
		{
			{ "protocol-root", "/foo/", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "hardware-root", "../libs/pilight/config/", 1, JSON_STRING }
		}, 1,	0
	},
	{
		{
			{ "hardware-root", "/foo/", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "actions-root", "../libs/pilight/config/", 1, JSON_STRING }
		}, 1,	0
	},
	{
		{
			{ "actions-root", "/foo/", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "functions-root", "../libs/pilight/config/", 1, JSON_STRING }
		}, 1,	0
	},
	{
		{
			{ "functions-root", "/foo/", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "operators-root", "../libs/pilight/config/", 1, JSON_STRING }
		}, 1,	0
	},
	{
		{
			{ "operators-root", "/foo/", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-sender", "info@pilight.org", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-sender", "info@pilight", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-sender", "info@pilight.", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-sender", "info", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-sender", "info@@pilight.org", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-sender", "1", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "smtp-port", "0", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "smtp-port", "-1", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "smtp-port", "a", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-port", "b", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "adhoc-mode", "server", 0, JSON_STRING },
		}, 1, 0
	},
	{
		{
			{ "adhoc-mode", "client", 0, JSON_STRING },
		}, 1, 0
	},
	{
		{
			{ "adhoc-mode", "foo", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "name", "foo", 0, JSON_STRING },
		}, 1, 0
	},
	{
		{
			{ "name", "1", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "name", "01234567890ABCDEFGH", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "name", "", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "adhoc-master", "foo", 0, JSON_STRING },
		}, 1, 0
	},
	{
		{
			{ "adhoc-master", "1", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "adhoc-master", "01234567890ABCDEFGH", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "adhoc-master", "", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-user", "foo", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-user", "1", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "smtp-user", "", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-password", "foo", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-password", "1", 0, JSON_NUMBER },
		}, 1, -1
	},
	{
		{
			{ "smtp-password", "", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "smtp-sender", "info@pilight.org", 0, JSON_STRING },
			{ "smtp-port", "25", 0, JSON_NUMBER },
			{ "smtp-host", "smtp.pilight.org", 0, JSON_STRING },
			{ "smtp-password", "foobar", 0, JSON_STRING },
			{ "smtp-user", "pilight", 0, JSON_STRING },
		}, 5, 0
	},
	{
		{
			{ "gpio-platform", "", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "gpio-platform", "foo", 0, JSON_STRING },
		}, 1, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
		}, 1, 0
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "99", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "1", 0, JSON_NUMBER },
		}, 2, 0
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "a", 0, JSON_STRING },
		}, 2, -1
	},
	{ /* GPIO 10 is the default for SCK */
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "14", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "1", 0, JSON_NUMBER },
			{ "firmware-gpio-sck", "1", 0, JSON_NUMBER },
		}, 3, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-sck", "99", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-sck", "1", 0, JSON_NUMBER },
		}, 2, 0
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-sck", "a", 0, JSON_STRING },
		}, 2, -1
	},
	{ /* GPIO 14 is the default for reset */
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-sck", "10", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "1", 0, JSON_NUMBER },
			{ "firmware-gpio-sck", "1", 0, JSON_NUMBER },
		}, 3, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-mosi", "99", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-mosi", "1", 0, JSON_NUMBER },
		}, 2, 0
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-mosi", "a", 0, JSON_STRING },
		}, 2, -1
	},
	{ /* GPIO 14 is the default for reset */
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-mosi", "14", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "1", 0, JSON_NUMBER },
			{ "firmware-gpio-mosi", "1", 0, JSON_NUMBER },
		}, 3, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-miso", "99", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-miso", "1", 0, JSON_NUMBER },
		}, 2, 0
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-miso", "a", 0, JSON_STRING },
		}, 2, -1
	},
	{ /* GPIO 14 is the default for reset */
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-miso", "14", 0, JSON_NUMBER },
		}, 2, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "1", 0, JSON_NUMBER },
			{ "firmware-gpio-miso", "1", 0, JSON_NUMBER },
		}, 3, -1
	},
	{
		{
			{ "gpio-platform", "gpio-stub", 0, JSON_STRING },
			{ "firmware-gpio-reset", "1", 0, JSON_NUMBER },
			{ "firmware-gpio-miso", "2", 0, JSON_NUMBER },
			{ "firmware-gpio-sck", "3", 0, JSON_NUMBER },
			{ "firmware-gpio-mosi", "4", 0, JSON_NUMBER },
		}, 5, -1
	},
	{
		{
			{ "foo", "bar", 0, JSON_STRING },
		}, 1, -1
	}
};

static void foo(int prio, char *file, int line, const char *format_str, ...) {
}

void test_config_settings(CuTest *tc) {
	if(wiringXSetup("test", foo) != -999) {
		printf("[ %-31.31s (preload libgpio)]\n", __FUNCTION__);
		fflush(stdout);
		wiringXGC();
		return;
	}

	memtrack();
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("config_settings.c", "", &file);

	int len = sizeof(settings)/sizeof(settings[0]);
	int i = 0, y = 0, pos = 0;
	for(i=0;i<len;i++) {
		y = 0; pos = 0;
		char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},\"settings\":{%s},\"hardware\":{},\"registry\":{}}";

		char content[1024] = "";

		for(y=0;y<settings[i].nrpairs;y++) {
			printf("[ %-10s %-3d %-26s ]\n", __FUNCTION__, i, settings[i].pairs[y].key);
			fflush(stdout);
			if(settings[i].pairs[y].type == JSON_STRING) {
				if(settings[i].pairs[y].haspath == 1) {
					pos += snprintf(&content[pos], 1024-pos, "\"%s\":\"%s%s\",", settings[i].pairs[y].key, file, settings[i].pairs[y].value);
				} else {
					pos += snprintf(&content[pos], 1024-pos, "\"%s\":\"%s\",", settings[i].pairs[y].key, settings[i].pairs[y].value);
				}
			} else if(settings[i].pairs[y].type == JSON_NUMBER || settings[i].pairs[y].type == JSON_ARRAY) {
				pos += snprintf(&content[pos], 1024-pos, "\"%s\":%s,", settings[i].pairs[y].key, settings[i].pairs[y].value);
			}
		}
		content[pos-1] = '\0';

		plua_init();

		test_set_plua_path(tc, __FILE__, "config_settings.c");

		config_init();

		FILE *f = fopen("storage_core.json", "w");
		fprintf(f, config, content);
		fclose(f);

		CuAssertIntEquals(tc, settings[i].status, config_read("storage_core.json", CONFIG_SETTINGS));

		if(settings[i].status == 0) {
			for(y=0;y<settings[i].nrpairs;y++) {
				if(settings[i].pairs[y].type == JSON_STRING) {
					char *ret = NULL;
					int x = 0;

					char check[1024] = { 0 };
					if(settings[i].pairs[y].haspath == 1) {
						sprintf(check, "%s%s", file, settings[i].pairs[y].value);
					} else {
						sprintf(check, "%s", settings[i].pairs[y].value);
					}

					CuAssertIntEquals(tc, settings[i].status, config_setting_get_string(settings[i].pairs[y].key, 0, &ret));
					CuAssertPtrNotNull(tc, ret);
					CuAssertStrEquals(tc, check, ret);
					FREE(ret);

					CuAssertIntEquals(tc, -1, config_setting_get_number(settings[i].pairs[y].key, 0, &x));
				} else if(settings[i].pairs[y].type == JSON_NUMBER) {
					char *ret = NULL;
					int x = 0;

					CuAssertIntEquals(tc, settings[i].status, config_setting_get_number(settings[i].pairs[y].key, 0, &x));
					CuAssertIntEquals(tc, atoi(settings[i].pairs[y].value), x);

					CuAssertIntEquals(tc, -1, config_setting_get_string(settings[i].pairs[y].key, 0, &ret));
				} else if(settings[i].pairs[y].type == JSON_ARRAY) {
					int el = 0;
					struct JsonNode *jarray = json_decode(settings[i].pairs[y].value);
					struct JsonNode *jchild = NULL;
					while((jchild = json_find_element(jarray, el)) != NULL) {
						char *ret = NULL;
						int x = 0;
						if(jchild->tag == JSON_NUMBER) {
							CuAssertIntEquals(tc, settings[i].status, config_setting_get_number(settings[i].pairs[y].key, 0, &x));
							CuAssertIntEquals(tc, atoi(settings[i].pairs[y].value), x);

							CuAssertIntEquals(tc, -1, config_setting_get_string(settings[i].pairs[y].key, 0, &ret));
						} else if(jchild->tag == JSON_STRING) {
							CuAssertIntEquals(tc, settings[i].status, config_setting_get_string(settings[i].pairs[y].key, el, &ret));
							CuAssertPtrNotNull(tc, ret);
							CuAssertStrEquals(tc, jchild->string_, ret);
							FREE(ret);

							CuAssertIntEquals(tc, -1, config_setting_get_number(settings[i].pairs[y].key, el, &x));
						}
						el++;
					}

					{
						char *ret = NULL;
						int x = 0;

						CuAssertIntEquals(tc, -1, config_setting_get_string(settings[i].pairs[y].key, el, &ret));
						CuAssertIntEquals(tc, -1, config_setting_get_number(settings[i].pairs[y].key, 0, &x));
					}
					json_delete(jarray);
				}
			}
		}

		config_write();
		config_gc();
		plua_gc();
		wiringXGC();
	}
	FREE(file);

	CuAssertIntEquals(tc, 0, xfree());
}

