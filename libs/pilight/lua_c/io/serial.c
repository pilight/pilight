/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <ctype.h>

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "../../core/log.h"
#include "../../config/config.h"
#include "../table.h"
#include "../io.h"

typedef struct lua_serial_t {
	PLUA_INTERFACE_FIELDS

	char *callback;

	int baudrate;
	int flowcontrol;
	int parity;
	int databits;
	int stopbits;

	int wrunning;
	int rrunning;
	int stop;
	int stopping;
	int reconnect;

	struct {
		struct timespec first;
		struct timespec second;
	} timestamp;

	uv_timer_t *timer_req;
	char rbuffer[BUFFER_SIZE];
	char wbuffer[BUFFER_SIZE];

	char *file;
	int fd;

} lua_serial_t;

typedef struct serial_fd_list_t {
	struct lua_serial_t *serial;
	struct serial_fd_list_t *next;
} serial_fd_list_t;

#ifdef _WIN32
	static uv_mutex_t lock;
#else
	static pthread_mutex_t lock;
	static pthread_mutexattr_t lock_attr;
#endif
static struct serial_fd_list_t *serial_fd_list = NULL;
static int gc_regged = 0;
static int mutex_init = 0;

static void plua_io_serial_object(lua_State *L, struct lua_serial_t *serial);
static int plua_io_serial__open(struct lua_serial_t *serial, lua_State *L);

/*
 * Remove from list
 */
static void plua_io_serial_gc(void *ptr, int rm_from_list) {
	struct lua_serial_t *data = ptr;

	if(rm_from_list == 1) {
#ifdef _WIN32
		uv_mutex_lock(&lock);
#else
		pthread_mutex_lock(&lock);
#endif

		struct serial_fd_list_t *currP, *prevP;

		prevP = NULL;

		for(currP = serial_fd_list; currP != NULL; prevP = currP, currP = currP->next) {
			if(currP->serial == data) {
				if(prevP == NULL) {
					serial_fd_list = currP->next;
				} else {
					prevP->next = currP->next;
				}

				FREE(currP);
				break;
			}
		}
#ifdef _WIN32
		uv_mutex_unlock(&lock);
#else
		pthread_mutex_unlock(&lock);
#endif
	}

	data->stopping = 1;

	while(data->rrunning == 1 || data->wrunning == 1) {
		usleep(100);
	}

	plua_metatable_free(data->table);

	if(data->fd > -1) {
		if(close(data->fd) != 0) {
			logprintf(LOG_ERR, "serial.gc: could not close device %s with fd (%p)", data->file, data->fd);
		}
		data->fd = -1;
	}

	if(data->file != NULL) {
		FREE(data->file);
	}

	if(data->callback != NULL) {
		FREE(data->callback);
	}

	FREE(data);
}

static void plua_io_serial_global_gc(void *ptr) {
#ifdef _WIN32
	uv_mutex_lock(&lock);
#else
	pthread_mutex_lock(&lock);
#endif
	struct serial_fd_list_t *node = NULL;
	while(serial_fd_list) {
		node = serial_fd_list;

		plua_io_serial_gc(node->serial, 0);

		serial_fd_list = serial_fd_list->next;
		FREE(node);
	}
#ifdef _WIN32
	uv_mutex_unlock(&lock);
#else
	pthread_mutex_unlock(&lock);
#endif

	gc_regged = 0;
}

