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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/mqtt.h"
#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/network.h"
#include "../libs/pilight/core/eventpool.h"
#include "../libs/libuv/uv.h"

#include "alltests.h"

static CuTest *gtc = NULL;
static uv_timer_t *timer_req1 = NULL;
static uv_timer_t *timer_req2 = NULL;
static uv_timer_t *stop_timer_req = NULL;
static uv_timer_t *start_timer_req = NULL;
static uv_async_t *async_req = NULL;
static uv_thread_t pth;

static int running = 1;
static int test = 0;
static int foo = 0;
static int step1[17] = { 0 };
static int step2[17] = { 0 };

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

static void disconnect(uv_timer_t *handle) {
	struct mqtt_client_t *client = handle->data;
	mqtt_disconnect(client);
	uv_timer_stop(handle);
}

static void stop(uv_timer_t *handle) {
	running = 0;
	usleep(1000);
	mqtt_server_gc();
	mqtt_client_gc();
	uv_stop(uv_default_loop());
}

static void async_close_cb(uv_async_t *handle) {
	if(!uv_is_closing((uv_handle_t *)handle)) {
		uv_close((uv_handle_t *)handle, close_cb);
	}
	running = 0;
	usleep(1000);
	mqtt_server_gc();
	mqtt_client_gc();
	uv_stop(uv_default_loop());
}

static void mqtt_callback1(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	switch(test) {
		case 0: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1[pkt->type] == 0 || step1[pkt->type] == 1 || step1[pkt->type] == 2);
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/lamp/status") == 0) {
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/mqtt/pilight1") == 0) {
						CuAssertStrEquals(gtc, "connected", pkt->payload.publish.message);
					}
					step1[pkt->type]++;
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 1: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/lamp/status") == 0) {
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/mqtt/pilight1") == 0) {
						CuAssertStrEquals(gtc, "connected", pkt->payload.publish.message);
					}
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 2: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "+/+/+", 0);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/lamp/status") == 0) {
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/mqtt/pilight1") == 0) {
						CuAssertStrEquals(gtc, "connected", pkt->payload.publish.message);
					}
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 3: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 0);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 4: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 1);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					mqtt_puback(client, pkt->payload.publish.msgid);

					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 5: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 1);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type] >= 0);
					step1[pkt->type]++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					if(step1[pkt->type]++ >= 2) {
						CuAssertIntEquals(gtc, 1, pkt->dub);
					}
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 6: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 2);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type] >= 0);
					step1[pkt->type]++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 7: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 2);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 8: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 2);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					mqtt_pubrec(client, pkt->payload.publish.msgid);

					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					step1[pkt->type]++;
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 9: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/mqtt/pilight1") == 0) {
						CuAssertStrEquals(gtc, "disconnected", pkt->payload.publish.message);
					}
					step1[pkt->type]++;
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 10: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					mqtt_disconnect(client);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 11: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					if(strcmp(pkt->payload.publish.topic, "pilight/status") == 0) {
						CuAssertStrEquals(gtc, "offline", pkt->payload.publish.message);
					} else if(strcmp(pkt->payload.publish.topic, "pilight/mqtt/pilight1") == 0) {
						CuAssertStrEquals(gtc, "disconnected", pkt->payload.publish.message);
					}
					step1[pkt->type]++;
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				} else {
					step1[pkt->type]++;
				}
			}
		} break;
		case 12: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
				}  else {
					step1[pkt->type]++;
				}
			}
		} break;
	}
}

static void mqtt_callback2(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt, void *userdata) {
	switch(test) {
		case 0: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 1: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 2: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 3: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 4: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 5: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 6: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 7: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 8: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
					uv_async_send(async_req);
				} else {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				}
			}
		} break;
		case 9: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 1, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_DISCONNECTED) {
					step2[MQTT_DISCONNECTED]++;
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))stop, 500, 500);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 10: {
		} break;
		case 11: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))disconnect, 1500, 1500);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
				} else {
					step2[pkt->type]++;
				}
			}
		} break;
		case 12: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
				} else if(pkt->type == MQTT_DISCONNECTED) {
					CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
				}  else {
					step2[pkt->type]++;
				}
			}
		} break;
	}
}

int isclosed(int sock) {
  fd_set rfd;
  FD_ZERO(&rfd);
  FD_SET(sock, &rfd);
  struct timeval tv = { 0 };
  select(sock+1, &rfd, 0, 0, &tv);
  return FD_ISSET(sock, &rfd);
}

static void start_last_client() {
	struct sockaddr_in addr4;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	CuAssertTrue(gtc, sockfd > 0);

	unsigned long on = 1;
	CuAssertIntEquals(gtc, 0, setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(int)));

	memset(&addr4, '\0', sizeof(struct sockaddr_in));
	CuAssertIntEquals(gtc, 0, uv_ip4_addr("127.0.0.1", 11883, &addr4));
	CuAssertIntEquals(gtc, 0, connect(sockfd, (struct sockaddr *)&addr4, sizeof(addr4)));
	CuAssertIntEquals(gtc, 0, fcntl(sockfd, F_SETFL, O_NONBLOCK));
	char buf;

	usleep(1000);

	CuAssertIntEquals(gtc, 0, isclosed(sockfd));
	CuAssertIntEquals(gtc, -1, recv(sockfd, &buf, 1, 0));
	CuAssertIntEquals(gtc, 11, errno);

	sleep(1);

	CuAssertIntEquals(gtc, 1, isclosed(sockfd));
	CuAssertIntEquals(gtc, 0, recv(sockfd, &buf, 1, 0));
	CuAssertIntEquals(gtc, 11, errno);
}

