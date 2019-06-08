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

#include "coap.h"

#include "../../libuv/uv.h"

#define LISTEN	0
#define SEND		1

typedef struct coap_data_t {
	void (*callback)(const struct sockaddr *addr, struct coap_packet_t *pkt, void *userdata);
	void *userdata;
	int type;
} coap_data_t;

static uv_mutex_t lock;
static int lock_init = 0;
static struct coap_data_t **data = NULL;
static int nrdata = 0;

void coap_gc(void) {
	int i = 0;
	uv_mutex_lock(&lock);
	for(i=0;i<nrdata;i++) {
		FREE(data[i]);
	}
	nrdata--;
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

unsigned char *coap_encode(struct coap_packet_t *coap, unsigned int *len) {
	unsigned char *buf = MALLOC(4);
	int i = 0;
	int x = 0;

	if(buf == NULL) {
		OUT_OF_MEMORY
	}

	buf[0] = coap->ver << 6;
	buf[0] |= coap->t << 4;
	buf[0] |= coap->tkl;

	buf[1] = coap->code[0] << 5;
	buf[1] |= coap->code[1];

	buf[2] = coap->msgid[0];
	buf[3] = coap->msgid[1];

	*len = 4;

	if(coap->tkl > 0) {
		if((buf = REALLOC(buf, (*len)+coap->tkl)) == NULL) {
			OUT_OF_MEMORY
		}
		for(x=0;x<coap->tkl;x++) {
			buf[(*len)++] = coap->token[x];
		}
	}

	for(i=0;i<coap->numopt;i++) {
		if((buf = REALLOC(buf, (*len)+1)) == NULL) {
			OUT_OF_MEMORY
		}

		int tmp = coap->options[i]->num;
		if(i > 0) {
			tmp -= coap->options[i-1]->num;
		}

		if(tmp > 268) {
			buf[(*len)] = 14 << 4;
		} else if(tmp > 12) {
			buf[(*len)] |= 13 << 4;
		} else {
			buf[(*len)] = tmp << 4;
		}

		if(coap->options[i]->len > 268) {
			buf[(*len)] |= 14;
		} else if(coap->options[i]->len > 12) {
			buf[(*len)] |= 13;
		} else {
			buf[(*len)] |= coap->options[i]->len;
		}

		(*len)++;

		if(tmp > 268) {
			if((buf = REALLOC(buf, (*len)+2)) == NULL) {
				OUT_OF_MEMORY
			}
			buf[(*len)++] = ((tmp - 269) & 0xFF00) >> 8;
			buf[(*len)++] = (tmp - 269) & 0x00FF;
		} else if(tmp > 12) {
			if((buf = REALLOC(buf, (*len)+1)) == NULL) {
				OUT_OF_MEMORY
			}
			buf[(*len)++] = (tmp - 13);
		}

		if(coap->options[i]->len > 268) {
			if((buf = REALLOC(buf, (*len)+coap->options[i]->len+2)) == NULL) {
				OUT_OF_MEMORY
			}
			buf[(*len)++] = ((coap->options[i]->len - 269) & 0xFF00) >> 8;
			buf[(*len)++] = (coap->options[i]->len - 269) & 0x00FF;
		} else if(coap->options[i]->len > 12) {
			if((buf = REALLOC(buf, (*len)+coap->options[i]->len+1)) == NULL) {
				OUT_OF_MEMORY
			}
			buf[(*len)++] = (coap->options[i]->len - 13);
		} else {
			if((buf = REALLOC(buf, (*len)+coap->options[i]->len)) == NULL) {
				OUT_OF_MEMORY
			}
		}

		for(x=0;x<coap->options[i]->len;x++) {
			buf[(*len)++] = coap->options[i]->val[x];
		}
	}

	if(coap->payload != NULL) {
		if((buf = REALLOC(buf, (*len)+strlen(coap->payload)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		buf[(*len)++] = 0xFF;
		for(x=0;x<strlen(coap->payload);x++) {
			buf[(*len)++] = coap->payload[x];
		}
	}

	return buf;
}

int coap_decode(struct coap_packet_t *coap, unsigned char *message, unsigned int msglen) {
	/*
	 * Message too short to be coap
	 */
	if(msglen < 4) {
		return -3;
	}

	memset(coap, 0, sizeof(struct coap_packet_t));
	int pos = 0, delta[2] = { 0 }, len = 0;
	coap->ver = (message[0] & 0xC0) >> 6;
	coap->t = (message[0] & 0x30) >> 4;
	coap->tkl = (message[0] & 0x0F);
	coap->code[0] = ((message[1] & 0xE0) >> 5);
	coap->code[1] = (message[1] & 0x1F);
	coap->msgid[0] = message[2];
	coap->msgid[1] = message[3];

	pos = 4;
	coap->token = NULL;
	if(coap->tkl > 0) {
		if((coap->token = MALLOC(coap->tkl+1)) == NULL) {
			OUT_OF_MEMORY
		}
		memcpy(coap->token, &message[pos], coap->tkl);
		pos += (coap->tkl);
	}

	if(message[pos] != 0xFF) {
		while(message[pos] != 0xFF && pos < msglen) {
			delta[0] = ((message[pos] & 0xF0) >> 4);
			len = message[pos] & 0x0F;

			switch(delta[0]) {
				case 13:
					pos += 1;
					delta[0] = (uint8_t)(message[pos] + 13);
				break;
				case 14:
					pos += 1;
					delta[0] = (uint16_t)((((message[pos] << 8) ) | message[pos+1])) + 269;
					pos += 1;
				break;
				case 15:
					return -1;
			}

			delta[0] += delta[1];

			switch(len) {
				case 13:
					pos += 1;
					len = (uint8_t)(message[pos] + 13);
				break;
				case 14:
					pos += 1;
					len = (uint16_t)((((message[pos] << 8) ) | message[pos+1])) + 269;
					pos += 1;
				break;
				case 15:
					return -1;
			}

			pos += 1;

			if((coap->options = REALLOC(coap->options, sizeof(struct coap_options_t *)*(coap->numopt+1))) == NULL) {
				OUT_OF_MEMORY
			}
			if((coap->options[coap->numopt] = MALLOC(sizeof(struct coap_options_t))) == NULL) {
				OUT_OF_MEMORY
			}
			memset(coap->options[coap->numopt], 0, sizeof(struct coap_options_t));
			if((coap->options[coap->numopt]->val = MALLOC(len+1)) == NULL) {
				OUT_OF_MEMORY
			}
			coap->options[coap->numopt]->len = len;
			memset(coap->options[coap->numopt]->val, 0, len+1);
			memcpy(coap->options[coap->numopt]->val, &message[pos], len);
			coap->options[coap->numopt]->num = delta[0];

			delta[1] = delta[0];
			pos += len;
			coap->numopt++;
		}

		pos++;

		if(pos < msglen) {
			int len = strlen((char *)&message[pos]);

			if(len > 0) {
				if((coap->payload = MALLOC(len+1)) == NULL) {
					OUT_OF_MEMORY
				}
				memset(coap->payload, 0, len+1);
				memcpy(coap->payload, &message[pos], len);
			}
		}
		return 0;
	}

	return -1;
}

void coap_free(struct coap_packet_t *coap) {
	int i = 0;
	if(coap->numopt > 0) {
		for(i=0;i<coap->numopt;i++) {
			FREE(coap->options[i]->val);
			FREE(coap->options[i]);
		}
		FREE(coap->options);
	}
	if(coap->token != NULL) {
		FREE(coap->token);
	}
	if(coap->payload != NULL) {
		FREE(coap->payload);
	}
}

void coap_dump(struct coap_packet_t *coap) {
	int i = 0, x = 0;
	printf("ver:\t%d\n", coap->ver);
	printf("t:\t%d\n", coap->t);
	printf("tkl:\t%d\n", coap->tkl);
	printf("code:\t%d.%d\n", coap->code[0], coap->code[1]);
	printf("msg id:\t0x%02X 0x%02X\n", coap->msgid[0], coap->msgid[1]);

	if(coap->token != NULL) {
		printf("token:\t%s\n", coap->token);
	}

	printf("\n");
	printf("options:\n");
	for(i=0;i<coap->numopt;i++) {
		printf(" [%*d] %*d: ", 3, i, 5, coap->options[i]->num);
		for(x=0;x<coap->options[i]->len;x++) {
			printf("%02x ", coap->options[i]->val[x]);
			if((x % 4) == 3) {
				printf("\n              ");
			}
		}
		printf("\n");
	}

	printf("\npayload:\n%s\n\n", coap->payload);
}

static void on_send(uv_udp_send_t *req, int status) {
	if(req->data != NULL) {
		FREE(req->data);
	}
	FREE(req);
}

static void read_cb(uv_udp_t *stream, ssize_t len, const uv_buf_t *buf, const struct sockaddr *addr, unsigned int f) {
	struct coap_data_t *data = stream->data;

	void (*func)(const struct sockaddr *addr, struct coap_packet_t *pkt, void *userdata) = data->callback;
	struct coap_packet_t pkt;
	memset(&pkt, 0, sizeof(struct coap_packet_t));

	if(len > 0) {
		if(func != NULL) {
			if(coap_decode(&pkt, (unsigned char *)buf->base, len) == 0) {
				func(addr, &pkt, data->userdata);
				coap_free(&pkt);
			}
		}
	}
	free(buf->base);
}

int coap_listen(void (*func)(const struct sockaddr *addr, struct coap_packet_t *pkt, void *userdata), void *userdata) {
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

	r = uv_ip4_addr("224.0.1.187", 5683, &addr);
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

	r = uv_udp_set_membership(coap_req, "224.0.1.187", NULL, UV_JOIN_GROUP);
	if(r != 0) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "uv_udp_set_membership: %s (%s #%d)", uv_strerror(r), __FILE__, __LINE__);
		goto close;
		/*LCOV_EXCL_STOP*/
	}

	uv_mutex_lock(&lock);
	if((data = REALLOC(data, sizeof(struct coap_data_t *)*(nrdata+1))) == NULL) {
		OUT_OF_MEMORY
	}
	if((data[nrdata] = MALLOC(sizeof(struct coap_data_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(data[nrdata], 0, sizeof(struct coap_data_t));

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

int coap_send(struct coap_packet_t *pkt, void (*func)(const struct sockaddr *addr, struct coap_packet_t *pkt, void *userdata), void *userdata) {
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

	int r = uv_ip4_addr("224.0.1.187", 5683, &addr);
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
	unsigned char *msg = coap_encode(pkt, &len);
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
	if((data = REALLOC(data, sizeof(struct coap_data_t *)*(nrdata+1))) == NULL) {
		OUT_OF_MEMORY
	}
	if((data[nrdata] = MALLOC(sizeof(struct coap_data_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(data[nrdata], 0, sizeof(struct coap_data_t));

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
