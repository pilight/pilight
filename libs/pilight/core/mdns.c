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

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/mem.h"

#include "mdns.h"

#include "../../libuv/uv.h"

#define LISTEN	0
#define SEND		1

typedef struct mdns_data_t {
	void (*callback)(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata);
	void *userdata;
	int type;
} mdns_data_t;

static uv_mutex_t lock;
static int lock_init = 0;
static struct mdns_data_t **data = NULL;
static int nrdata = 0;

void mdns_gc(void) {
	int i = 0;
	uv_mutex_lock(&lock);
	for(i=0;i<nrdata;i++) {
		FREE(data[i]);
	}
	nrdata = 0;
	uv_mutex_unlock(&lock);
	if(data != NULL) {
		FREE(data);
	}
}

static void alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	if((buf->base = malloc(len)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(buf->base, 0, len);
}


static void write_name(struct mdns_packet_t *pkt, unsigned char **buf, unsigned int *len, char *name) {
	char **suffices = NULL, *ptr = 0;
	int i = 0, l = strlen(name), count = 0;
	int x = 0, pos = 0, nrsuffices = 0, match = 0;
	for(i=0;i<l;i++) {
		if(name[i] == '.') {
			count++;
		}
	}
	count++;

	if((suffices = MALLOC(sizeof(char *)*(count))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(suffices, 0, sizeof(char *)*count);
	
	nrsuffices = 0, i = 0, count = 0;
	while((ptr = strstr(&name[i], ".")) != NULL) {
		if((suffices[nrsuffices++] = STRDUP(&name[i])) == NULL) {
			OUT_OF_MEMORY
		}
		if(match == 0) {
			for(count=0;count<pkt->nrcache;count++) {
				if(strcmp(pkt->cache[count]->suffix, &name[i]) == 0) {
					match = 1;
					count = nrsuffices - 1;
					break;
				}
			}
		}
		i = (ptr - name) + 1;
	}

	if((suffices[nrsuffices++] = STRDUP(&name[i])) == NULL) {
		OUT_OF_MEMORY
	}

	if(match == 0) {
		for(count=0;count<pkt->nrcache;count++) {
			if(strcmp(pkt->cache[count]->suffix, &name[i]) == 0) {
				match = 1;
				count = nrsuffices - 1;
				break;
			}
		}
		if(match == 0) {
			count = nrsuffices;
		}
	}

	l = strlen(name);

	if((pkt->cache = REALLOC(pkt->cache, sizeof(struct mdns_cache_t *)*(pkt->nrcache+nrsuffices))) == NULL) {
		OUT_OF_MEMORY
	}
	for(i=0;i<nrsuffices;i++) {
		if((pkt->cache[pkt->nrcache] = MALLOC(sizeof(struct mdns_cache_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->cache[pkt->nrcache], 0, sizeof(struct mdns_cache_t));
		if((pkt->cache[pkt->nrcache]->suffix = STRDUP(suffices[i])) == NULL) {
			OUT_OF_MEMORY
		}
		pkt->cache[pkt->nrcache]->pos = *len + strlen(name) - strlen(suffices[i]);
		pkt->nrcache++;
	}

	for(i=0;i<count;i++) {
		if((ptr = strstr(suffices[i], ".")) != NULL) {
			pos = ptr - suffices[i];
			suffices[i][pos] = '\0';
		}
		l = strlen(suffices[i]);
		if((*buf = REALLOC(*buf, (*len)+l+2)) == NULL) {
			OUT_OF_MEMORY
		}
		(*buf)[(*len)++] = l;
		for(x=0;x<l;x++) {
			(*buf)[(*len)++] = suffices[i][x];
		}
		if(ptr != NULL) {
			suffices[i][pos] = '.';
		}
	}

	if(count != nrsuffices) {
		unsigned int index = -1;
		for(i=0;i<pkt->nrcache;i++) {
			if(strcmp(pkt->cache[i]->suffix, suffices[count]) == 0) {
				index = pkt->cache[i]->pos;
				break;
			}
		}

		if((*buf = REALLOC(*buf, (*len)+2)) == NULL) {
			OUT_OF_MEMORY
		}
		(*buf)[(*len)++] = (index >> 8) | 0xC0;
		(*buf)[(*len)++] = index & 0xFF;
	} else {
		(*buf)[(*len)++] = 0;
	}

	for(i=0;i<nrsuffices;i++) {
		FREE(suffices[i]);
	}
	FREE(suffices);
}

static void encode_payload(struct mdns_packet_t *pkt, int type, unsigned char **buf, unsigned int *len) {
	struct mdns_payload_t *payload = NULL;
	int x = 0, nr = 0;
	unsigned int length_bits = 0;
	switch(type) {
		/*case 0: {
			nr = pkt->nrqueries;
		} break;*/
		case 1: {
			nr = pkt->nranswers;
		} break;
		case 2: {
			nr = pkt->rr_add;
		} break;
	}

	for(x=0;x<nr;x++) {
		switch(type) {
			/*case 0: {
				payload = &pkt->queries[x]->payload;
			} break;*/
			case 1: {
				payload = pkt->answers[x];
			} break;
			case 2: {
				payload = pkt->records[x];
			} break;
			default:
			break;
		}
		write_name(pkt, buf, len, payload->name);

		if((*buf = REALLOC(*buf, (*len)+32)) == NULL) {
			OUT_OF_MEMORY
		}
		(*buf)[(*len)++] = (payload->type >> 8) & 0xFF;
		(*buf)[(*len)++] = payload->type & 0xFF;
		switch(payload->type) {
			case 1:
			case 12:
			case 16:
			case 28:
			case 33:
			case 47: {
				(*buf)[(*len)] = (payload->flush & 0x1) << 7;
				(*buf)[(*len)++] |= (payload->class >> 8) & 0x7F;
				(*buf)[(*len)++] = payload->class & 0xFF;
				(*buf)[(*len)++] = (payload->ttl >> 24) & 0xFF;
				(*buf)[(*len)++] = (payload->ttl >> 16) & 0xFF;
				(*buf)[(*len)++] = (payload->ttl >> 8) & 0xFF;
				(*buf)[(*len)++] = payload->ttl & 0xFF;
			} break;
		}
		switch(payload->type) {
			case 1: {
				payload->length = 4;
			} break;
			case 16: {
				int i = 0;
				payload->length = 0;
				for(i=0;i<payload->data.text.nrvalues;i++) {
					payload->length += strlen(payload->data.text.values[i])+1;
				}
			} break;
			case 28: {
				payload->length = 16;
			} break;
			case 12:
			case 33: {
				payload->length = strlen(payload->data.domain);
			} break;
		}
		switch(payload->type) {
			case 1:
			case 12:
			case 16:
			case 28:
			case 33: {
				length_bits = (*len);
				(*buf)[(*len)++] = (payload->length >> 8) & 0xFF;
				(*buf)[(*len)++] = payload->length & 0xFF;
			} break;
			case 47: {
				/*
				 * Unsupported
				 */
				(*buf)[(*len)++] = 0;
				(*buf)[(*len)++] = 0;
			} break;
		}
		switch(payload->type) {
			case 33: {
				(*buf)[(*len)++] = (payload->priority >> 8) & 0xFF;
				(*buf)[(*len)++] = payload->priority & 0xFF;
				(*buf)[(*len)++] = (payload->weight >> 8) & 0xFF;
				(*buf)[(*len)++] = payload->weight & 0xFF;
				(*buf)[(*len)++] = (payload->port >> 8) & 0xFF;
				(*buf)[(*len)++] = payload->port & 0xFF;
			} break;
		}
		switch(payload->type) {
			case 1: {
				(*buf)[(*len)++] = payload->data.ip4[0] & 0xFF;
				(*buf)[(*len)++] = payload->data.ip4[1] & 0xFF;
				(*buf)[(*len)++] = payload->data.ip4[2] & 0xFF;
				(*buf)[(*len)++] = payload->data.ip4[3] & 0xFF;
			} break;
			case 16: {
				if((*buf = REALLOC(*buf, (*len)+payload->length)) == NULL) {
					OUT_OF_MEMORY
				}

				int i = 0, y = 0, l = 0;
				for(i=0;i<payload->data.text.nrvalues;i++) {
					l = strlen(payload->data.text.values[i]);
					(*buf)[(*len)++] = l;
					for(y=0;y<l;y++) {
						(*buf)[(*len)++] = payload->data.text.values[i][y];
					}
				}
			} break;
			case 12:
			case 33: {
				unsigned int slen = *len;
				write_name(pkt, buf, len, payload->data.domain);

				payload->length = (*len)-slen;
				if(payload->type == 33) {
					payload->length += 6;
				}
				(*buf)[length_bits] = (payload->length >> 8) & 0xFF;
				(*buf)[length_bits+1] = payload->length & 0xFF;
			} break;
			case 28: {
				int i = 0;
				for(i=0;i<16;i++) {
					(*buf)[(*len)++] = payload->data.ip6[i] & 0xFF;
				}
			} break;
		}
	}
}

unsigned char *mdns_encode(struct mdns_packet_t *pkt, unsigned int *len) {
	int x = 0;
	unsigned char *buf = MALLOC(12);
	memset(buf, 0, 12);

	buf[0] = (pkt->id >> 8) & 0xFF;
	buf[1] = pkt->id & 0xFF;
	buf[2] = (pkt->qr & 0x1) << 7;
	buf[2] |= (pkt->opcode & 0x0F) << 3;
	buf[2] |= (pkt->aa & 0x01) << 2;
	buf[2] |= (pkt->tc & 0x01) << 1;
	buf[2] |= (pkt->rd & 0x01);
	buf[3] = (pkt->ra & 0x1) << 7;
	buf[3] |= (pkt->z & 0x1) << 6;
	buf[3] |= (pkt->ad & 0x1) << 5;
	buf[3] |= (pkt->cd & 0x1) << 4;
	buf[3] |= (pkt->rcode & 0x0F);

	buf[4] = (pkt->nrqueries >> 8) & 0xFF;
	buf[5] = pkt->nrqueries & 0xFF;
	buf[6] = (pkt->nranswers >> 8) & 0xFF;
	buf[7] = pkt->nranswers & 0xFF;
	buf[8] = (pkt->rr_auth >> 8) & 0xFF;
	buf[9] = pkt->rr_auth & 0xFF;
	buf[10] = (pkt->rr_add >> 8) & 0xFF;
	buf[11] = pkt->rr_add & 0xFF;

	*len = 12;

	if(pkt->qr == 0) {
		for(x=0;x<pkt->nrqueries;x++) {
			// write_name(pkt, &buf, len, pkt->queries[x]->query.name);
			write_name(pkt, &buf, len, pkt->queries[x]->name);

			if((buf = REALLOC(buf, (*len)+4)) == NULL) {
				OUT_OF_MEMORY
			}
			// buf[(*len)++] = (pkt->queries[x]->query.type >> 8) & 0xFF;
			// buf[(*len)++] = pkt->queries[x]->query.type & 0xFF;

			buf[(*len)++] = (pkt->queries[x]->type >> 8) & 0xFF;
			buf[(*len)++] = pkt->queries[x]->type & 0xFF;

			// switch(pkt->queries[x]->query.type) {
			switch(pkt->queries[x]->type) {
				case 1:
				case 5:
				case 12:
				case 16:
				case 17:
				case 28:
				case 33:
				case 47: {
					// buf[(*len)] = (pkt->queries[x]->query.qu & 0x1) << 7;
					// buf[(*len)++] |= (pkt->queries[x]->query.class >> 8) & 0x7F;
					// buf[(*len)++] = pkt->queries[x]->query.class & 0xFF;
					buf[(*len)] = (pkt->queries[x]->qu & 0x1) << 7;
					buf[(*len)++] |= (pkt->queries[x]->class >> 8) & 0x7F;
					buf[(*len)++] = pkt->queries[x]->class & 0xFF;
				} break;
			}
		}
	}/* else {
		encode_payload(pkt, 0, &buf, len);
	}*/

	encode_payload(pkt, 1, &buf, len);
	encode_payload(pkt, 2, &buf, len);

	return buf;
}

void mdns_free(struct mdns_packet_t *pkt) {
	int x = 0;

	for(x=0;x<pkt->nrcache;x++) {
		if(pkt->cache[x] != NULL) {
			if(pkt->cache[x]->suffix != NULL) {
				FREE(pkt->cache[x]->suffix);
			}
			FREE(pkt->cache[x]);
		}
	}
	if(pkt->nrcache > 0) {
		FREE(pkt->cache);
	}

	for(x=0;x<pkt->nrqueries;x++) {
		if(pkt->queries[x] != NULL) {
			if(pkt->qr == 0) {
				/*if(pkt->queries[x]->query.name != NULL) {
					FREE(pkt->queries[x]->query.name);
				}*/
				if(pkt->queries[x]->name != NULL) {
					FREE(pkt->queries[x]->name);
				}
			}/* else {
				if(pkt->queries[x]->payload.name != NULL) {
					FREE(pkt->queries[x]->payload.name);
				}

				switch(pkt->queries[x]->payload.type) {
					case 1: {
					} break;
					case 5:
					case 12:
					case 33: {
						if(pkt->queries[x]->payload.data.domain != NULL) {
							FREE(pkt->queries[x]->payload.data.domain);
						}
					} break;
					case 16: {
						int i = 0;
						for(i=0;i<pkt->queries[x]->payload.data.text.nrvalues;i++) {
							FREE(pkt->queries[x]->payload.data.text.values[i]);
						}
						if(pkt->queries[x]->payload.data.text.nrvalues > 0) {
							FREE(pkt->queries[x]->payload.data.text.values);
						}
					} break;
					case 28: {
					} break;
				}
			}*/
		}
		FREE(pkt->queries[x]);
	}
	if(pkt->nrqueries > 0) {
		FREE(pkt->queries);
	}

	for(x=0;x<pkt->nranswers;x++) {
		if(pkt->answers[x] != NULL) {
			if(pkt->answers[x]->name != NULL) {
				FREE(pkt->answers[x]->name);
			}

			switch(pkt->answers[x]->type) {
				case 1: {
				} break;
				case 5:
				case 12:
				case 33: {
					if(pkt->answers[x]->data.domain != NULL) {
						FREE(pkt->answers[x]->data.domain);
					}
				} break;
				case 16: {
					int i = 0;
					for(i=0;i<pkt->answers[x]->data.text.nrvalues;i++) {
						FREE(pkt->answers[x]->data.text.values[i]);
					}
					if(pkt->answers[x]->data.text.nrvalues > 0) {
						FREE(pkt->answers[x]->data.text.values);
					}
				} break;
				case 28: {
				} break;
			}
			FREE(pkt->answers[x]);
		}
	}
	if(pkt->nranswers > 0) {
		FREE(pkt->answers);
	}

	for(x=0;x<pkt->rr_add;x++) {
		if(pkt->records[x] != NULL) {
			if(pkt->records[x]->name != NULL) {
				FREE(pkt->records[x]->name);
			}
			switch(pkt->records[x]->type) {
				case 1: {
				} break;
				case 5:
				case 12:
				case 33: {
					if(pkt->records[x]->data.domain != NULL) {
						FREE(pkt->records[x]->data.domain);
					}
				} break;
				case 16: {
					int i = 0;
					for(i=0;i<pkt->records[x]->data.text.nrvalues;i++) {
						FREE(pkt->records[x]->data.text.values[i]);
					}
					if(pkt->records[x]->data.text.nrvalues > 0) {
						FREE(pkt->records[x]->data.text.values);
					}
				} break;
				case 28: {
				} break;
			}
			FREE(pkt->records[x]);
		}
	}
	if(pkt->rr_add > 0) {
		FREE(pkt->records);
	}
}

void mdns_dump(struct mdns_packet_t *pkt) {
	int x = 0;
	printf("msgid: %04x\n", pkt->id);
	printf("qr: %d\n", pkt->qr);
	printf("opcode: %d\n", pkt->opcode);
	printf("aa: %d\n", pkt->aa);
	printf("tc: %d\n", pkt->tc);
	printf("rd: %d\n", pkt->rd);
	printf("ra: %d\n", pkt->ra);
	printf("z: %d\n", pkt->z);
	printf("ad: %d\n", pkt->ad);
	printf("cd: %d\n", pkt->cd);
	printf("rcode: %d\n", pkt->rcode);
	printf("nrqueries: %d\n", pkt->nrqueries);
	printf("nranswers: %d\n", pkt->nranswers);
	printf("rr_auth: %d\n", pkt->rr_auth);
	printf("rr_add: %d\n", pkt->rr_add);

	for(x=0;x<pkt->nrqueries;x++) {
		if(pkt->qr == 0) {
			/*printf("query[%d].name: %s\n", x, pkt->queries[x]->query.name);
			printf("query[%d].qu: %d\n", x, pkt->queries[x]->query.qu);
			printf("query[%d].class: %d\n", x, pkt->queries[x]->query.class);
			printf("query[%d].type: %d\n", x, pkt->queries[x]->query.type);*/
			printf("query[%d].name: %s\n", x, pkt->queries[x]->name);
			printf("query[%d].qu: %d\n", x, pkt->queries[x]->qu);
			printf("query[%d].class: %d\n", x, pkt->queries[x]->class);
			printf("query[%d].type: %d\n", x, pkt->queries[x]->type);
		}/*else {
			printf("query[%d].name: %s\n", x, pkt->queries[x]->payload.name);
			printf("query[%d].type: %d\n", x, pkt->queries[x]->payload.type);
			printf("query[%d].flush: %d\n", x, pkt->queries[x]->payload.flush);
			printf("query[%d].class: %d\n", x, pkt->queries[x]->payload.class);
			printf("query[%d].ttl: %d\n", x, pkt->queries[x]->payload.ttl);
			printf("query[%d].length: %d\n", x, pkt->queries[x]->payload.length);

			switch(pkt->queries[x]->payload.type) {
				case 1: {
					printf("query[%d].address: %d.%d.%d.%d\n", x,
						pkt->queries[x]->payload.data.ip4[0], pkt->queries[x]->payload.data.ip4[1],
						pkt->queries[x]->payload.data.ip4[2], pkt->queries[x]->payload.data.ip4[3]
					);
				} break;
				case 33: {
					printf("query[%d].priority: %d\n", x, pkt->queries[x]->payload.priority);
					printf("query[%d].weight: %d\n", x, pkt->queries[x]->payload.weight);
					printf("query[%d].port: %d\n", x, pkt->queries[x]->payload.port);
					printf("query[%d].domain: %s\n", x, pkt->queries[x]->payload.data.domain);
				} break;
				case 5:
				case 12: {
					printf("query[%d].domain: %s\n", x, pkt->queries[x]->payload.data.domain);
				} break;
				case 16: {
					int i = 0;
					for(i=0;i<pkt->queries[x]->payload.data.text.nrvalues;i++) {
						printf("query[%d].text[%d].length: %lu\n", x, i, strlen(pkt->queries[x]->payload.data.text.values[i]));
						printf("query[%d].text[%d].value: %s\n", x, i, pkt->queries[x]->payload.data.text.values[i]);
					}
				} break;
				case 28: {
						printf("query[%d].address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", x,
							pkt->queries[x]->payload.data.ip6[0], pkt->queries[x]->payload.data.ip6[1], pkt->queries[x]->payload.data.ip6[2], pkt->queries[x]->payload.data.ip6[3],
							pkt->queries[x]->payload.data.ip6[4], pkt->queries[x]->payload.data.ip6[5], pkt->queries[x]->payload.data.ip6[6], pkt->queries[x]->payload.data.ip6[7],
							pkt->queries[x]->payload.data.ip6[8], pkt->queries[x]->payload.data.ip6[9], pkt->queries[x]->payload.data.ip6[10], pkt->queries[x]->payload.data.ip6[11],
							pkt->queries[x]->payload.data.ip6[12], pkt->queries[x]->payload.data.ip6[13], pkt->queries[x]->payload.data.ip6[14], pkt->queries[x]->payload.data.ip6[15]);
				} break;
			}
		}*/
	}

	for(x=0;x<pkt->nranswers;x++) {
		printf("answer[%d].name: %s\n", x, pkt->answers[x]->name);
		printf("answer[%d].type: %d\n", x, pkt->answers[x]->type);
		printf("answer[%d].flush: %d\n", x, pkt->answers[x]->flush);
		printf("answer[%d].class: %d\n", x, pkt->answers[x]->class);
		printf("answer[%d].ttl: %d\n", x, pkt->answers[x]->ttl);
		printf("answer[%d].length: %d\n", x, pkt->answers[x]->length);

		switch(pkt->answers[x]->type) {
			case 1: {
				printf("answer[%d].address: %d.%d.%d.%d\n", x,
					pkt->answers[x]->data.ip4[0], pkt->answers[x]->data.ip4[1],
					pkt->answers[x]->data.ip4[2], pkt->answers[x]->data.ip4[3]
				);
			} break;
			case 33: {
				printf("answer[%d].priority: %d\n", x, pkt->answers[x]->priority);
				printf("answer[%d].weight: %d\n", x, pkt->answers[x]->weight);
				printf("answer[%d].port: %d\n", x, pkt->answers[x]->port);
				printf("answer[%d].domain: %s\n", x, pkt->answers[x]->data.domain);
			} break;
			case 5:
			case 12: {
				printf("answer[%d].domain: %s\n", x, pkt->answers[x]->data.domain);
			} break;
			case 16: {
				int i = 0;
				for(i=0;i<pkt->answers[x]->data.text.nrvalues;i++) {
					printf("answer[%d].text[%d].length: %u\n", x, i, strlen(pkt->answers[x]->data.text.values[i]));
					printf("answer[%d].text[%d].value: %s\n", x, i, pkt->answers[x]->data.text.values[i]);
				}
			} break;
			case 28: {
					printf("answer[%d].address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", x,
						pkt->answers[x]->data.ip6[0], pkt->answers[x]->data.ip6[1], pkt->answers[x]->data.ip6[2], pkt->answers[x]->data.ip6[3],
						pkt->answers[x]->data.ip6[4], pkt->answers[x]->data.ip6[5], pkt->answers[x]->data.ip6[6], pkt->answers[x]->data.ip6[7],
						pkt->answers[x]->data.ip6[8], pkt->answers[x]->data.ip6[9], pkt->answers[x]->data.ip6[10], pkt->answers[x]->data.ip6[11],
						pkt->answers[x]->data.ip6[12], pkt->answers[x]->data.ip6[13], pkt->answers[x]->data.ip6[14], pkt->answers[x]->data.ip6[15]);
			} break;
		}
	}

	for(x=0;x<pkt->rr_add;x++) {
		printf("record[%d].name: %s\n", x, pkt->records[x]->name);
		printf("record[%d].type: %d\n", x, pkt->records[x]->type);
		printf("record[%d].flush: %d\n", x, pkt->records[x]->flush);
		printf("record[%d].class: %d\n", x, pkt->records[x]->class);
		printf("record[%d].ttl: %d\n", x, pkt->records[x]->ttl);
		printf("record[%d].length: %d\n", x, pkt->records[x]->length);

		switch(pkt->records[x]->type) {
			case 1: {
				printf("record[%d].address: %d.%d.%d.%d\n", x,
					pkt->records[x]->data.ip4[0], pkt->records[x]->data.ip4[1],
					pkt->records[x]->data.ip4[2], pkt->records[x]->data.ip4[3]
				);
			} break;
			case 33: {
				printf("record[%d].priority: %d\n", x, pkt->records[x]->priority);
				printf("record[%d].weight: %d\n", x, pkt->records[x]->weight);
				printf("record[%d].port: %d\n", x, pkt->records[x]->port);
				printf("record[%d].domain: %s\n", x, pkt->records[x]->data.domain);
			} break;
			case 5:
			case 12: {
				printf("record[%d].domain: %s\n", x, pkt->records[x]->data.domain);
			} break;
			case 16: {
				int i = 0;
				for(i=0;i<pkt->records[x]->data.text.nrvalues;i++) {
					printf("record[%d].text[%d].length: %u\n", x, i, strlen(pkt->records[x]->data.text.values[i]));
					printf("record[%d].text[%d].value: %s\n", x, i, pkt->records[x]->data.text.values[i]);
				}
			} break;
			case 28: {
					printf("record[%d].address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", x,
						pkt->records[x]->data.ip6[0], pkt->records[x]->data.ip6[1], pkt->records[x]->data.ip6[2], pkt->records[x]->data.ip6[3],
						pkt->records[x]->data.ip6[4], pkt->records[x]->data.ip6[5], pkt->records[x]->data.ip6[6], pkt->records[x]->data.ip6[7],
						pkt->records[x]->data.ip6[8], pkt->records[x]->data.ip6[9], pkt->records[x]->data.ip6[10], pkt->records[x]->data.ip6[11],
						pkt->records[x]->data.ip6[12], pkt->records[x]->data.ip6[13], pkt->records[x]->data.ip6[14], pkt->records[x]->data.ip6[15]);
			} break;
		}
	}
}

static int read_name(char **name, unsigned char *buf, unsigned int *pos, unsigned int len) {
	int next = -1;
	int first = (*pos);
	int a = 0, b = 255;
	while(1) {
		int foo = buf[(*pos)];

		(*pos)++;
		if(*pos > len) {
			return -1;
		}
		if(foo == 0) {
			break;
		}
		int t = foo & 0xC0;
		if(t == 0x00) {
			int y = (*pos);
			if(((*pos) + foo) >= len) {
				return -2;
			}
			for(y=(*pos);y<(*pos)+foo;y++) {
				if(a > b) {
					a += 255;
					if((*name = REALLOC(*name, a)) == NULL) {
						OUT_OF_MEMORY
					}
					memset(&(*name)[a-255], 0, 255);
				}
				(*name)[a++] = buf[y];
			}
			(*name)[a++] = '.';
			(*pos) += foo;
		} else if(t == 0xC0) {
			if(next < 0) {
				next = (*pos) + 1;
			}
			(*pos) = ((foo & 0x3F) << 8 | buf[(*pos)]);
			if((*pos) >= first || (*pos) >= len) {
				return -3; // Bad circular domain name
			}
			first = (*pos);
		} else {
			return -4;
		}
	}
	if(next >= 0) {
		(*pos) = next;
	}
	if(a > 0 && (*name)[a-1] == '.') {
		(*name)[a-1] = '\0';
		if((*name = REALLOC(*name, a)) == NULL) {
			OUT_OF_MEMORY
		}
	}
	return 0;
}

static int parse_options(struct mdns_payload_t *pkt, unsigned char *buf, unsigned int *i, unsigned int len) {
	unsigned int sublen = 0, y = 0, z = 0;

	memset(&pkt->data.text, 0, sizeof(struct mdns_text_t));

	while(z<pkt->length) {
		sublen = buf[(*i)++];
		if((pkt->data.text.values = REALLOC(pkt->data.text.values, sizeof(char *)*(pkt->data.text.nrvalues+1))) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt->data.text.values[pkt->data.text.nrvalues] = MALLOC(sublen+1)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->data.text.values[pkt->data.text.nrvalues], 0, sublen+1);

		if((*i) > len) {
			return -5;
		}
		z++;
		for(y=(*i);y<(*i)+sublen;y++, z++) {
			pkt->data.text.values[pkt->data.text.nrvalues][y-(*i)] = buf[y];
			if(y > len) {
				return -6;
			}
		}
		pkt->data.text.nrvalues++;
		(*i) = y;
	}
	return 0;
}

static int parse_query(struct mdns_packet_t *pkt, unsigned char *buf, unsigned int len, unsigned int *i) {
	int x = 0, ret = 0;
	for(x=0;x<pkt->nrqueries;x++) {
		if((pkt->queries[x] = MALLOC(sizeof(struct mdns_query_t))) == NULL) {
			OUT_OF_MEMORY
		}
		// if((pkt->queries[x]->query.name = MALLOC(255)) == NULL) {
		if((pkt->queries[x]->name = MALLOC(255)) == NULL) {
			OUT_OF_MEMORY
		}
		// memset(pkt->queries[x]->query.name, 0, 255);
		memset(pkt->queries[x]->name, 0, 255);
		// if((ret = read_name(&pkt->queries[x]->query.name, buf, i, len)) < 0) {
		if((ret = read_name(&pkt->queries[x]->name, buf, i, len)) < 0) {
			return ret;
		}

		/*
		 * We need at least 4 bytes for additonal info
		 * including the current byte.
		 */
		if((*i)+3 > len) {
			return -7;
		}

		// pkt->queries[x]->query.type = (buf[(*i)] << 8) | buf[(*i)+1];
		pkt->queries[x]->type = (buf[(*i)] << 8) | buf[(*i)+1];
		(*i) += 2;
		// switch(pkt->queries[x]->query.type) {
		switch(pkt->queries[x]->type) {
			case 1:
			case 5:
			case 12:
			case 16:
			case 17:
			case 28:
			case 33:
			case 47: {
				// pkt->queries[x]->query.qu = (buf[*i] >> 7);
				// pkt->queries[x]->query.class = ((buf[(*i)] << 8) & 0x7F) | buf[(*i)+1];
				pkt->queries[x]->qu = (buf[*i] >> 7);
				pkt->queries[x]->class = ((buf[(*i)] << 8) & 0x7F) | buf[(*i)+1];
				(*i) += 2;
			} break;
		}
	}
	return 0;
}

static int parse_payload(struct mdns_packet_t *pkt, int type, unsigned char *buf, unsigned int len, unsigned int *i) {
	struct mdns_payload_t *payload = NULL;
	int x = 0, ret = 0, nr = 0;
	switch(type) {
		/*case 0: {
			nr = pkt->nrqueries;
		} break;*/
		case 1: {
			nr = pkt->nranswers;
		} break;
		case 2: {
			nr = pkt->rr_add;
		} break;
	}

	for(x=0;x<nr;x++) {
		switch(type) {
			/*case 0: {
				if((pkt->queries[x] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
					OUT_OF_MEMORY
				}
				payload = &pkt->queries[x]->payload;
			} break;*/
			case 1: {
				if((pkt->answers[x] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
					OUT_OF_MEMORY
				}
				payload = pkt->answers[x];
			} break;
			case 2: {
				if((pkt->records[x] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
					OUT_OF_MEMORY
				}
				payload = pkt->records[x];
			} break;
			default:
				return -15;
		}
		memset(payload, 0, sizeof(struct mdns_payload_t));

		if((payload->name = MALLOC(255)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(payload->name, 0, 255);
		if((ret = read_name(&payload->name, buf, i, len)) < 0) {
			return ret;
		}

		/*
		 * We need at least two bytes to determine the type
		 * including the current byte.
		 */
		if((*i)+1 >= len) {
			return -9;
		}
		payload->type = (buf[(*i)] << 8) | buf[(*i)+1];
		(*i)+=2;
		switch(payload->type) {
			/*
			 * We need at least twelve bytes to determine the data,
			 * and four digits ip address, including the current byte.
			 */
			case 1: {
				if((*i)+11 >= len) {
					return -10;
				}
			} break;
			case 28: {
				/*
				 * We need at least twelve bytes to determine the data,
				 * and sixteen digits ip6 address, including the current byte.
				 */
				if((*i)+11 >= len) {
					return -11;
				}
			} break;
			case 33: {
				/*
				 * We need at least twelve bytes to determine the data
				 * including the current byte.
				 */
				if((*i)+11 >= len) {
					return -12;
				}
			} break;
			case 5:
			case 12:
			case 16:
			case 17:
			case 47: {
				/*
				 * We need at least eight bytes to determine the data
				 * including the current byte.
				 */
				if((*i)+7 >= len) {
					return -13;
				}
			} break;
		}
		switch(payload->type) {
			case 1:
			case 5:
			case 12:
			case 16:
			case 17:
			case 28:
			case 33:
			case 47: {
				payload->flush = (buf[*i] >> 7);
				payload->class = ((buf[(*i)] << 8) & 0x7F) | buf[(*i)+1]; (*i) += 2;
				payload->ttl = buf[(*i)] << 24; (*i)++;
				payload->ttl |= buf[(*i)] << 16; (*i)++;
				payload->ttl |= buf[(*i)] << 8; (*i)++;
				payload->ttl |= buf[(*i)]; (*i)++;
				payload->length = (buf[(*i)] << 8) | buf[(*i)+1]; (*i) += 2;
			} break;
		}
		switch(payload->type) {
			case 33: { // SRV
				payload->priority = (buf[(*i)] << 8) | buf[(*i)+1]; (*i) += 2;
				payload->weight = (buf[(*i)] << 8) | buf[(*i)+1]; (*i) += 2;
				payload->port = (buf[(*i)] << 8) | buf[(*i)+1]; (*i) += 2;
			} break;
		}

		/*
		 * Options length overflows message length
		 * except when it's a cached name
		 */
		if(payload->type != 33 && ((*i)+payload->length > len && (*i)+2 != len)) {
			return -14;
		}

		switch(payload->type) {
			case 1: { // A, string 4
				memset(&payload->data.ip4, 0, 4);
				payload->data.ip4[0] = buf[(*i)]; (*i)++;
				payload->data.ip4[1] = buf[(*i)]; (*i)++;
				payload->data.ip4[2] = buf[(*i)]; (*i)++;
				payload->data.ip4[3] = buf[(*i)]; (*i)++;
			} break;
			case 5: // CNAME
			case 12: // PTR
			case 33: { // SRV
				if((payload->data.domain = MALLOC(255)) == NULL) {
					OUT_OF_MEMORY
				}
				memset(payload->data.domain , 0, 255);
				if((ret = read_name(&payload->data.domain, buf, i, len)) < 0) {
					return ret;
				}
			} break;
			case 16: { // TXT
				if((ret = parse_options(payload, buf, i, len)) < 0) {
					return ret;
				}
			} break;
			case 28: { // AAA, string 16
				memset(&payload->data.ip6, 0, 16);
				int a = 0;
				for(a=0;a<16;a++) {
					payload->data.ip6[a] = buf[(*i)];
					(*i)++;
				}
			} break;
			case 41:
			case 47: { // NSEC
				// Unsupported
				(*i)+=payload->length;
			} break;
			(*i)++;
		}
	}
	return 0;
}

int mdns_decode(struct mdns_packet_t *pkt, unsigned char *buf, unsigned int len) {
	if(len < 12) {
		// MDNS package size should be at least 12 bytes
		return -16;
	}

	pkt->id = buf[0] << 8 | buf[1];
	pkt->qr = (buf[2] >> 7);
	pkt->opcode = (buf[2] >> 3) & 0x0F;
	pkt->aa = (buf[2] >> 2) & 0x01;
	pkt->tc = (buf[2] >> 1) & 0x01;
	pkt->rd = buf[2] & 0x01;
	pkt->ra = (buf[3] >> 7);
	pkt->z = (buf[3] >> 6) & 0x01;
	pkt->ad = (buf[3] >> 5) & 0x01;
	pkt->cd = (buf[3] >> 4) & 0x01;
	pkt->rcode = buf[3] & 0x0F;
	pkt->nrqueries = buf[4] << 8 | buf[5];
	pkt->nranswers = buf[6] << 8 | buf[7];
	pkt->rr_auth = buf[8] << 8 | buf[9];
	pkt->rr_add = buf[10] << 8 | buf[11];

	unsigned int i = 12;

	int ret = 0;
	if(pkt->qr == 0) {
		if(pkt->nrqueries > 0) {
			if((pkt->queries = MALLOC(sizeof(struct mdns_query_t *)*pkt->nrqueries)) == NULL) {
				OUT_OF_MEMORY
			}
			memset(pkt->queries, 0, sizeof(struct mdns_query_t *)*pkt->nrqueries);
			if((ret = parse_query(pkt, buf, len, &i)) < 0) {
				mdns_free(pkt);
				return ret;
			}
		}
	} else {
			/*
			 * 0 = query
			 */
		/*if(pkt->nrqueries > 0) {
			if((pkt->queries = MALLOC(sizeof(struct mdns_query_t *)*pkt->nrqueries)) == NULL) {
				OUT_OF_MEMORY
			}
			memset(pkt->queries, 0, sizeof(struct mdns_query_t *)*pkt->nrqueries);
			if((ret = parse_payload(pkt, 0, buf, len, &i)) < 0) {
				mdns_free(pkt);
				return ret;
			}
		}*/
	}
	if(pkt->nranswers > 0) {
		/*
		 * 1 = answer
		 */
		if((pkt->answers = MALLOC(sizeof(struct mdns_payload_t *)*pkt->nranswers)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->answers, 0, sizeof(struct mdns_payload_t *)*pkt->nranswers);

		if((ret = parse_payload(pkt, 1, buf, len, &i)) < 0) {
			mdns_free(pkt);
			return ret;
		}
	}
	if(pkt->rr_add > 0) {
		/*
		 * 2 = additional records
		 */
		if((pkt->records = MALLOC(sizeof(struct mdns_payload_t *)*pkt->rr_add)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt->records, 0, sizeof(struct mdns_payload_t *)*pkt->rr_add);
		if((ret = parse_payload(pkt, 2, buf, len, &i)) < 0) {
			mdns_free(pkt);
			return ret;
		}
	}
	return 0;
}

static void on_send(uv_udp_send_t *req, int status) {
	if(req->data != NULL) {
		FREE(req->data);
	}
	FREE(req);
}

static void read_cb(uv_udp_t *stream, ssize_t len, const uv_buf_t *buf, const struct sockaddr *addr, unsigned int f) {
	struct mdns_data_t *data = stream->data;

	void (*func)(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata) = data->callback;
	struct mdns_packet_t pkt;
	memset(&pkt, 0, sizeof(struct mdns_packet_t));

	if(len > 0) {
		if(func != NULL && len > 0) {
			if(mdns_decode(&pkt, (unsigned char *)buf->base, len) == 0) {
				func(addr, &pkt, data->userdata);
				mdns_free(&pkt);
			}
		}
	}
	free(buf->base);
}

int mdns_listen(void (*func)(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata), void *userdata) {
	if(lock_init == 0) {
		lock_init = 1;
		uv_mutex_init(&lock);
	}

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
#endif

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	uv_udp_t *coap_req = MALLOC(sizeof(uv_udp_t));
	memset(coap_req, 0, sizeof(uv_udp_t));

	int r = uv_udp_init(uv_default_loop(), coap_req);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

#ifdef PILIGHT_UNITTEST
	r = uv_ip4_addr("224.0.0.251", 15353, &addr);
#else
	r = uv_ip4_addr("224.0.0.251", 5353, &addr);
#endif
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_ip4_addr: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	r = uv_udp_bind(coap_req, (const struct sockaddr *)&addr, UV_UDP_REUSEADDR);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_bind: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	r = uv_udp_set_membership(coap_req, "224.0.0.251", NULL, UV_JOIN_GROUP);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_set_membership: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	uv_mutex_lock(&lock);
	if((data = REALLOC(data, sizeof(struct mdns_packet_t *)*(nrdata+1))) == NULL) {
		OUT_OF_MEMORY
	}
	if((data[nrdata] = MALLOC(sizeof(struct mdns_packet_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(data[nrdata], 0, sizeof(struct mdns_packet_t));

	data[nrdata]->callback = func;
	data[nrdata]->userdata = userdata;
	data[nrdata]->type = LISTEN;

	coap_req->data = data[nrdata];
	nrdata++;
	uv_mutex_unlock(&lock);

	r = uv_udp_recv_start(coap_req, alloc, read_cb);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_recv_start: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	return 0;
close:
	if(data != NULL) {
		FREE(data);
	}
	FREE(coap_req);
	return -1;
}

int mdns_send(struct mdns_packet_t *pkt, void (*func)(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata), void *userdata) {
	if(lock_init == 0) {
		lock_init = 1;
		uv_mutex_init(&lock);
	}

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
#endif

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	uv_udp_t *coap_req = MALLOC(sizeof(uv_udp_t));
	if(coap_req == NULL) {
		OUT_OF_MEMORY
	}
	memset(coap_req, 0, sizeof(uv_udp_t));

	uv_udp_send_t *send_req = MALLOC(sizeof(uv_udp_send_t));
	if(send_req == NULL) {
		OUT_OF_MEMORY;
	}
	memset(send_req, 0, sizeof(uv_udp_send_t));

#ifdef PILIGHT_UNITTEST
	int r = uv_ip4_addr("224.0.0.251", 15353, &addr);
#else
	int r = uv_ip4_addr("224.0.0.251", 5353, &addr);
#endif
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	r = uv_udp_init(uv_default_loop(), coap_req);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	unsigned int len = 0;
	unsigned char *msg = mdns_encode(pkt, &len);
	uv_buf_t buf = uv_buf_init((char *)msg, len);

	r = uv_udp_send(send_req, coap_req, &buf, 1, (const struct sockaddr *)&addr, on_send);
	if(r != 0) {
		FREE(msg);
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	uv_mutex_lock(&lock);
	if((data = REALLOC(data, sizeof(struct mdns_packet_t *)*(nrdata+1))) == NULL) {
		OUT_OF_MEMORY
	}
	if((data[nrdata] = MALLOC(sizeof(struct mdns_packet_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(data[nrdata], 0, sizeof(struct mdns_packet_t));

	data[nrdata]->callback = func;
	data[nrdata]->userdata = userdata;
	data[nrdata]->type = SEND;

	coap_req->data = data[nrdata];
	nrdata++;
	uv_mutex_unlock(&lock);

	r = uv_udp_recv_start(coap_req, alloc, read_cb);
	if(r != 0) {
		FREE(msg);
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_init: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	FREE(msg);
	return 0;

close:
	if(data != NULL) {
		FREE(data);
	}
	FREE(send_req);
	FREE(coap_req);
	return -1;
}
