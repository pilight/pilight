/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/http.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/webserver.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/mdns.h"

#include "alltests.h"

static uv_thread_t pth;
static uv_timer_t *timer_req = NULL;
static uv_async_t *async_close_req = NULL;
static int mdns_socket = 0;
static int mdns_loop = 1;
static int check = 0;
static int step = 0;
static int run = 0;
static CuTest *gtc = NULL;

static struct {
	unsigned int len;
	unsigned char msg[523];
} messages[4] = {
	/*
	 * Tosmato
	 */
	{ 114, { 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x05, 0x5f, 0x68, 0x74, 0x74, 0x70, 0x04, 0x5f, 0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x1c, 0x0b, 0x73, 0x6f, 0x6e, 0x6f, 0x66, 0x66, 0x2d, 0x38, 0x30, 0x36, 0x36, 0xc0, 0x0c, 0xc0, 0x28, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x00, 0xc0, 0x28, 0x00, 0x21, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x0b, 0x73, 0x6f, 0x6e, 0x6f, 0x66, 0x66, 0x2d, 0x38, 0x30, 0x36, 0x36, 0xc0, 0x17, 0xc0, 0x54, 0x00, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04, 0xc0, 0xa8, 0x01, 0x80 } },
	/*
	 * Spotify
	 */
	{ 190, { 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x12, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x2d, 0x30, 0x10, 0x5f, 0x73, 0x70, 0x6f, 0x74, 0x69, 0x66, 0x79, 0x2d, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x04, 0x5f, 0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x2e, 0xc0, 0x0c, 0x09, 0x5f, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x73, 0x07, 0x5f, 0x64, 0x6e, 0x73, 0x2d, 0x73, 0x64, 0x04, 0x5f, 0x75, 0x64, 0x70, 0xc0, 0x35, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x1b, 0xc0, 0x1f, 0xc0, 0x0c, 0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xc0, 0x0c, 0xc0, 0x0c, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x21, 0x0b, 0x43, 0x50, 0x61, 0x74, 0x68, 0x3d, 0x2f, 0x7a, 0x63, 0x2f, 0x30, 0x0b, 0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x3d, 0x31, 0x2e, 0x30, 0x08, 0x53, 0x74, 0x61, 0x63, 0x6b, 0x3d, 0x53, 0x50, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04, 0xc0, 0xa8, 0x01, 0x80 } },
	/*
	 * Amplifier
	 */
	{ 522, { 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x05, 0x5f, 0x68, 0x74, 0x74, 0x70, 0x04, 0x5f, 0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x1f, 0x0e, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x20, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x0c, 0x08, 0x5f, 0x61, 0x69, 0x72, 0x70, 0x6c, 0x61, 0x79, 0xc0, 0x12, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x22, 0x0e, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x20, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x39, 0x05, 0x5f, 0x72, 0x61, 0x6f, 0x70, 0xc0, 0x12, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x2c, 0x1b, 0x30, 0x30, 0x30, 0x36, 0x37, 0x38, 0x32, 0x35, 0x32, 0x34, 0x46, 0x35, 0x40, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x20, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x5f, 0x0e, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x2d, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x17, 0x00, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04, 0x0a, 0x00, 0x00, 0xbf, 0xc0, 0x28, 0x00, 0x21, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0xc0, 0x8f, 0xc0, 0x28, 0x00, 0x10, 0x80, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x01, 0x00, 0xc0, 0x4e, 0x00, 0x21, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xc0, 0x8f, 0xc0, 0x4e, 0x00, 0x10, 0x80, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x5c, 0x1a, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65, 0x69, 0x64, 0x3d, 0x30, 0x30, 0x3a, 0x30, 0x36, 0x3a, 0x37, 0x38, 0x3a, 0x32, 0x35, 0x3a, 0x32, 0x34, 0x3a, 0x46, 0x35, 0x13, 0x66, 0x65, 0x61, 0x74, 0x75, 0x72, 0x65, 0x73, 0x3d, 0x30, 0x78, 0x34, 0x34, 0x34, 0x46, 0x38, 0x41, 0x30, 0x30, 0x09, 0x66, 0x6c, 0x61, 0x67, 0x73, 0x3d, 0x30, 0x78, 0x34, 0x0c, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x3d, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0x07, 0x73, 0x65, 0x65, 0x64, 0x3d, 0x39, 0x39, 0x0d, 0x73, 0x72, 0x63, 0x76, 0x65, 0x72, 0x73, 0x3d, 0x31, 0x39, 0x30, 0x2e, 0x39, 0xc0, 0x71, 0x00, 0x21, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xc0, 0x8f, 0xc0, 0x71, 0x00, 0x10, 0x80, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x6f, 0x06, 0x63, 0x6e, 0x3d, 0x30, 0x2c, 0x31, 0x07, 0x64, 0x61, 0x3d, 0x74, 0x72, 0x75, 0x65, 0x06, 0x65, 0x74, 0x3d, 0x30, 0x2c, 0x34, 0x0d, 0x66, 0x74, 0x3d, 0x30, 0x78, 0x34, 0x34, 0x34, 0x46, 0x38, 0x41, 0x30, 0x30, 0x08, 0x6d, 0x64, 0x3d, 0x30, 0x2c, 0x31, 0x2c, 0x32, 0x09, 0x61, 0x6d, 0x3d, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0x11, 0x66, 0x76, 0x3d, 0x73, 0x31, 0x30, 0x35, 0x32, 0x39, 0x34, 0x2e, 0x31, 0x31, 0x30, 0x32, 0x2e, 0x30, 0x05, 0x73, 0x64, 0x3d, 0x39, 0x39, 0x06, 0x73, 0x66, 0x3d, 0x30, 0x78, 0x34, 0x06, 0x74, 0x70, 0x3d, 0x55, 0x44, 0x50, 0x08, 0x76, 0x6e, 0x3d, 0x36, 0x35, 0x35, 0x33, 0x37, 0x08, 0x76, 0x73, 0x3d, 0x31, 0x39, 0x30, 0x2e, 0x39, 0xc0, 0x8f, 0x00, 0x2f, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0xc0, 0x28, 0x00, 0x2f, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0xc0, 0x4e, 0x00, 0x2f, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0xc0, 0x71, 0x00, 0x2f, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00 } },
	/*
	 * IPv6
	 */
	{ 182, { 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x18, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d, 0x5f, 0x5f, 0x61, 0x74, 0x5f, 0x5f, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x3a, 0x31, 0x32, 0x33, 0x34, 0x05, 0x5f, 0x68, 0x74, 0x74, 0x70, 0x04, 0x5f, 0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00, 0x21, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x08, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0xc0, 0x30, 0xc0, 0x47, 0x00, 0x1c, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x10, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f, 0x70, 0x81, 0x92, 0xa3, 0xb4, 0xc5, 0xd6, 0xe7, 0xf8, 0x09, 0xc0, 0x47, 0x00, 0x1c, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x10, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f, 0x70, 0x81, 0x92, 0xa3, 0xb4, 0xc5, 0xd6, 0xe7, 0xf8, 0x09, 0xc0, 0x47, 0x00, 0x1c, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x10, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f, 0x70, 0x81, 0x92, 0xa3, 0xb4, 0xc5, 0xd6, 0xe7, 0xf8, 0x09, 0xc0, 0x47, 0x00, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04, 0xc0, 0xa8, 0x01, 0x80 } }
};

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	uv_timer_stop(timer_req);

	uv_stop(uv_default_loop());
}

