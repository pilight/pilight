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
static int step1 = 0;
static int step2 = 0;

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

static void mqtt_callback1(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt) {
	switch(test) {
		case 0: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1 == 2 || step1 == 3);
					if(step1 == 2) {
						CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else {
						CuAssertStrEquals(gtc, "pilight/lamp/status", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					}
					step1++;
				} else {
					step1++;
				}
			} else {
				CuAssertIntEquals(gtc, step1++, 4);
			}
		} break;
		case 1: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "#", 0);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 8);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 6);
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else {
						CuAssertStrEquals(gtc, "pilight/lamp/status", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					}
					step1++;
				} else {
					step1++;
				}
			} else {
				CuAssertIntEquals(gtc, step1++, 6);
			}
		} break;
		case 2: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "+/+/+", 0);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 8);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 6);
					if(strcmp(pkt->payload.publish.topic, "tele/sonoff/POWER") == 0) {
						CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					} else {
						CuAssertStrEquals(gtc, "pilight/lamp/status", pkt->payload.publish.topic);
						CuAssertStrEquals(gtc, "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}", pkt->payload.publish.message);
					}
					step1++;
				} else {
					step1++;
				}
			} else {
				CuAssertIntEquals(gtc, step1++, 6);
			}
		} break;
		case 3: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 0);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 6);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 6);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else {
					step1++;
				}
			}
		} break;
		case 4: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 1);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 7);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					mqtt_puback(client, pkt->payload.publish.msgid);

					CuAssertTrue(gtc, step1 >= 2 && step1 <= 7);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					step1++;
				} else {
					step1++;
				}
			}
		} break;
		case 5: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 1);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 12);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 12);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					if(step1 > 4) {
						CuAssertIntEquals(gtc, 1, pkt->dub);
					}
					step1++;
				} else {
					step1++;
				}
			}
		} break;
		case 6: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 2);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 1 && step1 <= 10);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertTrue(gtc, step1 >= 1 && step1 <= 10);
					step1++;
				} else if(pkt->type == MQTT_PUBLISH) {
					mqtt_pubrec(client, pkt->payload.publish.msgid);

					CuAssertTrue(gtc, step1 >= 1 && step1 <= 10);
					step1++;
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					step1++;
				} else if(pkt->type == MQTT_PUBREL) {
					CuAssertTrue(gtc, step1 >= 1 && step1 <= 10);
					step1++;

					mqtt_pubcomp(client, pkt->payload.pubrel.msgid);
				} else if(pkt->type == MQTT_PUBCOMP) {
					CuAssertTrue(gtc, step1 >= 1 && step1 <= 10);
					step1++;
				} else {
					step1++;
				}
			}
		} break;
		case 7: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 2);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 13);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 13);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					step1++;
				} else {
					step1++;
				}
			}
		} break;
		case 8: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "tele/sonoff/POWER", 2);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 20);
					step1++;
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					mqtt_pubrec(client, pkt->payload.publish.msgid);

					CuAssertTrue(gtc, step1 >= 2 && step1 <= 20);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
					step1++;
				} else if(pkt->type == MQTT_PUBREL) {
					CuAssertTrue(gtc, step1 >= 2 && step1 <= 20);
					step1++;
				} else if(pkt->type == MQTT_PUBCOMP) {
					step1++;
				} else {
					step1++;
				}
			}
		} break;
		case 9: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertIntEquals(gtc, step1++, 2);
					CuAssertStrEquals(gtc, "tele/sonoff/POWER", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "on", pkt->payload.publish.message);
				} else {
					step1++;
				}
			} else {
				CuAssertTrue(gtc, step1 >= 2 && step1 <= 4);
			}
		} break;
		case 10: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertIntEquals(gtc, step1++, 2);
					uv_timer_stop(timer_req1);
					mqtt_disconnect(client);
				} else {
					step1++;
				}
			} else {
				step1++;
			}
		} break;
		case 11: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step1++, 0);

					if((timer_req1 = MALLOC(sizeof(uv_timer_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}

					timer_req1->data = client;
					uv_timer_init(uv_default_loop(), timer_req1);
					uv_timer_start(timer_req1, (void (*)(uv_timer_t *))ping, 500, 500);

					mqtt_subscribe(client, "#", 0);
				} else if(pkt->type == MQTT_SUBACK) {
					CuAssertIntEquals(gtc, step1++, 1);
				} else if(pkt->type == MQTT_PUBLISH) {
					CuAssertStrEquals(gtc, "pilight/status", pkt->payload.publish.topic);
					CuAssertStrEquals(gtc, "offline", pkt->payload.publish.message);
					CuAssertTrue(gtc, step1 == 3 || step1 == 4);
					step1++;
				} else {
					step1++;
				}
			} else {
				CuAssertIntEquals(gtc, step1++, 2);
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

					CuAssertIntEquals(gtc, step1++, 0);
				} else {
					step1++;
				}
			} else {
				uv_timer_stop(timer_req1);
				mqtt_client_remove(client->poll_req, 0);
				step1++;
			}
		} break;
	}
}