static void start_clients(uv_async_t *handle) {
	usleep(1000);
	if(test == 9) {
		mqtt_client("127.0.0.1", 11883, "pilight1", NULL, NULL, mqtt_callback2, NULL);
		usleep(1000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
	} else if(test == 10) {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
	} else if(test == 11) {
		mqtt_client("127.0.0.1", 11883, "pilight1", "pilight/status", "offline", mqtt_callback2, NULL);
		usleep(100);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
	} else if(test == 12) {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
		usleep(5000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback2, NULL);
	} else {
		switch(foo++) {
			case 0: {
				mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
				uv_timer_start(start_timer_req, (void (*)(uv_timer_t *))start_clients, 100, 0);
			} break;
			case 1: {
				mqtt_client("127.0.0.1", 11883, "pilight1", NULL, NULL, mqtt_callback2, NULL);
			} break;
		}
	}
}

void test_mqtt_server_client(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	int i = 0, x = 0;

	test = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);


	for(i=0;i<14;i++) {
		mqtt_activate();

		timer_req1 = NULL;
		timer_req2 = NULL;

		running = 1;
		foo = 0;
		test = i;

		for(x=0;x<17;x++) {
			step1[x] = 0;
			step2[x] = 0;
		}

		switch(test) {
			case 0: {
				printf("[ %-48s ]\n", "- regular wildcard (#) subscription w/o ping");
			} break;
			case 1: {
				printf("[ %-48s ]\n", "- regular wildcard (#) subscription with ping");
			} break;
			case 2: {
				printf("[ %-48s ]\n", "- regular wildcard (+) subscription with ping");
			} break;
			case 3: {
				printf("[ %-48s ]\n", "- regular specific subscription with ping");
			} break;
			case 4: {
				printf("[ %-48s ]\n", "- QoS 1 with confirmations and ping");
			} break;
			case 5: {
				printf("[ %-48s ]\n", "- QoS 1 without confirmations and ping");
			} break;
			case 6: {
				printf("[ %-48s ]\n", "- QoS 2 with confirmations and ping");
			} break;
			case 7: {
				printf("[ %-48s ]\n", "- QoS 2 without confirmations and ping (1)");
			} break;
			case 8: {
				printf("[ %-48s ]\n", "- QoS 2 without confirmations and ping (2)");
			} break;
			case 9: {
				printf("[ %-48s ]\n", "- publish with/without retain without ping");
			} break;
			case 10: {
				printf("[ %-48s ]\n", "- client disconnect");
			} break;
			case 11: {
				printf("[ %-48s ]\n", "- client disconnect with last will");
			} break;
			case 12: {
				printf("[ %-48s ]\n", "- identical client id (not implemented)");
			} break;
			case 13: {
				printf("[ %-48s ]\n", "- client connecting but not identifying");
			} break;
		}

		eventpool_init(EVENTPOOL_NO_THREADS);

		async_req = MALLOC(sizeof(uv_async_t));
		if(async_req == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		uv_async_init(uv_default_loop(), async_req, async_close_cb);

		if((stop_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		uv_timer_init(uv_default_loop(), stop_timer_req);
		uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 2500, 0);

		mqtt_server(11883);

		if(test == 13) {
			uv_thread_create(&pth, start_last_client, NULL);
		} else {
			start_timer_req = MALLOC(sizeof(uv_timer_t));
			CuAssertPtrNotNull(tc, start_timer_req);

			uv_timer_init(uv_default_loop(), start_timer_req);
			uv_timer_start(start_timer_req, (void (*)(uv_timer_t *))start_clients, 500, 0);
		}

		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		uv_walk(uv_default_loop(), walk_cb, NULL);
		uv_run(uv_default_loop(), UV_RUN_ONCE);

		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}

		uv_thread_join(&pth);

		eventpool_gc();

		switch(test) {
			case 0: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 3, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 1: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 3, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 2: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertTrue(tc, step1[MQTT_PUBLISH] >= 1);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 3: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 4: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH && x != MQTT_PINGRESP &&
						 x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_PINGRESP && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 5: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertTrue(tc, step1[MQTT_PUBLISH] >= 2);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 6: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 7: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 8: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 2, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH&&
					   x != MQTT_PUBREL && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 9: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 10: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				for(x=0;x<17;x++) {
					CuAssertIntEquals(tc, 0, step2[x]);
				}
			} break;
			case 11: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 2, step1[MQTT_PUBLISH]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK &&
						 x != MQTT_PUBLISH) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step2[MQTT_DISCONNECTED]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_DISCONNECTED) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 12: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 13: {
				for(x=0;x<17;x++) {
					CuAssertIntEquals(tc, 0, step1[x]);
				}
				for(x=0;x<17;x++) {
					CuAssertIntEquals(tc, 0, step2[x]);
				}
			} break;
		}
	}

	CuAssertIntEquals(tc, 0, xfree());
}
