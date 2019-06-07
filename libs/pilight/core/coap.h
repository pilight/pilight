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

typedef struct coap_options_t {
		unsigned char *val;
		unsigned int num;
		unsigned int len;
} coap_options_t;

typedef struct coap_packet_t {
	uint8_t ver;
	uint8_t t;
	uint8_t tkl;
	uint8_t code[2];
	uint8_t msgid[2];
	char *token;
	int numopt;
	struct coap_options_t **options;
	char *payload;
} coap_packet_t;

int coap_decode(struct coap_packet_t *coap, unsigned char *message, unsigned int msglen);
unsigned char *coap_encode(struct coap_packet_t *coap, unsigned int *len);
int coap_send(struct coap_packet_t *pkt, void (*)(const struct sockaddr *addr, struct coap_packet_t *pkt, void *userdata), void *userdata);
int coap_listen(void (*)(const struct sockaddr *addr, struct coap_packet_t *pkt, void *userdata), void *userdata);
void coap_free(struct coap_packet_t *coap);
void coap_dump(struct coap_packet_t *coap);
void coap_gc(void);

#endif