static void create_packet(struct mdns_packet_t *pkt) {
	pkt->id = (0xAA << 8) | 0xBB;
	pkt->qr = step;
	pkt->opcode = 2;
	pkt->aa = 0;
	pkt->tc = 1;
	pkt->rd = 0;
	pkt->ra = 1;
	pkt->z = 0;
	pkt->ad = 1;
	pkt->cd = 0;
	pkt->rcode = 1;
	pkt->nranswers = 5;
	pkt->rr_add = 5;
	pkt->rr_auth = 1;

	int i = 0;
	unsigned char foo = 0xF9;
	if(pkt->qr == 0) {
		pkt->nrqueries = 3;
		if((pkt->queries = MALLOC(sizeof(struct mdns_query_t *)*4)) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[0] = MALLOC(sizeof(struct mdns_query_t))) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[1] = MALLOC(sizeof(struct mdns_query_t))) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[2] = MALLOC(sizeof(struct mdns_query_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->queries[0], 0, sizeof(struct mdns_query_t));
		memset(pkt->queries[1], 0, sizeof(struct mdns_query_t));
		memset(pkt->queries[2], 0, sizeof(struct mdns_query_t));

		if((pkt->queries[0]->query.name = STRDUP("testname.local.foo")) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[1]->query.name = STRDUP("testname1.local1.foo")) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[2]->query.name = STRDUP("testname.local1.foo")) == NULL) {
			OUT_OF_MEMORY
		}
		pkt->queries[0]->query.qu = 0;
		pkt->queries[0]->query.type = 33;
		pkt->queries[0]->query.class = 1;
		pkt->queries[1]->query.qu = 0;
		pkt->queries[1]->query.type = 33;
		pkt->queries[1]->query.class = 1;
		pkt->queries[2]->query.qu = 0;
		pkt->queries[2]->query.type = 33;
		pkt->queries[2]->query.class = 1;
	} else {
		pkt->nrqueries = 5;
		if((pkt->queries = MALLOC(sizeof(struct mdns_payload_t *)*5)) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[0] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->queries[0], 0, sizeof(struct mdns_payload_t));
		if((pkt->queries[0]->payload.name = STRDUP("testname.local.foo")) == NULL) {
			OUT_OF_MEMORY
		}
		pkt->queries[0]->payload.type = 12;
		if((pkt->queries[0]->payload.data.domain = STRDUP("testname.local.foo")) == NULL) {
			OUT_OF_MEMORY
		}

		if((pkt->queries[1] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->queries[1], 0, sizeof(struct mdns_payload_t));
		if((pkt->queries[1]->payload.name = STRDUP("pilight.local")) == NULL) {
			OUT_OF_MEMORY
		}
		pkt->queries[1]->payload.type = 33;
		pkt->queries[1]->payload.priority = 1;
		pkt->queries[1]->payload.weight = 1;
		pkt->queries[1]->payload.port = 80;
		if((pkt->queries[1]->payload.data.domain = STRDUP("pilight.daemon.local")) == NULL) {
			OUT_OF_MEMORY
		}

		if((pkt->queries[2] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->queries[2], 0, sizeof(struct mdns_payload_t));
		if((pkt->queries[2]->payload.name = STRDUP("pilight.local1")) == NULL) {
			OUT_OF_MEMORY
		}
		pkt->queries[2]->payload.type = 1;
		pkt->queries[2]->payload.data.ip4[0] = 192;
		pkt->queries[2]->payload.data.ip4[1] = 168;
		pkt->queries[2]->payload.data.ip4[2] = 1;
		pkt->queries[2]->payload.data.ip4[3] = 2;

		if((pkt->queries[3] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->queries[3], 0, sizeof(struct mdns_payload_t));
		if((pkt->queries[3]->payload.name = STRDUP("pilight.local1")) == NULL) {
			OUT_OF_MEMORY
		}
		pkt->queries[3]->payload.type = 28;
		foo = 0xF9;

		for(i=0;i<16;i++) {
			pkt->queries[3]->payload.data.ip6[i] = foo += 0x11;
		}

		if((pkt->queries[4] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->queries[4], 0, sizeof(struct mdns_payload_t));
		if((pkt->queries[4]->payload.name = STRDUP("pilight.local1")) == NULL) {
			OUT_OF_MEMORY
		}
		pkt->queries[4]->payload.type = 16;

		pkt->queries[4]->payload.data.text.nrvalues = 2;
		if((pkt->queries[4]->payload.data.text.values = MALLOC(sizeof(char *)*pkt->queries[4]->payload.data.text.nrvalues)) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[4]->payload.data.text.values[0] = STRDUP("foo")) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->queries[4]->payload.data.text.values[1] = STRDUP("lorem")) == NULL) {
			OUT_OF_MEMORY
		}
	}

	if((pkt->answers = MALLOC(sizeof(struct mdns_payload_t *)*5)) == NULL) {
		OUT_OF_MEMORY
	}
	if((pkt->answers[0] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->answers[0], 0, sizeof(struct mdns_payload_t));
	if((pkt->answers[0]->name = STRDUP("testname.local.foo")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->answers[0]->type = 12;
	if((pkt->answers[0]->data.domain = STRDUP("testname.local.foo")) == NULL) {
		OUT_OF_MEMORY
	}

	if((pkt->answers[1] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->answers[1], 0, sizeof(struct mdns_payload_t));
	if((pkt->answers[1]->name = STRDUP("pilight.local")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->answers[1]->type = 33;
	pkt->answers[1]->priority = 1;
	pkt->answers[1]->weight = 1;
	pkt->answers[1]->port = 80;
	if((pkt->answers[1]->data.domain = STRDUP("pilight.daemon.local")) == NULL) {
		OUT_OF_MEMORY
	}

	if((pkt->answers[2] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->answers[2], 0, sizeof(struct mdns_payload_t));
	if((pkt->answers[2]->name = STRDUP("pilight.local1")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->answers[2]->type = 1;
	pkt->answers[2]->data.ip4[0] = 192;
	pkt->answers[2]->data.ip4[1] = 168;
	pkt->answers[2]->data.ip4[2] = 1;
	pkt->answers[2]->data.ip4[3] = 2;

	if((pkt->answers[3] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->answers[3], 0, sizeof(struct mdns_payload_t));
	if((pkt->answers[3]->name = STRDUP("pilight.local1")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->answers[3]->type = 28;
	foo = 0xF9;
	for(i=0;i<16;i++) {
		pkt->answers[3]->data.ip6[i] = foo += 0x11;
	}

	if((pkt->answers[4] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->answers[4], 0, sizeof(struct mdns_payload_t));
	if((pkt->answers[4]->name = STRDUP("pilight.local1")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->answers[4]->type = 16;

	pkt->answers[4]->data.text.nrvalues = 2;
	if((pkt->answers[4]->data.text.values = MALLOC(sizeof(char *)*pkt->answers[4]->data.text.nrvalues)) == NULL) {
		OUT_OF_MEMORY
	}
	if((pkt->answers[4]->data.text.values[0] = STRDUP("foo")) == NULL) {
		OUT_OF_MEMORY
	}
	if((pkt->answers[4]->data.text.values[1] = STRDUP("lorem")) == NULL) {
		OUT_OF_MEMORY
	}

	if((pkt->records = MALLOC(sizeof(struct mdns_payload_t *)*5)) == NULL) {
		OUT_OF_MEMORY
	}
	if((pkt->records[0] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->records[0], 0, sizeof(struct mdns_payload_t));
	if((pkt->records[0]->name = STRDUP("testname.local.foo")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->records[0]->type = 12;
	if((pkt->records[0]->data.domain = STRDUP("testname.local.foo")) == NULL) {
		OUT_OF_MEMORY
	}

	if((pkt->records[1] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->records[1], 0, sizeof(struct mdns_payload_t));
	if((pkt->records[1]->name = STRDUP("pilight.local")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->records[1]->type = 33;
	pkt->records[1]->priority = 1;
	pkt->records[1]->weight = 1;
	pkt->records[1]->port = 80;
	if((pkt->records[1]->data.domain = STRDUP("pilight.daemon.local")) == NULL) {
		OUT_OF_MEMORY
	}

	if((pkt->records[2] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->records[2], 0, sizeof(struct mdns_payload_t));
	if((pkt->records[2]->name = STRDUP("pilight.local1")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->records[2]->type = 1;
	pkt->records[2]->data.ip4[0] = 192;
	pkt->records[2]->data.ip4[1] = 168;
	pkt->records[2]->data.ip4[2] = 1;
	pkt->records[2]->data.ip4[3] = 2;

	if((pkt->records[3] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->records[3], 0, sizeof(struct mdns_payload_t));
	if((pkt->records[3]->name = STRDUP("pilight.local1")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->records[3]->type = 28;
	foo = 0xF9;

	for(i=0;i<16;i++) {
		pkt->records[3]->data.ip6[i] = foo += 0x11;
	}

	if((pkt->records[4] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(pkt->records[4], 0, sizeof(struct mdns_payload_t));
	if((pkt->records[4]->name = STRDUP("pilight.local1")) == NULL) {
		OUT_OF_MEMORY
	}
	pkt->records[4]->type = 16;

	pkt->records[4]->data.text.nrvalues = 2;
	if((pkt->records[4]->data.text.values = MALLOC(sizeof(char *)*pkt->records[4]->data.text.nrvalues)) == NULL) {
		OUT_OF_MEMORY
	}
	if((pkt->records[4]->data.text.values[0] = STRDUP("foo")) == NULL) {
		OUT_OF_MEMORY
	}
	if((pkt->records[4]->data.text.values[1] = STRDUP("lorem")) == NULL) {
		OUT_OF_MEMORY
	}
}

static void mdns_wait(void *param) {
	struct timeval tv;
	struct sockaddr_in addr;
	int n = 0, r = 0;
	socklen_t addrlen = sizeof(addr);
	fd_set fdsread;
	char buffer[8096];

#ifdef _WIN32
	unsigned long on = 1;
	r = ioctlsocket(mdns_socket, FIONBIO, &on);
	assert(r >= 0);
#else
	long arg = fcntl(mdns_socket, F_GETFL, NULL);
	r = fcntl(mdns_socket, F_SETFL, arg | O_NONBLOCK);
	CuAssertTrue(gtc, (r >= 0));
#endif

	while(mdns_loop) {
		FD_ZERO(&fdsread);
		FD_SET((unsigned long)mdns_socket, &fdsread);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		do {
			n = select(mdns_socket+1, &fdsread, NULL, NULL, &tv);
		} while(n == -1 && errno == EINTR && mdns_loop);

		if(n == 0) {
			continue;
		}
		if(mdns_loop == 0) {
			break;
		}
		if(n == -1) {
			goto clear;
		} else if(n > 0) {
			if(FD_ISSET(mdns_socket, &fdsread)) {
				memset(buffer, '\0', sizeof(buffer));
				r = recvfrom(mdns_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);
				if(step == 0) {
					CuAssertIntEquals(gtc, r, 316);
				} else if(step == 1) {
					CuAssertIntEquals(gtc, r, 372);
				}

				struct mdns_packet_t pkt;
				memset(&pkt, 0, sizeof(struct mdns_packet_t));
				CuAssertIntEquals(gtc, mdns_decode(&pkt, (unsigned char *)buffer, r), 0);

				CuAssertIntEquals(gtc, 0xAABB, pkt.id);
				CuAssertIntEquals(gtc, step, pkt.qr);
				CuAssertIntEquals(gtc, 2, pkt.opcode);
				CuAssertIntEquals(gtc, 0, pkt.aa);
				CuAssertIntEquals(gtc, 1, pkt.tc);
				CuAssertIntEquals(gtc, 0, pkt.rd);
				CuAssertIntEquals(gtc, 1, pkt.ra);
				CuAssertIntEquals(gtc, 0, pkt.z);
				CuAssertIntEquals(gtc, 1, pkt.ad);
				CuAssertIntEquals(gtc, 0, pkt.cd);
				CuAssertIntEquals(gtc, 1, pkt.rcode);
				if(step == 0) {
					CuAssertIntEquals(gtc, 3, pkt.nrqueries);
				} else if(step == 1) {
					CuAssertIntEquals(gtc, 5, pkt.nrqueries);
				}
				CuAssertIntEquals(gtc, 5, pkt.nranswers);
				CuAssertIntEquals(gtc, 1, pkt.rr_auth);
				CuAssertIntEquals(gtc, 5, pkt.rr_add);

				int x = 0;
				for(x=0;x<pkt.nrqueries;x++) {
					if(pkt.qr == 0) {
						switch(x) {
							case 0: {
								CuAssertStrEquals(gtc, "testname.local.foo", pkt.queries[x]->query.name);
							} break;
							case 1: {
								CuAssertStrEquals(gtc, "testname1.local1.foo", pkt.queries[x]->query.name);
							} break;
							case 2: {
								CuAssertStrEquals(gtc, "testname.local1.foo", pkt.queries[x]->query.name);
							} break;
						}
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->query.qu);
						CuAssertIntEquals(gtc, 33, pkt.queries[x]->query.type);
						CuAssertIntEquals(gtc, 1, pkt.queries[x]->query.class);
					} else {
						switch(x) {
							case 0: {
								CuAssertStrEquals(gtc, "testname.local.foo", pkt.queries[x]->payload.name);
								CuAssertStrEquals(gtc, "testname.local.foo", pkt.queries[x]->payload.data.domain);
								CuAssertIntEquals(gtc, 12, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 18, pkt.queries[x]->payload.length);
							} break;
							case 1: {
								CuAssertStrEquals(gtc, "pilight.local", pkt.queries[x]->payload.name);
								CuAssertStrEquals(gtc, "pilight.daemon.local", pkt.queries[x]->payload.data.domain);
								CuAssertIntEquals(gtc, 33, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.priority);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.weight);
								CuAssertIntEquals(gtc, 80, pkt.queries[x]->payload.port);
								CuAssertIntEquals(gtc, 20, pkt.queries[x]->payload.length);
							} break;
							case 2: {
								CuAssertStrEquals(gtc, "pilight.local1", pkt.queries[x]->payload.name);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 4, pkt.queries[x]->payload.length);
								CuAssertIntEquals(gtc, 192, pkt.queries[x]->payload.data.ip4[0]);
								CuAssertIntEquals(gtc, 168, pkt.queries[x]->payload.data.ip4[1]);
								CuAssertIntEquals(gtc, 1, pkt.queries[x]->payload.data.ip4[2]);
								CuAssertIntEquals(gtc, 2, pkt.queries[x]->payload.data.ip4[3]);
							} break;
							case 3: {
								CuAssertStrEquals(gtc, "pilight.local1", pkt.queries[x]->payload.name);
								CuAssertIntEquals(gtc, 28, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 16, pkt.queries[x]->payload.length);
								int i = 0;
								unsigned char foo = 0xF9;
								for(i=0;i<16;i++) {
									foo += 0x11;
									CuAssertIntEquals(gtc, foo, pkt.queries[x]->payload.data.ip6[i]);
								}
							} break;
							case 4: {
								CuAssertStrEquals(gtc, "pilight.local1", pkt.queries[x]->payload.name);
								CuAssertIntEquals(gtc, 16, pkt.queries[x]->payload.type);
								CuAssertIntEquals(gtc, 10, pkt.queries[x]->payload.length);
								CuAssertStrEquals(gtc, "foo", pkt.queries[x]->payload.data.text.values[0]);
								CuAssertStrEquals(gtc, "lorem", pkt.queries[x]->payload.data.text.values[1]);
							} break;
						}
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->payload.flush);
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->payload.class);
						CuAssertIntEquals(gtc, 0, pkt.queries[x]->payload.ttl);
					}
				}

				for(x=0;x<pkt.nranswers;x++) {
					switch(x) {
						case 0: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->name);
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.answers[x]->data.domain);
							CuAssertIntEquals(gtc, 12, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 18, pkt.answers[x]->length);
						} break;
						case 1: {
							CuAssertStrEquals(gtc, "pilight.local", pkt.answers[x]->name);
							CuAssertStrEquals(gtc, "pilight.daemon.local", pkt.answers[x]->data.domain);
							CuAssertIntEquals(gtc, 33, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->priority);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->weight);
							CuAssertIntEquals(gtc, 80, pkt.answers[x]->port);
							CuAssertIntEquals(gtc, 20, pkt.answers[x]->length);
						} break;
						case 2: {
							CuAssertStrEquals(gtc, "pilight.local1", pkt.answers[x]->name);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 4, pkt.answers[x]->length);
							CuAssertIntEquals(gtc, 192, pkt.answers[x]->data.ip4[0]);
							CuAssertIntEquals(gtc, 168, pkt.answers[x]->data.ip4[1]);
							CuAssertIntEquals(gtc, 1, pkt.answers[x]->data.ip4[2]);
							CuAssertIntEquals(gtc, 2, pkt.answers[x]->data.ip4[3]);
						} break;
						case 3: {
							CuAssertStrEquals(gtc, "pilight.local1", pkt.answers[x]->name);
							CuAssertIntEquals(gtc, 28, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 16, pkt.answers[x]->length);
							int i = 0;
							unsigned char foo = 0xF9;
							for(i=0;i<16;i++) {
								foo += 0x11;
								CuAssertIntEquals(gtc, foo, pkt.answers[x]->data.ip6[i]);
							}
						} break;
						case 4: {
							CuAssertStrEquals(gtc, "pilight.local1", pkt.answers[x]->name);
							CuAssertIntEquals(gtc, 16, pkt.answers[x]->type);
							CuAssertIntEquals(gtc, 10, pkt.answers[x]->length);
							CuAssertStrEquals(gtc, "foo", pkt.answers[x]->data.text.values[0]);
							CuAssertStrEquals(gtc, "lorem", pkt.answers[x]->data.text.values[1]);
						} break;
					}
					CuAssertIntEquals(gtc, 0, pkt.answers[x]->flush);
					CuAssertIntEquals(gtc, 0, pkt.answers[x]->class);
					CuAssertIntEquals(gtc, 0, pkt.answers[x]->ttl);
				}

				for(x=0;x<pkt.rr_add;x++) {
					switch(x) {
						case 0: {
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->name);
							CuAssertStrEquals(gtc, "testname.local.foo", pkt.records[x]->data.domain);
							CuAssertIntEquals(gtc, 12, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 18, pkt.records[x]->length);
						} break;
						case 1: {
							CuAssertStrEquals(gtc, "pilight.local", pkt.records[x]->name);
							CuAssertStrEquals(gtc, "pilight.daemon.local", pkt.records[x]->data.domain);
							CuAssertIntEquals(gtc, 33, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->priority);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->weight);
							CuAssertIntEquals(gtc, 80, pkt.records[x]->port);
							CuAssertIntEquals(gtc, 20, pkt.records[x]->length);
						} break;
						case 2: {
							CuAssertStrEquals(gtc, "pilight.local1", pkt.records[x]->name);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 4, pkt.records[x]->length);
							CuAssertIntEquals(gtc, 192, pkt.records[x]->data.ip4[0]);
							CuAssertIntEquals(gtc, 168, pkt.records[x]->data.ip4[1]);
							CuAssertIntEquals(gtc, 1, pkt.records[x]->data.ip4[2]);
							CuAssertIntEquals(gtc, 2, pkt.records[x]->data.ip4[3]);
						} break;
						case 3: {
							CuAssertStrEquals(gtc, "pilight.local1", pkt.records[x]->name);
							CuAssertIntEquals(gtc, 28, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 16, pkt.records[x]->length);
							int i = 0;
							unsigned char foo = 0xF9;
							for(i=0;i<16;i++) {
								foo += 0x11;
								CuAssertIntEquals(gtc, foo, pkt.records[x]->data.ip6[i]);
							}
						} break;
						case 4: {
							CuAssertStrEquals(gtc, "pilight.local1", pkt.records[x]->name);
							CuAssertIntEquals(gtc, 16, pkt.records[x]->type);
							CuAssertIntEquals(gtc, 10, pkt.records[x]->length);
							CuAssertStrEquals(gtc, "foo", pkt.records[x]->data.text.values[0]);
							CuAssertStrEquals(gtc, "lorem", pkt.records[x]->data.text.values[1]);
						} break;
					}
					CuAssertIntEquals(gtc, 0, pkt.records[x]->flush);
					CuAssertIntEquals(gtc, 0, pkt.records[x]->class);
					CuAssertIntEquals(gtc, 0, pkt.records[x]->ttl);
				}

				mdns_free(&pkt);

				r = sendto(mdns_socket, messages[0].msg, messages[0].len, 0, (struct sockaddr *)&addr, sizeof(addr));
				CuAssertIntEquals(gtc, r, messages[0].len);
			}
		}
	}

clear:
	return;
}

static void read_cb(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata) {
	unsigned int len = 0, i = 0;
	unsigned char *buf = mdns_encode(pkt, &len);

	// mdns_dump(pkt);

	struct sockaddr_in addr1;
	memset((char *)&addr1, '\0', sizeof(addr));
	addr1.sin_family = AF_INET;
	addr1.sin_addr.s_addr = inet_addr("224.0.0.251");
#ifdef PILIGHT_UNITTEST
	addr1.sin_port = htons(15353);
#else
	addr1.sin_port = htons(5353);
#endif

	switch(run) {
		case 0: {
			CuAssertIntEquals(gtc, messages[0].len, len);

			for(i=0;i<len;i++) {
				CuAssertIntEquals(gtc, messages[0].msg[i], buf[i]);
			}
			if(step == 0) {
				step++;
				struct mdns_packet_t pkt;
				memset(&pkt, 0, sizeof(struct mdns_packet_t));
				create_packet(&pkt);
				mdns_send(&pkt, read_cb, NULL);
				mdns_free(&pkt);
			} else if(step == 1) {
				check = 1;
				mdns_loop = 0;
				uv_stop(uv_default_loop());
			}
		} break;
		case 1: {
			CuAssertIntEquals(gtc, messages[step].len, len);

			for(i=0;i<len;i++) {
				CuAssertIntEquals(gtc, messages[step].msg[i], buf[i]);
			}
			step++;
			if(step == sizeof(messages)/sizeof(messages[0])) {
				check = 1;
				mdns_loop = 0;
				uv_stop(uv_default_loop());
			} else {
				int r = sendto(mdns_socket, messages[step].msg, messages[step].len, 0, (struct sockaddr *)&addr1, sizeof(addr1));
				CuAssertTrue(gtc, (r == messages[step].len));
			}
		} break;
	}
	FREE(buf);
}

static void mdns_server_tx(void) {
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	mdns_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (mdns_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef PILIGHT_UNITTEST
	addr.sin_port = htons(15353);
#else
	addr.sin_port = htons(5353);
#endif

	r = setsockopt(mdns_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	inet_pton(AF_INET, "224.0.0.251", &mreq.imr_multiaddr.s_addr);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	r = setsockopt(mdns_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(mdns_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));
}

static void mdns_server_rx(void) {
	struct sockaddr_in addr;
	int opt = 1;
	int r = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "could not initialize new socket\n");
		exit(EXIT_FAILURE);
	}
#endif

	mdns_socket = socket(AF_INET, SOCK_DGRAM, 0);
	CuAssertTrue(gtc, (mdns_socket > 0));

	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("224.0.0.251");
#ifdef PILIGHT_UNITTEST
	addr.sin_port = htons(15353);
#else
	addr.sin_port = htons(5353);
#endif

	r = setsockopt(mdns_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
	CuAssertTrue(gtc, (r >= 0));

	r = bind(mdns_socket, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r >= 0));

	r = sendto(mdns_socket, messages[step].msg, messages[step].len, 0, (struct sockaddr *)&addr, sizeof(addr));
	CuAssertTrue(gtc, (r == messages[step].len));
}

static void stop(void) {
	uv_async_send(async_close_req);
}

static void test_mdns_tx(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;
	run = 0;
	step = 0;

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	/*
	 * Don't make this too quick so we can properly test the
	 * timeout of the mdns library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	struct mdns_packet_t pkt;
	memset(&pkt, 0, sizeof(struct mdns_packet_t));

	create_packet(&pkt);

	mdns_server_tx();

	uv_thread_create(&pth, mdns_wait, NULL);

	mdns_send(&pkt, read_cb, NULL);
	mdns_free(&pkt);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	mdns_loop = 0;
	uv_thread_join(&pth);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();
	mdns_gc();

	CuAssertIntEquals(tc, 0, run);
	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_mdns_rx(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;
	run = 1;
	step = 0;

	async_close_req = MALLOC(sizeof(uv_async_t));
	if(async_close_req == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_async_init(uv_default_loop(), async_close_req, async_close_cb);

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	/*
	 * Don't make this too quick so we can properly test the
	 * timeout of the mdns library itself.
	 */
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

	mdns_listen(read_cb, NULL);
	mdns_server_rx();

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	eventpool_gc();
	mdns_gc();

	CuAssertIntEquals(tc, 4, step);
	CuAssertIntEquals(tc, 1, check);
	CuAssertIntEquals(tc, 0, xfree());
}

static void test_mdns_truncated(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	{
		struct mdns_packet_t pkt;
		unsigned char buf1[] = { 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x05, 0x5f, 0x68, 0x74, 0x74, 0x70, 0x04, 0x5f, 0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x1c, 0x0b, 0x73, 0x6f, 0x6e, 0x6f, 0x66, 0x66, 0x2d, 0x38, 0x30, 0x36, 0x36, 0xc0, 0x0c, 0xc0, 0x28, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x00, 0xc0, 0x28, 0x00, 0x21, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x0b, 0x73, 0x6f, 0x6e, 0x6f, 0x66, 0x66, 0x2d, 0x38, 0x30, 0x36, 0x36, 0xc0};
		memset(&pkt, 0, sizeof(struct mdns_packet_t));
		CuAssertTrue(tc, mdns_decode(&pkt, buf1, sizeof(buf1)) < 0);
	}

	{
		struct mdns_packet_t pkt;
		unsigned char buf1[] = { 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x05, 0x5f, 0x68, 0x74, 0x74, 0x70, 0x04, 0x5f, 0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x1f, 0x0e, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x20, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x0c, 0x08, 0x5f, 0x61, 0x69, 0x72, 0x70, 0x6c, 0x61, 0x79, 0xc0, 0x12, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x22, 0x0e, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x20, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x39, 0x05, 0x5f, 0x72, 0x61, 0x6f, 0x70, 0xc0, 0x12, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x11, 0x94, 0x00, 0x2c, 0x1b, 0x30, 0x30, 0x30, 0x36, 0x37, 0x38, 0x32, 0x35, 0x32, 0x34, 0x46, 0x35, 0x40, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x20, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x5f, 0x0e, 0x4d, 0x61, 0x72, 0x61, 0x6e, 0x74, 0x7a, 0x2d, 0x53, 0x52, 0x36, 0x30, 0x30, 0x39, 0xc0, 0x17, 0x00, 0x01 };
		memset(&pkt, 0, sizeof(struct mdns_packet_t));
		CuAssertTrue(tc, mdns_decode(&pkt, buf1, sizeof(buf1)) < 0);
	}

	eventpool_gc();
	mdns_gc();

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_mdns(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_mdns_tx);
	SUITE_ADD_TEST(suite, test_mdns_rx);
	SUITE_ADD_TEST(suite, test_mdns_truncated);

	return suite;
}
