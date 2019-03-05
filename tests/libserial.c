/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#define __USE_GNU
#include <dlfcn.h>
#include <termios.h>

#ifdef _WIN32
__declspec(dllexport)
#endif

int tcgetattr(int fildes, struct termios *termios_p) {
	memset(termios_p, 0, sizeof(struct termios));
	return 0;
}

int tcsetattr(int fildes, int optional_actions, const struct termios *termios_p) {
	char buffer[1024];
	int l = 0;

	l += snprintf(buffer, 1024,
		"%4u",
		optional_actions);
	l += snprintf(&buffer[l], 1024-l,
		"%8u",
		termios_p->c_iflag);
	l += snprintf(&buffer[l], 1024-l,
		"%8u",
		termios_p->c_oflag);
	l += snprintf(&buffer[l], 1024-l,
		"%8u",
		termios_p->c_cflag);
	l += snprintf(&buffer[l], 1024-l,
		"%8u",
		termios_p->c_lflag);

	int i = 0;
	for(i=0;i<NCCS;i++) {
		l += snprintf(&buffer[l], 1024-l,
			"%4u",
			termios_p->c_cc[i]);
	}

	l += snprintf(&buffer[l], 1024-l,
		"%8u",
		termios_p->c_ispeed);
	l += snprintf(&buffer[l], 1024-l,
		"%8u",
		termios_p->c_ospeed);
	write(fildes, buffer, l);
	return 0;
}