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
static uv_timer_t *timer_req1 = NULL;
static uv_timer_t *timer_req2 = NULL;
static uv_timer_t *stop_timer_req = NULL;
static uv_thread_t pth;

static int test = 0;
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

static void ping(uv_timer_t *handle) {
	mqtt_ping(handle->data);
}

static void stop(uv_timer_t *handle) {
	mqtt_gc();
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
					CuAssertTrue(gtc, step1[pkt->type] == 0 || step1[pkt->type] == 1);
					if(step1[pkt->type] == 0) {
						CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else {
						CuAssertStrEquals(gtc, "pilight/lamp/status", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					}
					step1[pkt->type]++;
				} else {
					step1[0]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else {
						CuAssertStrEquals(gtc, "pilight/lamp/status", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					}
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else {
						CuAssertStrEquals(gtc, "pilight/lamp/status", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					}
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					mqtt_puback(client, pkt->payload.publish.msgid);

					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}

					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					if(step1[pkt->type]++ >= 2) {
						CuAssertIntEquals(gtc, 1, pkt->dub);
					}
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					mqtt_pubrec(client, pkt->payload.publish.msgid);

					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else if(pkt->type == MQTT_PUBREL) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_pubcomp(client, pkt->payload.pubrel.msgid);
				} else if(pkt->type == MQTT_PUBCOMP) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 7: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 2);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_SUBACK) {
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {

					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					mqtt_pubrec(client, pkt->payload.publish.msgid);

					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					step1[pkt->type]++;
				} else if(pkt->type == MQTT_PUBREL) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else if(pkt->type == MQTT_PUBCOMP) {
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
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
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 10: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 100, 100);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					uv_timer_stop(timer_req1);
					mqtt_disconnect(client);
				} else {
					step1[pkt->type]++;
				}
			} else {
				step1[MQTT_DISCONNECTED]++;
			}
		} break;
		case 11: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					if(timer_req1 == NULL) {
						if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}

						timer_req1->data = client;
						uv_timer_init(uv_default_loop(), timer_req1);
						uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
					}
					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertStrEquals(gtc, "pilight/status", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "offline", pkt->payload.publish.message);
					CuAssertTrue(gtc, step1[pkt->type]++ >= 0);
				} else {
					step1[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step1[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 12: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);

					CuAssertIntEquals(gtc, step1[pkt->type]++, 0);
				} else {
					step1[pkt->type]++;
				}
			} else {
				uv_timer_stop(timer_req1);
				mqtt_client_remove(client->poll_req, 0);
				step1[MQTT_DISCONNECTED]++;
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
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 1: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 2: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 3: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 4: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 100, 100);

					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 5: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 100, 100);

					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 6: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 100, 100);

					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 7: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 100, 100);

					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 8: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 100, 100);

					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				} else {
					CuAssertTrue(gtc, step2[pkt->type]++ >= 0);
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 9: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
					mqtt_publish(client, 0, 0, 1, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2[pkt->type]++;
				}
			} else {
				step2[MQTT_DISCONNECTED]++;
				if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}

				timer_req2->data = client;
				uv_timer_init(uv_default_loop(), timer_req2);
				uv_timer_start(timer_req2, (void (*)(uv_timer_t *))stop, 500, 500);
			}
		} break;
		case 10: {
		} break;
		case 11: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
			}
		} break;
		case 12: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					if((timer_req2 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req2->data = client;
					uv_timer_init(uv_default_loop(), timer_req2);
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 500, 500);

					CuAssertIntEquals(gtc, step2[pkt->type]++, 0);
				} else {
					step2[pkt->type]++;
				}
			} else {
				CuAssertIntEquals(gtc, step2[MQTT_DISCONNECTED]++, 0);
			}
		} break;
	}
}

static void start_clients(void *param) {
	usleep(1000);
	if(test == 9) {
		mqtt_client("127.0.0.1", 11883, "pilight1", NULL, NULL, mqtt_callback2, NULL);
		usleep(1000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
	} else if(test == 10) {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
	} else if(test == 11) {
		mqtt_client("127.0.0.1", 11883, "pilight1", "pilight/status", "offline", mqtt_callback2, NULL);
		usleep(2000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
	} else if(test == 12) {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
		usleep(1000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback2, NULL);
	} else {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1, NULL);
		usleep(1000);
		mqtt_client("127.0.0.1", 11883, "pilight1", NULL, NULL, mqtt_callback2, NULL);
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

	for(i=0;i<13;i++) {
		timer_req1 = NULL;
		timer_req2 = NULL;

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
				printf("[ %-48s ]\n", "- identical client id");
			} break;
		}

		eventpool_init(EVENTPOOL_NO_THREADS);

		if((stop_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		uv_timer_init(uv_default_loop(), stop_timer_req);
		uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 1000, 0);

		mqtt_server(11883);
		uv_thread_create(&pth, start_clients, NULL);

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
				CuAssertIntEquals(tc, 2, step1[MQTT_PUBLISH]);
				CuAssertIntEquals(tc, 1, step1[MQTT_DISCONNECTED]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
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
			case 1: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 2, step1[MQTT_PUBLISH]);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 1);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_PINGRESP) {
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
			case 2: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertTrue(tc, step1[MQTT_PUBLISH] >= 1);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 1);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_PINGRESP) {
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
			case 3: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 1);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_PINGRESP) {
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
			case 4: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				CuAssertTrue(tc, step2[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 5: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertTrue(tc, step1[MQTT_PUBLISH] >= 15);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				CuAssertTrue(tc, step2[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 6: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBREL]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBCOMP]);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH &&
					   x != MQTT_PINGRESP && x != MQTT_PUBCOMP && x != MQTT_PUBREL) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				CuAssertTrue(tc, step2[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 7: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertTrue(gtc, step1[MQTT_PUBLISH] >= 5);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH &&
					   x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				CuAssertTrue(tc, step2[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 8: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertTrue(gtc, step1[MQTT_PUBLISH] >= 5);
				CuAssertTrue(gtc, step1[MQTT_PUBREL] >= 5);
				CuAssertTrue(tc, step1[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH &&
					   x != MQTT_PINGRESP && x != MQTT_PUBREL) {
						CuAssertIntEquals(tc, 0, step1[x]);
					}
				}
				CuAssertIntEquals(tc, 1, step2[MQTT_CONNACK]);
				CuAssertTrue(tc, step2[MQTT_PINGRESP] >= 5);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_PINGRESP) {
						CuAssertIntEquals(tc, 0, step2[x]);
					}
				}
			} break;
			case 9: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				CuAssertIntEquals(tc, 1, step1[MQTT_DISCONNECTED]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
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
			case 10: {
				CuAssertIntEquals(tc, 1, step1[MQTT_CONNACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_SUBACK]);
				CuAssertIntEquals(tc, 1, step1[MQTT_PINGRESP]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PINGRESP) {
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
				CuAssertIntEquals(tc, 1, step1[MQTT_PUBLISH]);
				CuAssertIntEquals(tc, 1, step1[MQTT_DISCONNECTED]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_SUBACK && x != MQTT_PUBLISH && x != MQTT_DISCONNECTED) {
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
				CuAssertIntEquals(tc, 1, step1[MQTT_DISCONNECTED]);
				for(x=0;x<17;x++) {
					if(x != MQTT_CONNACK && x != MQTT_DISCONNECTED) {
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
		}
	}

	CuAssertIntEquals(tc, 0, xfree());
}
