/*
 * ARP Scan is Copyright (C) 2005-2013 Roy Hills, NTA Monitor Ltd.
 *
 * This file is part of arp-scan.
 *
 * arp-scan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * arp-scan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with arp-scan.  If not, see <http://www.gnu.org/licenses/>.
 *
 * arp-scan.h -- Header file for ARP scanner
 *
 * Author:	Roy Hills
 * Date:	11 October 2005
 *
 * The ARP protocol is defined in RFC 826 Ethernet Address Resolution Protocol
 *
 */

#ifndef _ARP_H_
#define _ARP_H_

typedef struct arp_list_t {
	char dstmac[19];
	char dstip[INET_ADDRSTRLEN+1];
	int tries;
	int found;
	int timeout;
	unsigned long time;
	struct arp_list_t *next;
} arp_list_t;

void arp_add_host(struct arp_list_t **, const char *);
int arp_resolv(struct arp_list_t *, char *, char *, char *, int, void (*)(char *, char *));

#endif
