/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _ARP_H_
#define _ARP_H_

typedef struct arp_list_t {
	char dstmac[19];
	char dstip[INET_ADDRSTRLEN+1];
	int tries;
	int timeout;
	int called;
	unsigned long time;

	struct arp_list_t *next;
} arp_list_t;

void arp_add_host(struct arp_list_t **, const char *);
int arp_resolv(struct arp_list_t *, char *, char *, char *, int, void (*)(char *, char *));

#endif
