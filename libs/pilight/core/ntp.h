/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _NTP_H_
#define _NTP_H_

void *ntpthread(void *);
int ntpinterval(void *);
int getntpdiff(void);
int ntp_gc(void);
int isntpsynced(void);

#endif
