/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>
#include <sys/utsname.h>

#include "../libs/pilight/core/mem.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/protocols/protocol.h"
#include "../libs/pilight/protocols/GPIO/lm75.h"
#include "../libs/pilight/protocols/GPIO/lm76.h"
#include "../libs/pilight/protocols/GPIO/bmp180.h"
#include "../libs/wiringx/wiringX.h"

#include "alltests.h"

#define MAX_DIR_PATH 2048
#define LM75   0
#define LM76   1
#define BMP180 2

static CuTest *gtc = NULL;
static int step = 0;
static int fd = 0;
static int dev = 0;
static uv_thread_t pth;

int init_module(char *, unsigned long, char *);
int delete_module(char *, int);

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void *done(void *param) {
	json_delete(param);
	return NULL;
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static int findfile(char *path, char *pattern, char **out) {
	DIR *dir = NULL;
	struct dirent *entry;
	char cwd[MAX_DIR_PATH+1];
	char file[MAX_DIR_PATH+1];
	struct stat dir_stat;
	int ret = -1;

	snprintf(cwd, MAX_DIR_PATH, "%s/", path);

	dir = opendir(cwd);
	if(!dir) {
		fprintf(stderr, "Cannot read directory '%s': ", cwd);
		perror("");
		closedir(dir);
		return -1;
	}

	while((entry = readdir(dir))) {
		if(entry->d_name && strstr(entry->d_name, pattern)) {
			snprintf(*out, MAX_DIR_PATH, "%s/%s", cwd, entry->d_name);
			closedir(dir);
			ret = 0;
			return 0;
		}

		snprintf(file, MAX_DIR_PATH, "%s%s", cwd, entry->d_name);

		if(stat(file, &dir_stat) == -1) {
			perror("stat:");
			closedir(dir);
			return -1;
		}

		if(strcmp(entry->d_name, ".") == 0) {
			continue;
		}
		if(strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		if(S_ISDIR(dir_stat.st_mode)) {
			ret = findfile(file, pattern, out);
			if(ret == 0) {
				break;
			}
		}
	}
	closedir(dir);
	return ret;
}

static void *grab_file(const char *filename, unsigned long *size) {
	unsigned int max = 16384;
	int ret, fd, err_save;
	void *buffer;

	fd = open(filename, O_RDONLY, 0);
	if(fd < 0) {
		return NULL;
	}

	buffer = MALLOC(max);
	if(buffer == NULL) {
		goto out_error;
	}

	*size = 0;
	while((ret = read(fd, buffer + *size, max - *size)) > 0) {
		*size += ret;
		if(*size == max) {
			void *p = REALLOC(buffer, max *= 2);
			if(p == NULL) {
				goto out_error;
			}
			buffer = p;
		}
	}
	if(ret < 0) {
		goto out_error;
	}

	close(fd);
	return buffer;

out_error:
	err_save = errno;
	FREE(buffer);
	close(fd);
	errno = err_save;
	return NULL;
}

static void *received(int reason, void *param) {
	struct reason_code_received_t *data = param;

	switch(dev) {
		case LM75: {
			switch(step++) {
				case 0: {
					printf("[ - %-46s ]\n", "second interval");
					fflush(stdout);

					CuAssertStrEquals(gtc, "{\"id\":72,\"temperature\":23.62}", data->message);
					CuAssertIntEquals(gtc, 0, wiringXI2CWriteReg16(fd, 0x00, 0xe017));
				} break;
				case 1: {
					printf("[ - %-46s ]\n", "third interval");
					fflush(stdout);
					CuAssertStrEquals(gtc, "{\"id\":72,\"temperature\":23.88}", data->message);
					CuAssertIntEquals(gtc, 0, wiringXI2CWriteReg16(fd, 0x00, 0xc017));
				} break;
				case 2: {
					CuAssertStrEquals(gtc, "{\"id\":72,\"temperature\":23.75}", data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		} break;
		case LM76: {
			switch(step++) {
				case 0: {
					printf("[ - %-46s ]\n", "second interval");
					fflush(stdout);

					CuAssertStrEquals(gtc, "{\"id\":72,\"temperature\":21.6250}", data->message);
					CuAssertIntEquals(gtc, 0, wiringXI2CWriteReg16(fd, 0x00, 0x8015));
				} break;
				case 1: {
					CuAssertStrEquals(gtc, "{\"id\":72,\"temperature\":21.5000}", data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		} break;
		case BMP180: {
			switch(step++) {
				case 0: {
					CuAssertStrEquals(gtc, "{\"id\":119,\"temperature\":15.00,\"pressure\":699.64}", data->message);
					uv_stop(uv_default_loop());
				} break;
			}
		} break;
	}
	return NULL;
}

static void loop(void *param) {
	int x = 0;
	sleep(1);
	usleep(5000);
	/*
	 * Requested temperature
	 */
	x = wiringXI2CReadReg8(fd, 0xF4);
	CuAssertIntEquals(gtc, x, 0x2e);
	usleep(200000);
	/*
	 * Requested pressure
	 */
	x = wiringXI2CReadReg8(fd, 0xF4);
	CuAssertIntEquals(gtc, x, 0x34);
	wiringXI2CWriteReg8(fd, 0xF6, 0x5D);
	wiringXI2CWriteReg8(fd, 0xF7, 0x23);
	wiringXI2CWriteReg8(fd, 0xF8, 0x00);
}

/*
 * Add timeout in case of failed init
 */
static void test_protocols_i2c_lm75(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	dev = LM75;
	step = 0;
	
	memtrack();

	gtc = tc;
	
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	struct utsname utsname;
	char path[MAX_DIR_PATH+1];
	char file[MAX_DIR_PATH+1], *p = file;
	unsigned long len = 0;

	memset(file, '\0', sizeof(file));

	int ret = delete_module("i2c_stub", 0);

	CuAssertIntEquals(tc, 0, uname(&utsname));
	snprintf(path, MAX_DIR_PATH, "/lib/modules/%s", utsname.release);
	ret = findfile(path, "i2c-stub.ko", &p);
	if(ret != 0) {
		printf("[ - %-46s ]\n", "i2c-stub kernel module not available");
		fflush(stdout);
		return;
	}

	p = grab_file(file, &len);
	CuAssertPtrNotNull(tc, p);

	ret = init_module(p, len, "chip-addr=0x48");
	CuAssertIntEquals(tc, 0, ret);
	FREE(p);

	fd = wiringXI2CSetup("/dev/i2c-0", 0x48);
	CuAssertTrue(tc, fd > 0);

	CuAssertIntEquals(tc, 0, wiringXSetup("raspberrypi1b2", NULL));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0x00, 0xA017));

	lm75Init();

	char *add = "{\"test\":{\"protocol\":[\"lm75\"],\"id\":[{\"id\":72,\"i2c-path\":\"/dev/i2c-0\"}],\"temperature\":0.0,\"poll-interval\":1}}";

	printf("[ - %-46s ]\n", "first interval");
	fflush(stdout);

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	eventpool_gc();
	protocol_gc();
	close(fd);

	ret = delete_module("i2c_stub", 0);
	CuAssertIntEquals(tc, 0, ret);

	CuAssertIntEquals(tc, 3, step);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_i2c_lm76(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	dev = LM76;
	step = 0;
	
	memtrack();

	gtc = tc;
	
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	struct utsname utsname;
	char path[MAX_DIR_PATH+1];
	char file[MAX_DIR_PATH+1], *p = file;
	unsigned long len = 0;

	memset(file, '\0', sizeof(file));

	int ret = delete_module("i2c_stub", 0);

	CuAssertIntEquals(tc, 0, uname(&utsname));
	snprintf(path, MAX_DIR_PATH, "/lib/modules/%s", utsname.release);
	ret = findfile(path, "i2c-stub.ko", &p);
	if(ret != 0) {
		printf("[ - %-46s ]\n", "i2c-stub kernel module not available");
		fflush(stdout);
		return;
	}

	p = grab_file(file, &len);
	CuAssertPtrNotNull(tc, p);

	ret = init_module(p, len, "chip-addr=0x48");
	CuAssertIntEquals(tc, 0, ret);
	FREE(p);

	fd = wiringXI2CSetup("/dev/i2c-0", 0x48);
	CuAssertTrue(tc, fd > 0);

	CuAssertIntEquals(tc, 0, wiringXSetup("raspberrypi1b2", NULL));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0x00, 0xa015));

	lm76Init();

	char *add = "{\"test\":{\"protocol\":[\"lm76\"],\"id\":[{\"id\":72,\"i2c-path\":\"/dev/i2c-0\"}],\"temperature\":0.0,\"poll-interval\":1}}";

	printf("[ - %-46s ]\n", "first interval");
	fflush(stdout);

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	eventpool_gc();
	protocol_gc();
	close(fd);

	ret = delete_module("i2c_stub", 0);
	CuAssertIntEquals(tc, 0, ret);

	CuAssertIntEquals(tc, 2, step);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_protocols_i2c_bmp180(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	dev = BMP180;
	step = 0;
	
	memtrack();

	gtc = tc;
	
	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	struct utsname utsname;
	char path[MAX_DIR_PATH+1];
	char file[MAX_DIR_PATH+1], *p = file;
	unsigned long len = 0;

	memset(file, '\0', sizeof(file));

	int ret = delete_module("i2c_stub", 0);

	CuAssertIntEquals(tc, 0, uname(&utsname));
	snprintf(path, MAX_DIR_PATH, "/lib/modules/%s", utsname.release);
	ret = findfile(path, "i2c-stub.ko", &p);
	if(ret != 0) {
		printf("[ - %-46s ]\n", "i2c-stub kernel module not available");
		fflush(stdout);
		return;
	}

	p = grab_file(file, &len);
	CuAssertPtrNotNull(tc, p);

	ret = init_module(p, len, "chip-addr=0x77");
	CuAssertIntEquals(tc, 0, ret);
	FREE(p);

	fd = wiringXI2CSetup("/dev/i2c-0", 0x77);
	CuAssertTrue(tc, fd > 0);

	CuAssertIntEquals(tc, 0, wiringXSetup("raspberrypi1b2", NULL));
	/* Set version */
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg8(fd, 0xD0, 0x55));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg8(fd, 0xD1, 0x01));

	/* Set calibration */
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xAA, 0x9801));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xAC, 0xB8FF));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xAE, 0xD1C7));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xB0, 0xE57F));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xB2, 0xF57F));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xB4, 0x715A));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xB6, 0x2E18));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xB8, 0x0400));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xBA, 0x0080));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xBC, 0xF9DD));
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xBE, 0x340B));

	/* Temperature */
	CuAssertIntEquals(tc, 0, wiringXI2CWriteReg16(fd, 0xF6, 0xFA6C));

	bmp180Init();

	char *add = "{\"test\":{\"protocol\":[\"bmp180\"],\"id\":[{\"id\":119,\"i2c-path\":\"/dev/i2c-0\"}],\"temperature\":0.0,\"pressure\":0.0,\"oversampling\":0,\"poll-interval\":1}}";

	// printf("[ - %-46s ]\n", "first interval");
	// fflush(stdout);

	eventpool_init(EVENTPOOL_NO_THREADS);

	eventpool_trigger(REASON_DEVICE_ADDED, done, json_decode(add));
	eventpool_callback(REASON_CODE_RECEIVED, received);

	uv_thread_create(&pth, loop, NULL);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}

	eventpool_gc();
	protocol_gc();
	close(fd);

	ret = delete_module("i2c_stub", 0);
	CuAssertIntEquals(tc, 0, ret);

	CuAssertIntEquals(tc, 1, step);
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_protocols_i2c(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_protocols_i2c_lm75);
	SUITE_ADD_TEST(suite, test_protocols_i2c_lm76);
	SUITE_ADD_TEST(suite, test_protocols_i2c_bmp180);

	return suite;
}