static void mqtt_callback2(struct mqtt_client_t *client, struct mqtt_pkt_t *pkt) {
	switch(test) {
		case 0: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2++;
				}
			} else {
				CuAssertIntEquals(gtc, step2++, 1);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 1: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2++;
				}
			} else {
				CuAssertIntEquals(gtc, step2++, 1);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 2: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2++;
				}
			} else {
				CuAssertIntEquals(gtc, step2++, 1);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 3: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertTrue(gtc, step2++ == 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2++;
				}
			} else {
				CuAssertTrue(gtc, step2++ == 1);
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
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 500, 500);

					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2 >= 1 && step2 <= 5);
					step2++;
				} else {
					step2++;
				}
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
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 500, 500);

					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2 >= 1 && step2 <= 5);
					step2++;
				} else {
					step2++;
				}
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
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 500, 500);

					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2 >= 1 && step2 <= 5);
					step2++;
				} else {
					step2++;
				}
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
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 500, 500);

					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2 >= 1 && step2 <= 5);
					step2++;
				} else {
					step2++;
				}
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
					uv_timer_start(timer_req2, (void (*)(uv_timer_t *))ping, 500, 500);

					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 0, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else if(pkt->type == MQTT_PINGRESP) {
					CuAssertTrue(gtc, step2 >= 1 && step2 <= 5);
					step2++;
				} else {
					step2++;
				}
			}
		} break;
		case 9: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2++, 0);
					mqtt_publish(client, 0, 0, 1, "tele/sonoff/POWER", "on");
					mqtt_publish(client, 0, 0, 0, "pilight/lamp/status", "{\"Time\":\"1970-01-01T02:32:27\",\"Uptime\":\"0T02:25:16\",\"Vcc\":3.238,\"SleepMode\":\"Dynamic\",\"Sleep\":50,\"LoadAvg\":19,\"POWER\":\"ON\",\"Wifi\":{\"AP\":1,\"SSId\":\"pilight\",\"BSSId\":\"AA:BB:CC:DD:EE:FF\",\"Channel\":2,\"RSSI\":82,\"LinkCount\":1,\"Downtime\":\"0T00:00:06\"}}");
				} else {
					step2++;
				}
			} else {
				CuAssertIntEquals(gtc, step2++, 1);
				mqtt_gc();
				uv_stop(uv_default_loop());
			}
		} break;
		case 10: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2++, 0);
				} else {
					step2++;
				}
			} else {
				CuAssertIntEquals(gtc, step2++, 1);
			}
		} break;
		case 11: {
			if(pkt != NULL) {
				if(pkt->type == MQTT_CONNACK) {
					CuAssertIntEquals(gtc, step2++, 0);
				} else {
					step2++;
				}
			} else {
				CuAssertIntEquals(gtc, step2++, 1);
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

					CuAssertIntEquals(gtc, step2++, 0);
				} else {
					step2++;
				}
			} else {
				CuAssertIntEquals(gtc, step2++, 1);
			}
		} break;
	}
}

static void start_clients(void *param) {
	usleep(1000);
	if(test == 9) {
		mqtt_client("127.0.0.1", 11883, "pilight1", NULL, NULL, mqtt_callback2);
		usleep(1000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1);
	} else if(test == 10) {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1);
	} else if(test == 11) {
		mqtt_client("127.0.0.1", 11883, "pilight1", "pilight/status", "offline", mqtt_callback2);
		usleep(1000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1);
	} else if(test == 12) {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1);
		usleep(1000);
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback2);
	} else {
		mqtt_client("127.0.0.1", 11883, "pilight", NULL, NULL, mqtt_callback1);
		mqtt_client("127.0.0.1", 11883, "pilight1", NULL, NULL, mqtt_callback2);
	}
}

