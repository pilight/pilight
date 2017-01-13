#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/network.h"
#include "../libs/mbedtls/mbedtls/sha256.h"

#include "alltests.h"
#include "gplv3.h"

#define NRSUITS 128

CuSuite *suite_common(void);
CuSuite *suite_network(void);
CuSuite *suite_binary(void);
CuSuite *suite_datetime(void);
CuSuite *suite_json(void);
CuSuite *suite_sha256cache(void);
CuSuite *suite_strptime(void);
CuSuite *suite_dso(void);
CuSuite *suite_options(void);
CuSuite *suite_eventpool(void);
CuSuite *suite_ssdp(void);
CuSuite *suite_ntp(void);
CuSuite *suite_http(void);
CuSuite *suite_ping(void);
CuSuite *suite_mail(void);
CuSuite *suite_webserver(void);
CuSuite *suite_socket(void);
CuSuite *suite_log(void);
CuSuite *suite_protocols_433(void);
CuSuite *suite_protocols_api(void);
CuSuite *suite_protocols_api_openweathermap(void);
CuSuite *suite_protocols_api_wunderground(void);
CuSuite *suite_protocols_api_lirc(void);
CuSuite *suite_protocols_api_xbmc(void);

CuString *output = NULL;
CuSuite *suite = NULL;
CuSuite *suites[NRSUITS];

static unsigned char passout[33];
static char converted[65];
static char password[65] = "test";
static char username[65] = "test";

char whitelist[10][16] = {
	{ "1.1.1.1" },
	{ "10.0.0.140" },
	{ "10.0.0.141" },
	{ "10.0.0.*" },
	{ "10.0.*.*" },
	{ "10.*.*.*" },
	{ "*.*.*.*" },
	{ 0 }
};

int whitelistnr = 0;
int config_enable = 0;
int nr = 0;

int settings_select_string(enum origin_t a, char *b, char **c) {
	// printf("settings_select_string: %s\n", b);
	if(strcmp(b, "pem-file") == 0) {
		*c = "../res/pilight.pem";
		return 0;
	} else if(strcmp(b, "name") == 0) {
		*c = "Test";
		return 0;
	} else if(strcmp(b, "whitelist") == 0) {
		*c = whitelist[whitelistnr];
		return 0;
	} else if(strcmp(b, "webserver-root") == 0) {
		*c = "../libs/webgui/";
		return 0;
	} else if(strcmp(b, "pem-file") == 0) {
		*c = "../res/pilight.pem";
		return 0;
	}
	return -1;
}

int settings_select_number(enum origin_t a, char *b, double *c) {
	if(strcmp(b, "webserver-enable") == 0) {
		*c = 1;
		return 0;
	} else if(strcmp(b, "webserver-http-port") == 0) {
		*c = 10080;
		return 0;
	} else if(strcmp(b, "webserver-https-port") == 0) {
		*c = 10443;
		return 0;
	}
	return -1;
}

int settings_select_string_element(enum origin_t a, char *b, int c, char **d) {
	int i = 0, x = 0;
	mbedtls_sha256_context ctx;

	if((config_enable & CONFIG_AUTH) == CONFIG_AUTH && strcmp("webserver-authentication", b) == 0) {
		if(c == 0) {
			*d = username;
			return 0;
		} else if(c == 1) {			
			for(i=0;i<SHA256_ITERATIONS;i++) {
				mbedtls_sha256_init(&ctx);
				mbedtls_sha256_starts(&ctx, 0);
				mbedtls_sha256_update(&ctx, (unsigned char *)password, strlen((char *)password));
				mbedtls_sha256_finish(&ctx, passout);
				for(x=0;x<64;x+=2) {
					sprintf(&password[x], "%02x", passout[x/2] );
				}
				mbedtls_sha256_free(&ctx);
			}

			for(x=0;x<64;x+=2) {
				sprintf(&converted[x], "%02x", passout[x/2] );
			}

			*d = converted;
			mbedtls_sha256_free(&ctx);
			return 0;
		}
	}
	return -1;
}

// int socket_get_port(void) {
	// return 1234;
// }

// int dev2ip(char *a, char **b, sa_family_t c) {
	// // printf("devip: %s\n", a);
	// strcpy(*b, "123.123.123.123");
	// return 0;
// }

// void _logprintf(int prio, char *file, int line, const char *str, ...) {
	// va_list ap;
	// char buffer[1024];

	// memset(buffer, 0, 1024);
	
	// va_start(ap, str);
	// vsnprintf(buffer, 1024, str, ap);
	// va_end(ap);

	// printf("(%s #%d) %s\n", file, line, buffer);
// }

int RunAllTests(void) {
	int i = 0;
	output = CuStringNew();
	suite = CuSuiteNew();

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
	
	suites[nr++] = suite_common();
	suites[nr++] = suite_network();
	suites[nr++] = suite_binary();
	suites[nr++] = suite_datetime();
	suites[nr++] = suite_json();
	suites[nr++] = suite_sha256cache();
	suites[nr++] = suite_strptime();
	suites[nr++] = suite_options();
	suites[nr++] = suite_dso();
	suites[nr++] = suite_eventpool();
	suites[nr++] = suite_log();
	/*
	 * FIXME:
	 * When SSDP is started before ping
	 * ping will not work properly.
	 */
	suites[nr++] = suite_ping();
	suites[nr++] = suite_ssdp();
	suites[nr++] = suite_ntp();
	suites[nr++] = suite_http();
	suites[nr++] = suite_mail();
	suites[nr++] = suite_webserver();
	suites[nr++] = suite_socket();
	suites[nr++] = suite_protocols_433();
	suites[nr++] = suite_protocols_api();
	suites[nr++] = suite_protocols_api_lirc();
	suites[nr++] = suite_protocols_api_xbmc();
	suites[nr++] = suite_protocols_api_openweathermap();
	suites[nr++] = suite_protocols_api_wunderground();

	for(i=0;i<nr;i++) {
		CuSuiteAddSuite(suite, suites[i]);
	}

	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);

	int r = (suite->failCount == 0) ? 0 : -1;

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

	return RunAllTests();
}
