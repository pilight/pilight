/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __FreeBSD__

#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c.h>

#include "i2c-dev.h"

extern inline __s32 i2c_smbus_access(int fd, char rw, int cmd, int size, union i2c_smbus_data *data) {
	struct i2c_smbus_ioctl_data args;

	args.read_write = rw;
	args.command = cmd;
	args.size = size;
	args.data = data;

	return ioctl(fd, I2C_SMBUS, &args);
}

extern inline __s32 i2c_smbus_read_byte(int fd) {
	union i2c_smbus_data data;
	if(i2c_smbus_access(fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data) <= 0) {
		return -1;
	} else {
		return 0x0FF & data.byte;
	}
}

extern inline __s32 i2c_smbus_write_byte(int fd, int value) {
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}

extern inline __s32 i2c_smbus_read_byte_data(int fd, int cmd) {
	union i2c_smbus_data data;
	if(i2c_smbus_access(fd, I2C_SMBUS_READ, cmd, I2C_SMBUS_BYTE_DATA, &data) <= 0) {
		return -1;
	} else {
		return 0x0FF & data.byte;
	}
}

extern inline __s32 i2c_smbus_write_byte_data(int fd, int cmd, int value) {
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_BYTE_DATA, &data);
}

extern inline __s32 i2c_smbus_read_word_data(int fd, int cmd) {
	union i2c_smbus_data data;
	if(i2c_smbus_access(fd, I2C_SMBUS_READ, cmd, I2C_SMBUS_WORD_DATA, &data) <= 0) {
		return -1;
	} else {
		return 0x0FFFF & data.word;
	}
}

extern inline __s32 i2c_smbus_write_word_data(int fd, int cmd, __u16 value) {
	union i2c_smbus_data data;
	data.word = value;

	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_WORD_DATA, &data);
}

#endif