static int plua_io_serial_change_attributes(struct lua_serial_t *serial) {
	struct termios options;
	speed_t myBaud;
	int status = 0;

	switch(serial->baudrate) {
		case 50: myBaud = B50; break;
		case 75: myBaud = B75; break;
		case 110: myBaud = B110; break;
		case 134: myBaud = B134; break;
		case 150: myBaud = B150; break;
		case 200: myBaud = B200; break;
		case 300: myBaud = B300; break;
		case 600: myBaud = B600; break;
		case 1200: myBaud = B1200; break;
		case 1800: myBaud = B1800; break;
		case 2400: myBaud = B2400; break;
		case 4800: myBaud = B4800; break;
		case 9600: myBaud = B9600; break;
		case 19200: myBaud = B19200; break;
		case 38400: myBaud = B38400; break;
		case 57600: myBaud = B57600; break;
		case 115200: myBaud = B115200; break;
		case 230400: myBaud = B230400; break;
		default:
		return -1;
	}

	memset(&options, 0, sizeof(struct termios));

	tcgetattr(serial->fd, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, myBaud);
	cfsetospeed(&options, myBaud);

	options.c_cflag |= (CLOCAL | CREAD);

	options.c_cflag &= ~CSIZE;

	switch(serial->databits) {
		case 7:
			options.c_cflag |= CS7;
		break;
		case 8:
			options.c_cflag |= CS8;
		break;
		default:
		return -1;
	}

	switch(serial->parity) {
		case 'n':
		case 'N':/*no parity*/
			options.c_cflag &= ~PARENB;
			options.c_iflag &= ~INPCK;
		break;
		case 'o':
		case 'O':
			options.c_cflag |= (PARODD | PARENB);
			options.c_iflag |= INPCK; /* Disable parity checking */
		break;
		case 'e':
		case 'E':
			options.c_cflag |= PARENB; /* Enable parity */
			options.c_cflag &= ~PARODD;
			options.c_iflag |= INPCK;
		break;
		case 'S':
		case 's': /*as no parity*/
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;
		break;
		return -1;
	}

	switch(serial->stopbits) {
		case 1:
			options.c_cflag &= ~CSTOPB;
		break;
		case 2:
			options.c_cflag |= CSTOPB;
		break;
		return -1;
	}

	switch(serial->flowcontrol) {
		case 'x':
		case 'X':
			options.c_iflag |= (IXON | IXOFF | IXANY);
		break;
		case 'n':
		case 'N':
			options.c_iflag &= ~(IXON | IXOFF | IXANY);
		break;
		return -1;
	}

	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;

	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 150;

	tcflush(serial->fd, TCIFLUSH);

	tcsetattr(serial->fd, TCSANOW | TCSAFLUSH, &options);

	ioctl(serial->fd, TIOCMGET, &status);

	status |= TIOCM_DTR;
	status |= TIOCM_RTS;

	ioctl(serial->fd, TIOCMSET, &status);

	return 0;
}

static int plua_io_serial_close(struct lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "serial.close requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	if(serial->file == NULL) {
		luaL_error(L, "serial.close: serial device has not been set");
	}

	if(serial->fd == -1) {
		luaL_error(L, "serial.close: device %s has not been opened", serial->file);
	}

	serial->stop = 1;
	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_io_serial_callback(char *type, uv_fs_t *req) {
	struct lua_serial_t *serial = req->data;
	char name[255], *p = name;
	memset(name, '\0', 255);

	if(serial->callback == NULL){
		return;
	}

	/*
	 * Only create a new state once the timer is triggered
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = serial->module;

	logprintf(LOG_DEBUG, "lua serial read on state #%d", state->idx);

	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		/*
		 * FIXME shouldn't state be freed?
		 */
		luaL_error(state->L, "cannot find %s lua module", name);
		return;
	}

	lua_getfield(state->L, -1, serial->callback);
	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		/*
		 * FIXME shouldn't state be freed?
		 */
		luaL_error(state->L, "%s: async callback %s does not exist", state->module->file, serial->callback);
		return;
	}

	lua_pushstring(state->L, type);
	plua_io_serial_object(state->L, serial);
	if(strcmp(type, "write") == 0) {
		if(req->result < 0) {
			lua_pushstring(state->L, "fail");
		} else {
			lua_pushstring(state->L, "success");
		}
	} else {
		lua_pushstring(state->L, serial->rbuffer);
	}

	if(lua_pcall(state->L, 3, 0, 0) == LUA_ERRRUN) {
		if(lua_type(state->L, -1) == LUA_TNIL) {
			logprintf(LOG_ERR, "%s: syntax error", state->module->file);
			plua_clear_state(state);
			return;
		}
		if(lua_type(state->L, -1) == LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(state->L,  -1));
			lua_pop(state->L, -1);
			plua_clear_state(state);
			return;
		}
	}

	lua_remove(state->L, 1);
	plua_clear_state(state);
}

