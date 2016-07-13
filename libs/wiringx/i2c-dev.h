/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _WIRINGX_I2C_DEV_H_
#define _WIRINGX_I2C_DEV_H_

#ifndef INLINE
#define INLINE static inline
#endif

#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c.h>

#define I2C_SLAVE	0x0703
#define I2C_SMBUS	0x0720

struct i2c_smbus_ioctl_data {
	__u8 read_write;
	__u8 command;
	__u32 size;
	union i2c_smbus_data *data;
};

inline __s32 i2c_smbus_access(int fd, char rw, int cmd, int size, union i2c_smbus_data *data);
inline __s32 i2c_smbus_read_byte(int fd);
inline __s32 i2c_smbus_write_byte(int fd, int value);
inline __s32 i2c_smbus_read_byte_data(int fd, int cmd);
inline __s32 i2c_smbus_write_byte_data(int fd, int cmd, int value);
inline __s32 i2c_smbus_read_word_data(int fd, int cmd);
inline __s32 i2c_smbus_write_word_data(int fd, int cmd, __u16 value);

#endif
