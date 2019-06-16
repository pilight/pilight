/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _COAP_H_
#define _COAP_H_

#include <stdint.h>
#include <netinet/in.h>

struct mdns_cache_t {
	char *suffix;
	int pos;
} mdns_cache_t;

struct mdns_text_t {
	char **values;
	int nrvalues;
} mdns_text_t;

struct mdns_payload_t {
	char *name;
	uint8_t flush;
	uint16_t type;
	uint8_t class;
	uint16_t ttl;
	uint8_t length;
	uint8_t priority;
	uint8_t weight;
	uint16_t port;
	union {
		char *domain;
		uint8_t ip4[4];
		uint8_t ip6[6];
		struct mdns_text_t text;
	} data;
} mdns_payload_t;

struct mdns_query_t {
	char *name;
	uint8_t qu;
	uint8_t class;
	uint8_t type;
} mdns_query_t;

struct mdns_packet_t {
	uint16_t id;
	uint8_t qr;
	uint8_t opcode;
	uint8_t aa;
	uint8_t tc;
	uint8_t rd;
	uint8_t ra;
	uint8_t z;
	uint8_t ad;
	uint8_t cd;
	uint8_t rcode;
	uint16_t nrqueries;
	uint16_t nranswers;
	uint16_t rr_auth;
	uint16_t rr_add;

	/*union {
		struct mdns_payload_t payload;
		struct mdns_query_t query;
	} **queries;*/

	struct mdns_query_t **queries;
	struct mdns_payload_t **answers;
	struct mdns_payload_t **records;

	uint16_t nrcache;
	struct mdns_cache_t **cache;

} mdns_packet_t;


int mdns_decode(struct mdns_packet_t *mdns, unsigned char *message, unsigned int msglen);
unsigned char *mdns_encode(struct mdns_packet_t *mdns, unsigned int *len);
int mdns_send(struct mdns_packet_t *pkt, void (*)(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata), void *userdata);
int mdns_listen(void (*)(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata), void *userdata);
void mdns_free(struct mdns_packet_t *mdns);
void mdns_dump(struct mdns_packet_t *mdns);
void mdns_gc(void);

#endif
