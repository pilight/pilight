/*
	Copyright (c) 2014 CurlyMo <curlymoo1@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _WIRING_X_H_
#define _WIRING_X_H_

#include <errno.h>
#include <syslog.h>

#ifndef	TRUE
#define	TRUE	(1==1)
#define	FALSE	(1==2)
#endif

#define HIGH		1
#define LOW			0

#define INPUT				0
#define OUTPUT				1
#define	PWM_OUTPUT			2
#define	GPIO_CLOCK			3
#define	SOFT_PWM_OUTPUT		4
#define	SOFT_TONE_OUTPUT	5
#define	PWM_TONE_OUTPUT		6
#define SYS					7

#define	INT_EDGE_SETUP		0
#define INT_EDGE_FALLING	1
#define INT_EDGE_RISING		2
#define INT_EDGE_BOTH 		3

#define	PWM_MODE_MS			0
#define	PWM_MODE_BAL		1

void (*wiringXLog)(int prio, const char *format_str, ...);

typedef struct platform_t {
	char *name;
	int (*setup)(void);
	int (*pinMode)(int pin, int mode);
	int (*digitalWrite)(int pin, int val);
	int (*digitalRead)(int pin);
	int (*identify)(void);
	int (*waitForInterrupt)(int pin, int ms);
	int (*isr)(int pin, int mode);
	int (*I2CRead)(int fd);
	int (*I2CReadReg8)(int fd, int reg);
	int (*I2CReadReg16)(int fd, int reg);
	int (*I2CWrite)(int fd, int data);
	int (*I2CWriteReg8)(int fd, int reg, int data);
	int (*I2CWriteReg16)(int fd, int reg, int data);
	int (*I2CSetup)(int devId);
	int (*validGPIO)(int gpio);
	int (*gc)(void);
	struct platform_t *next;
} platform_t;

struct platform_t *platforms;

void platform_register(struct platform_t **platform, const char *name);
void delayMicroseconds(unsigned int howLong);
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int waitForInterrupt(int pin, int ms);
int wiringXGC(void);
int wiringXISR(int pin, int mode);
int wiringXSetup(void);
int wiringXI2CRead(int fd);
int wiringXI2CReadReg8(int fd, int reg);
int wiringXI2CReadReg16(int fd, int reg);
int wiringXI2CWrite(int fd, int data);
int wiringXI2CWriteReg8(int fd, int reg, int data);
int wiringXI2CWriteReg16(int fd, int reg, int data);
int wiringXI2CSetup(int devId);
char *wiringXPlatform(void);
int wiringXValidGPIO(int gpio);

#endif
