/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _NETWORK_H_
#define _NETWORK_H_

#ifndef _WIN32
	#if _WIN32_WINNT < 0x0501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#define WIN32_LEAN_AND_MEAN
	#include <sys/types.h>
	#include <ifaddrs.h>
	#include <sys/socket.h>
#endif
#include <stdint.h>

#include "pilight.h"

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#ifdef _WIN32
#define sa_family_t uint16_t
#endif

int inetdevs(char ***array);
int dev2mac(char *ifname, char **mac);
#ifdef __FreeBSD__
int dev2ip(char *dev, char **ip, __sa_family_t type);
#else
int dev2ip(char *dev, char **ip, sa_family_t type);
#endif
int host2ip(char *host, char **ip);
int whitelist_check(char *ip);
void whitelist_free(void);

#ifdef __FreeBSD__
struct sockaddr *sockaddr_dup(struct sockaddr *sa);
int rep_getifaddrs(struct ifaddrs **ifap);
void rep_freeifaddrs(struct ifaddrs *ifap);
#endif

#endif