void test_mqtt_server_client(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	gtc = tc;

	int i = 0;

	test = 0;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	for(i=0;i<13;i++) {
		test = i;
		step1 = 0;
		step2 = 0;

		switch(test) {
			case 0: {
				printf("[ %.2d %-45s ]\n", i+1, "regular wildcard (#) subscription w/o ping");
			} break;
			case 1: {
				printf("[ %.2d %-45s ]\n", i+1, "regular wildcard (#) subscription with ping");
			} break;
			case 2: {
				printf("[ %.2d %-45s ]\n", i+1, "regular wildcard (+) subscription with ping");
			} break;
			case 3: {
				printf("[ %.2d %-45s ]\n", i+1, "regular specific subscription with ping");
			} break;
			case 4: {
				printf("[ %.2d %-45s ]\n", i+1, "QoS 1 with confirmations and ping");
			} break;
			case 5: {
				printf("[ %.2d %-45s ]\n", i+1, "QoS 1 without confirmations and ping");
			} break;
			case 6: {
				printf("[ %.2d %-45s ]\n", i+1, "QoS 2 with confirmations and ping");
			} break;
			case 7: {
				printf("[ %.2d %-45s ]\n", i+1, "QoS 2 without confirmations and ping (1)");
			} break;
			case 8: {
				printf("[ %.2d %-45s ]\n", i+1, "QoS 2 without confirmations and ping (2)");
			} break;
			case 9: {
				printf("[ %.2d %-45s ]\n", i+1, "publish with/without retain without ping");
			} break;
			case 10: {
				printf("[ %.2d %-45s ]\n", i+1, "client disconnect");
			} break;
			case 11: {
				printf("[ %.2d %-45s ]\n", i+1, "client disconnect with last will");
			} break;
			case 12: {
				printf("[ %.2d %-45s ]\n", i+1, "identical client id");
			} break;
		}

		eventpool_init(EVENTPOOL_NO_THREADS);

		if((stop_timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		uv_timer_init(uv_default_loop(), stop_timer_req);
		uv_timer_start(stop_timer_req, (void (*)(uv_timer_t *))stop, 2750, 0);

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
				CuAssertIntEquals(tc, 5, step1);
				CuAssertIntEquals(tc, 2, step2);
			} break;
			case 1: {
				CuAssertTrue(tc, step1 >= 6 && step1 <= 9);
				CuAssertIntEquals(tc, 2, step2);
			} break;
			case 2: {
				CuAssertTrue(tc, step1 >= 6 && step1 <= 9);
				CuAssertIntEquals(tc, 2, step2);
			} break;
			case 3: {
				CuAssertTrue(tc, step1 >= 4 && step1 <= 7);
				CuAssertIntEquals(tc, 2, step2);
			} break;
			case 4: {
				CuAssertIntEquals(tc, 8, step1);
				CuAssertTrue(tc, step2 >= 4 && step2 <= 6);
			} break;
			case 5: {
				CuAssertTrue(tc, step1 >= 11 && step1 <= 13);
				CuAssertTrue(tc, step2 >= 4 && step2 <= 6);
			} break;
			case 6: {
				CuAssertIntEquals(tc, 11, step1);
				CuAssertTrue(tc, step2 >= 4 && step2 <= 6);
			} break;
			case 7: {
				CuAssertTrue(tc, step1 >= 11 && step1 <= 13);
				CuAssertTrue(tc, step2 >= 4 && step2 <= 6);
			} break;
			case 8: {
				CuAssertTrue(tc, step1 >= 6 && step1 <= 19);
				CuAssertTrue(tc, step2 >= 4 && step2 <= 6);
			} break;
			case 9: {
				CuAssertIntEquals(tc, 3, step1);
				CuAssertIntEquals(tc, 2, step2);
			} break;
			case 10: {
				CuAssertIntEquals(tc, 3, step1);
				CuAssertIntEquals(tc, 0, step2);
			} break;
			case 11: {
				CuAssertIntEquals(tc, 8, step1);
				CuAssertIntEquals(tc, 2, step2);
			} break;
			case 12: {
				CuAssertIntEquals(tc, 2, step1);
				CuAssertTrue(tc, step2 >= 4 && step2 <= 6);
			} break;
		}
	}

	CuAssertIntEquals(tc, 0, xfree());
}