static void plua_io_serial_read_callback(uv_fs_t *req) {
	struct lua_serial_t *serial = req->data;
	unsigned int len = req->result;

	if(serial->stopping == 1) {
		memset(serial->rbuffer, 0, BUFFER_SIZE);
		uv_fs_req_cleanup(req);
		FREE(req);
		return;
	}
	serial->rrunning = 1;
	if((int)len < 0) {
		memset(serial->rbuffer, 0, BUFFER_SIZE);
		logprintf(LOG_ERR, "read error: %s", uv_strerror(req->result));
	} else if((int)len == 0) {
		memset(serial->rbuffer, 0, BUFFER_SIZE);
		memcpy(&serial->timestamp.first, &serial->timestamp.second, sizeof(struct timespec));
		clock_gettime(CLOCK_MONOTONIC, &serial->timestamp.second);
		if((((double)serial->timestamp.second.tv_sec + 1.0e-9*serial->timestamp.second.tv_nsec) -
			((double)serial->timestamp.first.tv_sec + 1.0e-9*serial->timestamp.first.tv_nsec)) < 0.0001) {
			if(access(serial->file, F_OK) == -1) {
				plua_io_serial_callback("disconnect", req);
				goto stop;
			}
		}
		if(serial->stop == 0) {
			clock_gettime(CLOCK_MONOTONIC, &serial->timestamp.second);

			uv_fs_t *read_req = NULL;
			if((read_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			read_req->data = serial;

			uv_buf_t buf = uv_buf_init(serial->rbuffer, BUFFER_SIZE);
			uv_fs_read(uv_default_loop(), read_req, serial->fd, &buf, 1, -1, plua_io_serial_read_callback);
		}
	} else if(len > 0) {
		serial->rbuffer[req->result] = '\0';
		plua_io_serial_callback("read", req);
		memset(serial->rbuffer, 0, BUFFER_SIZE);
	}

stop:
	uv_fs_req_cleanup(req);
	FREE(req);

	serial->rrunning = 0;

	if(serial->stop == 1) {
		plua_io_serial_gc(serial, 1);
	}
}

static void plua_io_serial_write_callback(uv_fs_t *req) {
	struct lua_serial_t *serial = req->data;

	if(serial->stopping == 1) {
		return;
	}

	plua_io_serial_callback("write", req);
	serial->wrunning = 0;

	uv_fs_req_cleanup(req);
	FREE(req);

	if(serial->stop == 1) {
		plua_io_serial_gc(serial, 1);
	}
}

static int plua_io_serial_set_callback(lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "serial.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	if(serial->file == NULL) {
		luaL_error(L, "serial.setCallback: serial device has not been set");
	}

	const char *func = NULL;
	char buf[128] = { '\0' }, *p = buf, name[255] = { '\0' };
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	if(lua_type(L, -1) == LUA_TSTRING) {
		func = lua_tostring(L, -1);
		lua_remove(L, -1);
	}

	p = name;
	plua_namespace(serial->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		luaL_error(L, "cannot find %s lua module", serial->module->name);
		return 0;
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		luaL_error(L, "%s: serial callback %s does not exist", serial->module->file, func);
		return 0;
	}

	if(serial->callback != NULL) {
		FREE(serial->callback);
	}

	if((serial->callback = STRDUP((char *)func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_pushboolean(L, 1);

	return 1;
}

static int plua_io_serial__open(struct lua_serial_t *serial, lua_State *L) {
	int nrargs = lua_gettop(L);

	if(nrargs != 0) {
		luaL_error(L, "serial.open requires 0 arguments, %d given", lua_gettop(L));
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	if((serial->fd = open(serial->file, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK)) == -1) {
		logprintf(LOG_ERR, "serial.open: could not open serial device \"%s\"", serial->file);
		lua_pushboolean(L, 0);
	} else if(plua_io_serial_change_attributes(serial) == 0) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	if(serial->timer_req == NULL) {
		serial->timer_req = MALLOC(sizeof(uv_timer_t));
		if(serial->timer_req == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		serial->timer_req->data = serial;

		uv_timer_init(uv_default_loop(), serial->timer_req);
	}

	return 1;
}

static int plua_io_serial_open(lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));

	plua_io_serial__open(serial, L);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_io_serial_set_baudrate(struct lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));
	int baudrate = 9600;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "serial.setBaudrate requires 1 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TNUMBER),
		1, buf);

	if(lua_type(L, -1) == LUA_TNUMBER) {
		baudrate = lua_tonumber(L, -1);
		lua_remove(L, -1);
	}

	serial->baudrate = baudrate;

	if(serial->fd > -1 && plua_io_serial_change_attributes(serial) == 0) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_io_serial_set_parity(struct lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));
	const char *parity = NULL;
	char list[8][2] = { "n", "N", "o", "O", "e", "E", "s", "S"};

	if(lua_gettop(L) != 1) {
		luaL_error(L, "serial.setParity requires 1 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	if(lua_type(L, -1) == LUA_TSTRING) {
		parity = lua_tostring(L, -1);
		lua_remove(L, -1);
	}

	int i = 0, match = 0;
	for(i=0;i<5;i++) {
		if(strcmp(parity, list[i]) == 0) {
			match = 1;
			break;
		}
	}
	if(match == 0) {
		luaL_error(L, "serial.setParity: \"%s\" is an unsupported parity", parity);
	}

	serial->parity = tolower(parity[0]);

	if(serial->fd > -1 && plua_io_serial_change_attributes(serial) == 0) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_io_serial_get_data(lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "serial.getUserdata requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
		return 0;
	}

	plua_metatable__push(L, (struct plua_interface_t *)serial);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_io_serial_write(lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *content = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "serial.write requires 1 argument, %d given", lua_gettop(L));
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	if(serial->stopping == 1) {
		return 0;
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	content = (char *)lua_tostring(L, -1);
	lua_remove(L, -1);

	uv_fs_t *write_req = NULL;
	if((write_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_buf_t wbuf = uv_buf_init(serial->wbuffer, BUFFER_SIZE);

	strcpy(wbuf.base, content);
	wbuf.len = strlen(content);
	write_req->data = serial;

	serial->wrunning = 1;

	uv_fs_write(uv_default_loop(), write_req, serial->fd, &wbuf, 1, -1, plua_io_serial_write_callback);

	plua_ret_false(L);

	return 0;
}

static int plua_io_serial_read(lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "serial.write requires 0 arguments, %d given", lua_gettop(L));
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	uv_fs_t *read_req = NULL;
	if((read_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	read_req->data = serial;

	clock_gettime(CLOCK_MONOTONIC, &serial->timestamp.second);

	uv_buf_t buffer = uv_buf_init(serial->rbuffer, BUFFER_SIZE);
	uv_fs_read(uv_default_loop(), read_req, serial->fd, &buffer, 1, -1, plua_io_serial_read_callback);

	plua_ret_true(L);

	return 0;
}

static int plua_io_serial_set_data(lua_State *L) {
	struct lua_serial_t *serial = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "serial.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(serial == NULL) {
		luaL_error(L, "internal error: serial object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		if(serial->table != (void *)lua_topointer(L, -1)) {
			plua_metatable_free(serial->table);
		}
		serial->table = (void *)lua_topointer(L, -1);

		if(serial->table->ref != NULL) {
			uv_sem_post(serial->table->ref);
		}

		plua_ret_true(L);
		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, serial->table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static void plua_io_serial_object(lua_State *L, struct lua_serial_t *serial) {
	lua_newtable(L);

	lua_pushstring(L, "setBaudrate");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_set_baudrate, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setParity");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_set_parity, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "open");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_open, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "write");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_write, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "read");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_read, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "close");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_close, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_get_data, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, serial);
	lua_pushcclosure(L, plua_io_serial_set_data, 1);
	lua_settable(L, -3);
}

int plua_io_serial(struct lua_State *L) {
	if(lua_gettop(L) != 1) {
		luaL_error(L, "serial requires 1 argument, %d given", lua_gettop(L));
		return 0;
	}

	char *name = NULL;

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "string expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TSTRING),
			1, buf);

		name = (void *)lua_tostring(L, -1);
		lua_remove(L, -1);
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	if(mutex_init == 0) {
		mutex_init = 1;
#ifdef _WIN32
		uv_mutex_init(&lock);
#else
		pthread_mutexattr_init(&lock_attr);
		pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&lock, &lock_attr);
#endif
	}

#ifdef _WIN32
	uv_mutex_lock(&lock);
#else
	pthread_mutex_lock(&lock);
#endif
	int match = 0;
	struct serial_fd_list_t *node = serial_fd_list;
	while(node) {
		if(strcmp(node->serial->file, name) == 0) {
			match = 1;
			break;
		}
		node = node->next;
	}
#ifdef _WIN32
	uv_mutex_unlock(&lock);
#else
	pthread_mutex_unlock(&lock);
#endif

	if(match == 0) {
		struct lua_serial_t *lua_serial = MALLOC(sizeof(struct lua_serial_t));
		if(lua_serial == NULL) {
			OUT_OF_MEMORY
		}
		memset(lua_serial, 0, sizeof(struct lua_serial_t));
		plua_metatable_init(&lua_serial->table);

		lua_serial->baudrate = 9600;
		lua_serial->parity = 'n';
		lua_serial->databits = 8;
		lua_serial->stopbits = 1;
		lua_serial->flowcontrol = 'n';
		lua_serial->fd = -1;
		lua_serial->stop = 0;
		lua_serial->stopping = 0;
		lua_serial->wrunning = 0;
		lua_serial->rrunning = 0;
		lua_serial->reconnect = 0;

		if((lua_serial->file = STRDUP(name)) == NULL) {
			OUT_OF_MEMORY
		}

		lua_serial->module = state->module;
		lua_serial->L = L;

#ifdef _WIN32
		uv_mutex_lock(&lock);
#else
		pthread_mutex_lock(&lock);
#endif
		if((node = MALLOC(sizeof(struct serial_fd_list_t))) == NULL) {
			OUT_OF_MEMORY
		}
		node->serial = lua_serial;
		node->next = serial_fd_list;
		serial_fd_list = node;
#ifdef _WIN32
		uv_mutex_unlock(&lock);
#else
		pthread_mutex_unlock(&lock);
#endif

		plua_io_serial_object(L, lua_serial);
	} else {
		plua_io_serial_object(L, node->serial);
	}

	if(gc_regged == 0) {
		gc_regged = 1;
		plua_gc_reg(NULL, NULL, plua_io_serial_global_gc);
	}

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
