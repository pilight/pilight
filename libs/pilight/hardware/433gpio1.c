/*
       Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#ifndef _WIN32
#include <wiringx.h>
#endif

#include "../../libuv/uv.h"
#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/eventpool.h"
#ifdef PILIGHT_REWRITE
#include "hardware.h"
#else
#include "../config/hardware.h"
#include "../config/settings.h"
#endif
#include "433gpio1.h"

static int gpio_433_1_in = -1;
static int gpio_433_1_out = -1;
static int pollpri = UV_PRIORITIZED;
static uv_connect_t *connect_req = NULL;

#if defined(__arm__) || defined(__mips__) || defined(PILIGHT_UNITTEST)
typedef struct timestamp_t {
       unsigned long first;
       unsigned long second;
} timestamp_t;

typedef struct data_t {
       char rbuffer[1024];
       int rptr;
} data_t;

static struct data_t data;
static struct timestamp_t timestamp;

static void *reason_received_pulsetrain_free(void *param) {
       struct reason_received_pulsetrain_t *data = param;
       FREE(data);
       return NULL;
}

static void *reason_send_code_success_free(void *param) {
       struct reason_send_code_success_free *data = param;
       FREE(data);
       return NULL;
}

static void write_cb(uv_write_t *req, int status) {
       FREE(req);
}

static void poll_cb(uv_poll_t *req, int status, int events) {
       int duration = 0;
       int fd = req->io_watcher.fd;
       char *p = data.rbuffer;
       char num[12];

       if(events & pollpri) {
               uint8_t c = 0;

               (void)read(fd, &c, 1);
               lseek(fd, 0, SEEK_SET);

               struct timeval tv;
               gettimeofday(&tv, NULL);
               timestamp.first = timestamp.second;
               timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

               duration = (int)((int)timestamp.second-(int)timestamp.first);

               if(duration > 0) {
                       int len = snprintf(num, 12, "%d ", duration);
                       if(1024-(data.rptr+len) <= 0 || (duration > 10000 && data.rptr > 50)) {
                               uv_write_t *write_req = MALLOC(sizeof(uv_write_t));
                               if(write_req == NULL) {
                                       OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
                               }
                               uv_buf_t buf = uv_buf_init(p, data.rptr);
                               int r = uv_write((uv_write_t *)write_req, (uv_stream_t *)connect_req->handle, &buf, 1, write_cb);
                               data.rptr = 0;
                       }
                       memcpy(&p[data.rptr], num, len-1);
                       memcpy(&p[data.rptr+(len-1)], " ", 1);
                       data.rptr += len;
                       // data.rptr += snprintf(&p[data.rptr], 1024-data.rptr, "%d ", duration);
               }
       };
       if(events & UV_DISCONNECT) {
               FREE(req); /*LCOV_EXCL_LINE*/
       }
       return;
}

static void *gpio433_1Send(int reason, void *param) {
       struct reason_send_code_t *data1 = param;
       int *code = data1->pulses;
       int rawlen = data1->rawlen;
       int repeats = data1->txrpt;

       int r = 0, x = 0;
       if(gpio_433_1_out >= 0) {
               for(r=0;r<repeats;r++) {
                       for(x=0;x<rawlen;x+=2) {
                               digitalWrite(gpio_433_1_out, 1);
                               usleep((__useconds_t)code[x]);
                               digitalWrite(gpio_433_1_out, 0);
                               if(x+1 < rawlen) {
                                       usleep((__useconds_t)code[x+1]);
                               }
                       }
               }
               digitalWrite(gpio_433_1_out, 0);
       }

       struct reason_code_sent_success_t *data2 = MALLOC(sizeof(struct reason_code_sent_success_t));
       strcpy(data2->message, data1->message);
       strcpy(data2->uuid, data1->uuid);
       eventpool_trigger(REASON_CODE_SEND_SUCCESS, reason_send_code_success_free, data2);
       return NULL;
}

static void connect_cb(uv_connect_t *req, int status) {
       printf("== %d %p\n", status, req->handle);
       // FREE(req);
}
#endif

static unsigned short int gpio433_1HwInit(void) {
#if defined(__arm__) || defined(__mips__) || defined(PILIGHT_UNITTEST)

       /* Make sure the pilight sender gets
          the highest priority available */
#ifdef _WIN32
       SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
       struct sched_param sched;
       memset(&sched, 0, sizeof(sched));
       sched.sched_priority = 80;
       pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched);
#endif

       uv_poll_t *poll_req = NULL;
       char *platform = GPIO_PLATFORM;

