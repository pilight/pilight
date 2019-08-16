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
#include "../libs/pilight/core/mqtt.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"

static CuTest *gtc = NULL;

void test_mqtt_encode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	// CONNECT
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x10, 0x36, 0x00, 0x04, 0x6d, 0x71, 0x74, 0x74, 0x04, 0xc0, 0x00, 0x3c, 0x00, 0x07, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x00, 0x13, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x2f, 0x6c, 0x61, 0x6d, 0x70, 0x2f, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x00, 0x02, 0x6f, 0x6e, 0x00, 0x03, 0x66, 0x6f, 0x6f, 0x00, 0x03, 0x62, 0x61, 0x72};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_connect(&pkt, 0, 0, 0, "pilight", "mqtt", 4, "foo", "bar", 0, 0, 0, 60, "pilight/lamp/status", "on");

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// CONNACK
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x20, 0x02, 0x00, 0x00};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_connack(&pkt, MQTT_CONNECTION_ACCEPTED);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBLISH QoS 0
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x30, 0x17, 0x00, 0x13, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x2f, 0x6c, 0x61, 0x6d, 0x70, 0x2f, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x6f, 0x6e};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_publish(&pkt, 0, 0, 0, "pilight/lamp/status", 1, "on");

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBLISH QoS 0
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x30, 0x89, 0x02, 0x00, 0x13, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x2f, 0x6c, 0x61, 0x6d, 0x70, 0x2f, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x7b, 0x22, 0x54, 0x69, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x31, 0x39, 0x37, 0x30, 0x2d, 0x30, 0x31, 0x2d, 0x30, 0x31, 0x54, 0x30, 0x32, 0x3a, 0x33, 0x32, 0x3a, 0x32, 0x37, 0x22, 0x2c, 0x22, 0x55, 0x70, 0x74, 0x69, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x30, 0x54, 0x30, 0x32, 0x3a, 0x32, 0x35, 0x3a, 0x31, 0x36, 0x22, 0x2c, 0x22, 0x56, 0x63, 0x63, 0x22, 0x3a, 0x33, 0x2e, 0x32, 0x33, 0x38, 0x2c, 0x22, 0x53, 0x6c, 0x65, 0x65, 0x70, 0x4d, 0x6f, 0x64, 0x65, 0x22, 0x3a, 0x22, 0x44, 0x79, 0x6e, 0x61, 0x6d, 0x69, 0x63, 0x22, 0x2c, 0x22, 0x53, 0x6c, 0x65, 0x65, 0x70, 0x22, 0x3a, 0x35, 0x30, 0x2c, 0x22, 0x4c, 0x6f, 0x61, 0x64, 0x41, 0x76, 0x67, 0x22, 0x3a, 0x31, 0x39, 0x2c, 0x22, 0x50, 0x4f, 0x57, 0x45, 0x52, 0x22, 0x3a, 0x22, 0x4f, 0x4e, 0x22, 0x2c, 0x22, 0x57, 0x69, 0x66, 0x69, 0x22, 0x3a, 0x7b, 0x22, 0x41, 0x50, 0x22, 0x3a, 0x31, 0x2c, 0x22, 0x53, 0x53, 0x49, 0x64, 0x22, 0x3a, 0x22, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x22, 0x2c, 0x22, 0x42, 0x53, 0x53, 0x49, 0x64, 0x22, 0x3a, 0x22, 0x41, 0x41, 0x3a, 0x42, 0x42, 0x3a, 0x43, 0x43, 0x3a, 0x44, 0x44, 0x3a, 0x45, 0x45, 0x3a, 0x46, 0x46, 0x22, 0x2c, 0x22, 0x43, 0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x22, 0x3a, 0x32, 0x2c, 0x22, 0x52, 0x53, 0x53, 0x49, 0x22, 0x3a, 0x38, 0x32, 0x2c, 0x22, 0x4c, 0x69, 0x6e, 0x6b, 0x43, 0x6f, 0x75, 0x6e, 0x74, 0x22, 0x3a, 0x31, 0x2c, 0x22, 0x44, 0x6f, 0x77, 0x6e, 0x74, 0x69, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x30, 0x54, 0x30, 0x30, 0x3a, 0x30, 0x30, 0x3a, 0x30, 0x36, 0x22, 0x7d, 0x7d};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_publish(&pkt, 0, 0, 0, "pilight/lamp/status", 1, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			// printf("0x%02x, ", buf[i]);
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBLISH QoS 1
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x32, 0x19, 0x00, 0x13, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x2f, 0x6c, 0x61, 0x6d, 0x70, 0x2f, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x00, 0x01, 0x6f, 0x6e};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_publish(&pkt, 0, 1, 0, "pilight/lamp/status", 1, "on");

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBLISH QoS 2
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x34, 0x19, 0x00, 0x13, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x2f, 0x6c, 0x61, 0x6d, 0x70, 0x2f, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x00, 0x01, 0x6f, 0x6e};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_publish(&pkt, 0, 2, 0, "pilight/lamp/status", 1, "on");

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBACK
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x40, 0x02, 0x00, 0x01};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_puback(&pkt, 1);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBREC
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x50, 0x02, 0x00, 0x01};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_pubrec(&pkt, 1);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBREL
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x60, 0x02, 0x00, 0x01};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_pubrel(&pkt, 1);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PUBCOMP
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x70, 0x02, 0x00, 0x01};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_pubcomp(&pkt, 1);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// SUBSCRIBE
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x80, 0x18, 0x00, 0x01, 0x00, 0x13, 0x70, 0x69, 0x6c, 0x69, 0x67, 0x68, 0x74, 0x2f, 0x6c, 0x61, 0x6d, 0x70, 0x2f, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x01};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_subscribe(&pkt, "pilight/lamp/status", 1, 1);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// SUBACK
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0x90, 0x03, 0x00, 0x01, 0x01};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_suback(&pkt, 1, 1);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PINGREQ
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0xc0, 0x00};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_pingreq(&pkt);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// PINGRESP
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0xd0, 0x00};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_pingresp(&pkt);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	// DISCONNECT
	{
		struct mqtt_pkt_t pkt;
		unsigned char output[] = {0xe0, 0x00};
		unsigned char *buf = NULL;
		unsigned int len = 0, i = 0;

		memset(&pkt, 0, sizeof(struct mqtt_pkt_t));

		mqtt_pkt_disconnect(&pkt);

		CuAssertIntEquals(tc, 0, mqtt_encode(&pkt, &buf, &len));
		CuAssertIntEquals(tc, sizeof(output), len);
		for(i=0;i<len;i++) {
			CuAssertIntEquals(tc, output[i], buf[i]);
		}
		mqtt_free(&pkt);
		FREE(buf);
	}

	CuAssertIntEquals(tc, 0, xfree());
}
