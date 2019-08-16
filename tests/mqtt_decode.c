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

void test_mqtt_decode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	gtc = tc;

	memtrack();

	// CONNECT
	{
		unsigned char buf[] = {0x10, 0x3c, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, 0x04, 0xae, 0x00, 0x0a, 0x00, 0x0b, 0x44, 0x56, 0x45, 0x53, 0x5f, 0x31, 0x43, 0x37, 0x46, 0x38, 0x32, 0x00, 0x0f, 0x74, 0x65, 0x6c, 0x65, 0x2f, 0x73, 0x6f, 0x6e, 0x6f, 0x66, 0x66, 0x2f, 0x4c, 0x57, 0x54, 0x00, 0x07, 0x4f, 0x66, 0x66, 0x6c, 0x69, 0x6e, 0x65, 0x00, 0x09, 0x44, 0x56, 0x45, 0x53, 0x5f, 0x55, 0x53, 0x45, 0x52};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 1);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		CuAssertStrEquals(tc, pkt[i]->header.connect.protocol, "MQTT");
		CuAssertIntEquals(tc, pkt[i]->header.connect.version, 4);
		CuAssertIntEquals(tc, pkt[i]->header.connect.username, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.password, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.willretain, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.qos, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.willflag, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.cleansession, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.reserved, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.connect.keepalive, 10);
		CuAssertStrEquals(tc, pkt[i]->payload.connect.clientid, "DVES_1C7F82");
		CuAssertStrEquals(tc, pkt[i]->payload.connect.willtopic, "tele/sonoff/LWT");
		CuAssertStrEquals(tc, pkt[i]->payload.connect.willmessage, "Offline");
		CuAssertStrEquals(tc, pkt[i]->payload.connect.username, "DVES_USER");
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	{
		unsigned char buf[] = {0x10, 0x45, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, 0x04, 0x06, 0x00, 0x3c, 0x00, 0x10, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x00, 0x20, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x6f, 0x6e, 0x6c, 0x69, 0x6e, 0x65, 0x00, 0x05, 0x66, 0x61, 0x6c, 0x73, 0x65};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 1);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		CuAssertStrEquals(tc, pkt[i]->header.connect.protocol, "MQTT");
		CuAssertIntEquals(tc, pkt[i]->header.connect.version, 4);
		CuAssertIntEquals(tc, pkt[i]->header.connect.username, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.password, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.willretain, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.qos, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.willflag, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.cleansession, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.reserved, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.connect.keepalive, 60);
		CuAssertStrEquals(tc, pkt[i]->payload.connect.clientid, "shelly1pm-ABCDEF");
		CuAssertStrEquals(tc, pkt[i]->payload.connect.willtopic, "shellies/shelly1pm-ABCDEF/online");
		CuAssertStrEquals(tc, pkt[i]->payload.connect.willmessage, "false");
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	{
		unsigned char buf[] = {0x10, 0x25, 0x00, 0x06, 0x4d, 0x51, 0x49, 0x73, 0x64, 0x70, 0x03, 0x02, 0x00, 0x0a, 0x00, 0x17, 0x6d, 0x6f, 0x73, 0x71, 0x2f, 0x48, 0x70, 0x35, 0x3a, 0x45, 0x4d, 0x61, 0x3f, 0x5d, 0x77, 0x41, 0x5f, 0x74, 0x57, 0x6d, 0x45, 0x75, 0x65};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 1);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		CuAssertStrEquals(tc, pkt[i]->header.connect.protocol, "MQIsdp");
		CuAssertIntEquals(tc, pkt[i]->header.connect.version, 3);
		CuAssertIntEquals(tc, pkt[i]->header.connect.username, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.password, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.willretain, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.qos, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.willflag, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connect.cleansession, 1);
		CuAssertIntEquals(tc, pkt[i]->header.connect.reserved, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.connect.keepalive, 10);
		CuAssertStrEquals(tc, pkt[i]->payload.connect.clientid, "mosq/Hp5:EMa?]wA_tWmEue");
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// CONNACK
	{
		unsigned char buf[] = {0x20, 0x02, 0x00, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 2);
		CuAssertIntEquals(tc, pkt[i]->header.connack.reserved, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connack.session, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.connack.code, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// CONNACK
	{
		unsigned char buf[] = {0x20, 0x02, 0x00, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 2);
		CuAssertIntEquals(tc, pkt[i]->header.connack.reserved, 0);
		CuAssertIntEquals(tc, pkt[i]->header.connack.session, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.connack.code, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PUBLISH QoS 0
	{
		unsigned char buf[] = {0x31, 0x17, 0x00, 0x0f, 0x74, 0x65, 0x6c, 0x65, 0x2f, 0x73, 0x6f, 0x6e, 0x6f, 0x66, 0x66, 0x2f, 0x4c, 0x57, 0x54, 0x4f, 0x6e, 0x6c, 0x69, 0x6e, 0x65};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 3);
		CuAssertIntEquals(tc, pkt[i]->dub, 0);
		CuAssertIntEquals(tc, pkt[i]->qos, 0);
		CuAssertIntEquals(tc, pkt[i]->retain, 1);
		CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "tele/sonoff/LWT");
		CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "Online");
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PUBLISH QoS 1
	{
		unsigned char buf[] = {0x32, 0x27, 0x00, 0x21, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x72, 0x65, 0x6c, 0x61, 0x79, 0x2f, 0x30, 0x03, 0x94, 0x6f, 0x6e};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 3);
		CuAssertIntEquals(tc, pkt[i]->dub, 0);
		CuAssertIntEquals(tc, pkt[i]->qos, 1);
		CuAssertIntEquals(tc, pkt[i]->retain, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.publish.msgid, 916);
		CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/relay/0");
		CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "on");
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PUBLISH QoS 2
	{
		unsigned char buf[] = {0x34, 0x28, 0x00, 0x20, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x6f, 0x6e, 0x6c, 0x69, 0x6e, 0x65, 0x03, 0xd6, 0x74, 0x72, 0x75, 0x65};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 3);
		CuAssertIntEquals(tc, pkt[i]->dub, 0);
		CuAssertIntEquals(tc, pkt[i]->qos, 2);
		CuAssertIntEquals(tc, pkt[i]->retain, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.publish.msgid, 982);
		CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/online");
		CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "true");
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PUBACK
	{
		unsigned char buf[] = {0x40, 0x02, 0x03, 0x94};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 4);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.puback.msgid, 916);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PUBREC
	{
		unsigned char buf[] = {0x50, 0x02, 0x03, 0xd6};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 5);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.puback.msgid, 982);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PUBREL
	{
		unsigned char buf[] = {0x62, 0x02, 0x03, 0xd6};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 6);
		CuAssertIntEquals(tc, pkt[i]->reserved, 2);
		CuAssertIntEquals(tc, pkt[i]->payload.puback.msgid, 982);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PUBCOMP
	{
		unsigned char buf[] = {0x70, 0x02, 0x03, 0xd6};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 7);
		CuAssertIntEquals(tc, pkt[i]->dub, 0);
		CuAssertIntEquals(tc, pkt[i]->qos, 0);
		CuAssertIntEquals(tc, pkt[i]->retain, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.puback.msgid, 982);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// SUBSCRIBE
	{
		unsigned char buf[] = {0x82, 0x12, 0x00, 0x02, 0x00, 0x0d, 0x63, 0x6d, 0x6e, 0x64, 0x2f, 0x73, 0x6f, 0x6e, 0x6f, 0x66, 0x66, 0x2f, 0x23, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 8);
		CuAssertIntEquals(tc, pkt[i]->reserved, 2);
		CuAssertIntEquals(tc, pkt[i]->payload.subscribe.msgid, 2);
		CuAssertStrEquals(tc, pkt[i]->payload.subscribe.topic, "cmnd/sonoff/#");
		CuAssertIntEquals(tc, pkt[i]->payload.subscribe.qos, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// SUBSCRIBE
	{
		unsigned char buf[] = {0x82, 0x1a, 0x00, 0x04, 0x00, 0x15, 0x63, 0x6d, 0x6e, 0x64, 0x2f, 0x44, 0x56, 0x45, 0x53, 0x5f, 0x31, 0x43, 0x37, 0x46, 0x38, 0x32, 0x5f, 0x66, 0x62, 0x2f, 0x23, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 8);
		CuAssertIntEquals(tc, pkt[i]->reserved, 2);
		CuAssertIntEquals(tc, pkt[i]->payload.subscribe.msgid, 4);
		CuAssertStrEquals(tc, pkt[i]->payload.subscribe.topic, "cmnd/DVES_1C7F82_fb/#");
		CuAssertIntEquals(tc, pkt[i]->payload.subscribe.qos, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// SUBACK
	{
		unsigned char buf[] = {0x90, 0x03, 0x00, 0x02, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 9);
		CuAssertIntEquals(tc, pkt[i]->dub, 0);
		CuAssertIntEquals(tc, pkt[i]->qos, 0);
		CuAssertIntEquals(tc, pkt[i]->retain, 0);
		CuAssertIntEquals(tc, pkt[i]->payload.suback.msgid, 2);
		CuAssertIntEquals(tc, pkt[i]->payload.suback.qos, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PINGREQ
	{
		unsigned char buf[] = {0xc0, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 12);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// PINGRESP
	{
		unsigned char buf[] = {0xd0, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 13);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// DISCONNECT
	{
		unsigned char buf[] = {0xe0, 0x00};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 1, nr);
		CuAssertIntEquals(tc, pkt[i]->type, 14);
		CuAssertIntEquals(tc, pkt[i]->reserved, 0);
		mqtt_free(pkt[0]);
		FREE(pkt[0]);
		FREE(pkt);
	}

	// Multiple packets
	{
		unsigned char buf[] = {0x30, 0x26, 0x00, 0x20, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x6f, 0x6e, 0x6c, 0x69, 0x6e, 0x65, 0x74, 0x72, 0x75, 0x65, 0x30, 0x9b, 0x01, 0x00, 0x11, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x61, 0x6e, 0x6e, 0x6f, 0x75, 0x6e, 0x63, 0x65, 0x7b, 0x22, 0x69, 0x64, 0x22, 0x3a, 0x22, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x22, 0x2c, 0x22, 0x6d, 0x61, 0x63, 0x22, 0x3a, 0x22, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x50, 0x51, 0x52, 0x22, 0x2c, 0x22, 0x69, 0x70, 0x22, 0x3a, 0x22, 0x31, 0x39, 0x32, 0x2e, 0x31, 0x32, 0x2e, 0x32, 0x2e, 0x31, 0x22, 0x2c, 0x22, 0x6e, 0x65, 0x77, 0x5f, 0x66, 0x77, 0x22, 0x3a, 0x74, 0x72, 0x75, 0x65, 0x2c, 0x20, 0x22, 0x66, 0x77, 0x5f, 0x76, 0x65, 0x72, 0x22, 0x3a, 0x22, 0x32, 0x30, 0x31, 0x39, 0x30, 0x36, 0x31, 0x32, 0x2d, 0x30, 0x39, 0x30, 0x30, 0x30, 0x36, 0x2f, 0x46, 0x57, 0x31, 0x2e, 0x35, 0x2e, 0x30, 0x5f, 0x68, 0x6f, 0x74, 0x66, 0x69, 0x78, 0x2d, 0x48, 0x54, 0x2d, 0x50, 0x4d, 0x40, 0x35, 0x65, 0x61, 0x37, 0x30, 0x34, 0x36, 0x39, 0x22, 0x7d, 0x30, 0x25, 0x00, 0x21, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x72, 0x65, 0x6c, 0x61, 0x79, 0x2f, 0x30, 0x6f, 0x6e, 0x30, 0x24, 0x00, 0x21, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x69, 0x6e, 0x70, 0x75, 0x74, 0x2f, 0x30, 0x30, 0x30, 0x2d, 0x00, 0x27, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x72, 0x65, 0x6c, 0x61, 0x79, 0x2f, 0x30, 0x2f, 0x70, 0x6f, 0x77, 0x65, 0x72, 0x30, 0x2e, 0x30, 0x30, 0x30, 0x30, 0x00, 0x28, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x72, 0x65, 0x6c, 0x61, 0x79, 0x2f, 0x30, 0x2f, 0x65, 0x6e, 0x65, 0x72, 0x67, 0x79, 0x37, 0x34, 0x33, 0x33, 0x30, 0x31, 0x30, 0x2c, 0x00, 0x25, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x74, 0x65, 0x6d, 0x70, 0x65, 0x72, 0x61, 0x74, 0x75, 0x72, 0x65, 0x33, 0x37, 0x2e, 0x38, 0x38, 0x30, 0x2c, 0x00, 0x29, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x6f, 0x76, 0x65, 0x72, 0x74, 0x65, 0x6d, 0x70, 0x65, 0x72, 0x61, 0x74, 0x75, 0x72, 0x65, 0x30, 0x82, 0x15, 0x09, 0xcd, 0x00, 0x10, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x6e, 0x64, 0x01, 0x82, 0x26, 0x09, 0xce, 0x00, 0x21, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x6e, 0x64, 0x01, 0x82, 0x2e, 0x09, 0xcf, 0x00, 0x29, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x79, 0x31, 0x70, 0x6d, 0x2d, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x2f, 0x72, 0x65, 0x6c, 0x61, 0x79, 0x2f, 0x30, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x6e, 0x64, 0x01};

		struct mqtt_pkt_t **pkt = NULL;
		unsigned int nr = 0, i = 0;

		CuAssertIntEquals(tc, 0, mqtt_decode(&pkt, buf, sizeof(buf), &nr));
		CuAssertIntEquals(tc, 11, nr);
		for(i=0;i<nr;i++) {
			switch(i) {
				case 0: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/online");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "true");
				} break;
				case 1: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/announce");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "{\"id\":\"shelly1pm-ABCDEF\",\"mac\":\"ABCDEFGHIPQR\",\"ip\":\"192.12.2.1\",\"new_fw\":true, \"fw_ver\":\"20190612-090006/FW1.5.0_hotfix-HT-PM@5ea70469\"}");
				} break;
				case 2: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/relay/0");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "on");
				} break;
				case 3: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/input/0");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "0");
				} break;
				case 4: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/relay/0/power");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "0.00");
				} break;
				case 5: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/relay/0/energy");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "743301");
				} break;
				case 6: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/temperature");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "37.88");
				} break;
				case 7: {
					CuAssertIntEquals(tc, pkt[i]->type, 3);
					CuAssertIntEquals(tc, pkt[i]->dub, 0);
					CuAssertIntEquals(tc, pkt[i]->qos, 0);
					CuAssertIntEquals(tc, pkt[i]->retain, 0);
					CuAssertStrEquals(tc, pkt[i]->payload.publish.topic, "shellies/shelly1pm-ABCDEF/overtemperature");
					CuAssertStrEquals(tc, pkt[i]->payload.publish.message, "0");
				} break;
				case 8: {
					CuAssertIntEquals(tc, pkt[i]->type, 8);
					CuAssertIntEquals(tc, pkt[i]->reserved, 2);
					CuAssertIntEquals(tc, pkt[i]->payload.subscribe.msgid, 205);
					CuAssertStrEquals(tc, pkt[i]->payload.subscribe.topic, "shellies/command");
					CuAssertIntEquals(tc, pkt[i]->payload.subscribe.qos, 1);
				} break;
				case 9: {
					CuAssertIntEquals(tc, pkt[i]->type, 8);
					CuAssertIntEquals(tc, pkt[i]->reserved, 2);
					CuAssertIntEquals(tc, pkt[i]->payload.subscribe.msgid, 207);
					CuAssertStrEquals(tc, pkt[i]->payload.subscribe.topic, "shellies/shelly1pm-ABCDEF/command");
					CuAssertIntEquals(tc, pkt[i]->payload.subscribe.qos, 1);
				} break;
				case 10: {
					CuAssertIntEquals(tc, pkt[i]->type, 8);
					CuAssertIntEquals(tc, pkt[i]->reserved, 2);
					CuAssertIntEquals(tc, pkt[i]->payload.subscribe.msgid, 207);
					CuAssertStrEquals(tc, pkt[i]->payload.subscribe.topic, "shellies/shelly1pm-ABCDEF/relay/0/command");
					CuAssertIntEquals(tc, pkt[i]->payload.subscribe.qos, 1);
				} break;
			}
			mqtt_free(pkt[i]);
			FREE(pkt[i]);
		}
		FREE(pkt);
	}

	CuAssertIntEquals(tc, 0, xfree());
}