#ifdef PILIGHT_REWRITE
       if(settings_select_string(ORIGIN_MASTER, "gpio-platform", &platform) != 0 || strcmp(platform, "none") == 0) {
               logprintf(LOG_ERR, "no gpio-platform configured");
               return EXIT_FAILURE;
       }
       if(wiringXSetup(platform, _logprintf) < 0) {
               return EXIT_FAILURE;
       }
#else
       if(settings_find_string("gpio-platform", &platform) != 0 || strcmp(platform, "none") == 0) {
               logprintf(LOG_ERR, "no gpio-platform configured");
               return EXIT_FAILURE;
       }
       if(wiringXSetup(platform, logprintf1) < 0) {
               return EXIT_FAILURE;
       }
#endif
       if(gpio_433_1_out >= 0) {
               if(wiringXValidGPIO(gpio_433_1_out) != 0) {
                       logprintf(LOG_ERR, "invalid sender pin: %d", gpio_433_1_out);
                       return EXIT_FAILURE;
               }
               pinMode(gpio_433_1_out, PINMODE_OUTPUT);
       }
       if(gpio_433_1_in >= 0) {
               if(wiringXValidGPIO(gpio_433_1_in) != 0) {
                       logprintf(LOG_ERR, "invalid receiver pin: %d", gpio_433_1_in);
                       return EXIT_FAILURE;
               }
               if(wiringXISR(gpio_433_1_in, ISR_MODE_BOTH) < 0) {
                       logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_433_1_in);
                       return EXIT_FAILURE;
               }
       }
       if(gpio_433_1_in >= 0) {
               if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
                       OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
               }
               int fd = wiringXSelectableFd(gpio_433_1_in);
               memset(data.rbuffer, '\0', sizeof(data.rbuffer));
               data.rptr = 0;

               uv_poll_init(uv_default_loop(), poll_req, fd);

#ifdef PILIGHT_UNITTEST
               char *dev = getenv("PILIGHT_433GPIO_READ");
               if(dev == NULL) {
                       pollpri = UV_PRIORITIZED; /*LCOV_EXCL_LINE*/
               } else {
                       pollpri = UV_READABLE;
               }
#endif
               uv_poll_start(poll_req, pollpri, poll_cb);
       }

       connect_req = MALLOC(sizeof(uv_connect_t));
       if(connect_req == NULL) {
               OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
       }
       uv_pipe_t *pipe_req = MALLOC(sizeof(uv_pipe_t));
       if(pipe_req == NULL) {
               OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
       }
       int r = 0;
       if((r = uv_pipe_init(uv_default_loop(), pipe_req, 1)) != 0) {
               logprintf(LOG_ERR, "uv_pipe_init: %s", uv_strerror(r));
               FREE(pipe_req);
               FREE(connect_req);
               return -1;
       }
       uv_pipe_connect(connect_req, pipe_req, "/tmp/pilight", connect_cb);

       eventpool_callback(REASON_SEND_CODE, gpio433_1Send);

       return EXIT_SUCCESS;
#else
       logprintf(LOG_ERR, "the 433gpio module is not supported on this hardware", gpio_433_1_in);
       return EXIT_FAILURE;
#endif
}

static unsigned short gpio433_1Settings(struct JsonNode *json) {
       if(strcmp(json->key, "receiver") == 0) {
               if(json->tag == JSON_NUMBER) {
                       gpio_433_1_in = (int)json->number_;
               } else {
                       return EXIT_FAILURE;
               }
       }
       if(strcmp(json->key, "sender") == 0) {
               if(json->tag == JSON_NUMBER) {
                       gpio_433_1_out = (int)json->number_;
               } else {
                       return EXIT_FAILURE;
               }
       }
       return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void gpio433_1Init(void) {
       hardware_register(&gpio433_1);
       hardware_set_id(gpio433_1, "433gpio1");

       options_add(&gpio433_1->options, 'r', "receiver", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
       options_add(&gpio433_1->options, 's', "sender", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");

       gpio433_1->minrawlen = 1000;
       gpio433_1->maxrawlen = 0;
       gpio433_1->mingaplen = 5100;
       gpio433_1->maxgaplen = 10000;

       gpio433_1->hwtype=RF433_1;
       gpio433_1->comtype=COMOOK;
       gpio433_1->init=&gpio433_1HwInit;
       gpio433_1->settings=&gpio433_1Settings;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
       module->name = "433gpio1";
       module->version = "2.0";
       module->reqversion = "8.0";
       module->reqcommit = NULL;
}

void init(void) {
       gpio433_1Init();
}
#endif
