/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _ARP_H_
#define _ARP_H_

typedef struct arp_list_t {
	struct arp_list_nodes_t *nodes;
} arp_list_t;

typedef struct arp_list_nodes_t {
	char dstmac[19];
	char dstip[INET_ADDRSTRLEN+1];
	int tries;
	int timeout;
	int called;
	unsigned long time;

	struct arp_list_nodes_t *next;
} arp_list_nodes_t;

void arp_add_host(struct arp_list_t **, const char *);
void arp_init_list(struct arp_list_t **);
int arp_resolv(struct arp_list_t *, char *, char *, char *, int, void (*)(char *, char *));

#endif
