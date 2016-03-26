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

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/threadpool.h"
#include "../core/eventpool.h"
#include "../core/time.h"
#include "../hardware/hardware.h"
#include "../../wiringx/wiringX.h"
#include "433gpio.h"

static void* gpio433SendHandler(void *arg);
static int gpio433SendQueue(int *code, int rawlen, int repeats);
static int gpio433Send(int *code, int rawlen, int repeats);
static int gpio433SendCSMACD(int *code, int rawlen, int repeats);
static void gpio433ReportRawCode(int *code, int rawlen, unsigned long duration, struct timespec* endts);
static void gpio433ReportCode(int *code, int rawlen, unsigned long duration, struct timespec* endts);

typedef struct data_t {
	int rbuffer[1024];
	int rptr;
	unsigned long duration;
	void *(*callback)(void *);
} data_t;

typedef enum mediaaccesscontrol_t {
	MACNONE = 0,
	MACCSMA,
	MACCSMACD
} mediaaccesscontrol_t;

typedef struct sendqueue_t {
	int* code;
	int rawlen;
	int repeats;
	int retry;
	struct sendqueue_t* next;
} sendqueue_t;

typedef struct csmadata_t {
	int last_rawlen;
	struct timespec send_lockedts;
	pthread_mutex_t send_lock;
	pthread_cond_t send_signal;
	pthread_condattr_t send_signal_attr;
	struct sendqueue_t *sendqueue_head;
	struct sendqueue_t *sendqueue_tail;
	pthread_mutex_t sendqueue_lock;
	pthread_cond_t sendqueue_signal;
	pthread_t sendthread;
	bool sendthread_stop;
	int (*sendimpl)(int *code, int rawlen, int repeats);
} csmadata_t;

static int gpio_433_in = 0;
static int gpio_433_out = 0;
static int doPause = 0;

static struct timespec receive_last;

static enum mediaaccesscontrol_t mac;
static unsigned int max_retries;
static unsigned int wait_n_frames;
static unsigned long collision_waitmin;
static unsigned long collision_waitunit;
static unsigned int collision_waitmaxunits;
static struct csmadata_t* csmadata;

static unsigned long calc_duration(int *code, int rawlen, unsigned long duration) {
	if(duration == 0) {
		int i;
		for(i=0;i<rawlen;i++) {
			duration += code[i];
		}
	}

	return duration;
}

static void send_lock(unsigned long duration, struct timespec* from) {
	struct timespec tmp;
	if(from == NULL) {
		clock_gettime(CLOCK_MONOTONIC, &tmp);
	} else {
		tmp = *from;
	}

	time_add_us(&tmp, duration);
	if(time_cmp(&tmp, &csmadata->send_lockedts) > 0) {
		csmadata->send_lockedts = tmp;
	}
}

static sendqueue_t *sendqueue_pop_front() {
	if(csmadata->sendqueue_head == NULL) {
		return NULL;
	}
	struct sendqueue_t *tmp = csmadata->sendqueue_head;
	csmadata->sendqueue_head = csmadata->sendqueue_head->next;
	if(csmadata->sendqueue_head == NULL) {
		csmadata->sendqueue_tail = NULL;
	}
	return tmp;
}

static void sendqueue_push_front(sendqueue_t* entry) {
	if(csmadata->sendqueue_head == NULL) {
		entry->next = NULL;
		csmadata->sendqueue_head = csmadata->sendqueue_tail = entry;
		return;
	}
	entry->next = csmadata->sendqueue_head;
	csmadata->sendqueue_head = entry;
}

static void sendqueue_push_back(sendqueue_t* entry) {
	entry->next = NULL;
	if(csmadata->sendqueue_tail == NULL) {
		csmadata->sendqueue_head = csmadata->sendqueue_tail = entry;
		return;
	}
	csmadata->sendqueue_tail->next = entry;
	csmadata->sendqueue_tail = entry;
}

static void *reason_received_pulsetrain_free(void *param) {
	struct reason_received_pulsetrain_t *data = param;
	FREE(data);
	return NULL;
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	struct data_t *data = node->userdata;

#ifdef _WIN32
	if(InterlockedExchangeAdd(&doPause, 0) == 1) {
#else
	if(__sync_add_and_fetch(&doPause, 0) == 1) {
#endif
		return 0;
	}
	switch(event) {
		case EV_POLL: {
			eventpool_fd_enable_highpri(node);
		} break;
		case EV_CONNECT_SUCCESS: {
			eventpool_fd_enable_highpri(node);
			clock_gettime(CLOCK_MONOTONIC, &receive_last);
			data->duration = 0;
		} break;
		case EV_HIGHPRI: {
			uint8_t c = 0;
			struct timespec now;
			struct timespec tmp;

			(void)read(node->fd, &c, 1);
			lseek(node->fd, 0, SEEK_SET);

			clock_gettime(CLOCK_MONOTONIC, &now);
			time_diff(&now, &receive_last, &tmp);
			receive_last = now;

			int duration = (int) time_long_us(&tmp);
			if(duration > 0) {
				data->rbuffer[data->rptr++] = duration;
				data->duration += duration;
				if(data->rptr > MAXPULSESTREAMLENGTH-1) {
					data->rptr = 0;
				}
				if(duration > gpio433->mingaplen) {
					/* Let's do a little filtering here as well */
					if(data->rptr >= gpio433->minrawlen && data->rptr <= gpio433->maxrawlen) {
						if(mac != MACNONE) {
							gpio433ReportRawCode(data->rbuffer, data->rptr, data->duration, &now);
						}

						struct reason_received_pulsetrain_t *data1 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
						if(data1 == NULL) {
							OUT_OF_MEMORY
						}
						data1->length = data->rptr;
						memcpy(data1->pulses, data->rbuffer, data->rptr*sizeof(int));
						data1->duration = data->duration;
						data1->endts = now;
						data1->hardware = gpio433->id;

						eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data1);
					}
					data->rptr = 0;
					data->duration = 0;
				}
			}

			eventpool_fd_enable_highpri(node);
		} break;
		case EV_DISCONNECTED: {
			close(node->fd);
			FREE(node->userdata);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

static unsigned short gpio433HwInit(void *(*callback)(void *)) {
#if defined(__arm__) || defined(__mips__)
	char *platform = GPIO_PLATFORM;
	if(settings_select_string(ORIGIN_MASTER, "gpio-platform", &platform) != 0 || strcmp(platform, "none") == 0) {
		logprintf(LOG_ERR, "no gpio-platform configured");
		return EXIT_FAILURE;
	}
	if(wiringXSetup(platform, logprintf) < 0) {
		return EXIT_FAILURE;
	}
	if(gpio_433_out >= 0) {
		if(wiringXValidGPIO(gpio_433_out) != 0) {
			logprintf(LOG_ERR, "invalid sender pin: %d", gpio_433_out);
			return EXIT_FAILURE;
		}
		pinMode(gpio_433_out, PINMODE_OUTPUT);
	}
	if(gpio_433_in >= 0) {
		if(wiringXValidGPIO(gpio_433_in) != 0) {
			logprintf(LOG_ERR, "invalid receiver pin: %d", gpio_433_in);
			return EXIT_FAILURE;
		}
		if(wiringXISR(gpio_433_in, ISR_MODE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_433_in);
			return EXIT_SUCCESS;
		}
	}
	if(gpio_433_in > 0) {
		int fd = wiringXSelectableFd(gpio_433_in);

		struct data_t *data = MALLOC(sizeof(struct data_t));
		if(data == NULL) {
			OUT_OF_MEMORY;
		}
		memset(data->rbuffer, '\0', sizeof(data->rbuffer));
		data->rptr = 0;
		data->callback = callback;

		switch(mac) {
		case MACNONE:
			break;
		case MACCSMACD:
			/* no break */
		case MACCSMA:
			gpio433->sendOOK = &gpio433SendQueue;
			gpio433->reportCode = &gpio433ReportCode;

			csmadata = (struct csmadata_t*) malloc(sizeof(struct csmadata_t));
			if(csmadata == NULL) {
				OUT_OF_MEMORY;
			}
			csmadata->last_rawlen = 0;
			csmadata->send_lockedts.tv_sec = 0;
			csmadata->send_lockedts.tv_nsec = 0;
			pthread_condattr_init(&csmadata->send_signal_attr);
			pthread_condattr_setclock(&csmadata->send_signal_attr, CLOCK_MONOTONIC);
			pthread_mutex_init(&csmadata->send_lock, NULL);
			pthread_cond_init(&csmadata->send_signal, &csmadata->send_signal_attr);
			csmadata->sendqueue_head = csmadata->sendqueue_tail = NULL;
			pthread_mutex_init(&csmadata->sendqueue_lock, NULL);
			pthread_cond_init(&csmadata->sendqueue_signal, NULL);
			csmadata->sendimpl = (mac == MACCSMA) ? &gpio433Send : &gpio433SendCSMACD;
			csmadata->sendthread_stop = false;

			if(pthread_create(&csmadata->sendthread, NULL, gpio433SendHandler, NULL)) {
				logprintf(LOG_ERR, "not able to create send thread");
				return EXIT_FAILURE;
			}
			break;
		default:
			logprintf(LOG_ERR, "invalid mac: %d", mac);
			return EXIT_FAILURE;
		}

		eventpool_fd_add("433gpio", fd, client_callback, NULL, data);
	}
	return EXIT_SUCCESS;
#else
	logprintf(LOG_ERR, "the 433gpio module is not supported on this hardware", gpio_433_in);
	return EXIT_FAILURE;
#endif
}

static unsigned short gpio433HwDeinit(void) {
	switch(mac) {
	case MACCSMACD:
		/* no break */
	case MACCSMA:
		csmadata->sendthread_stop = true;
		pthread_mutex_lock(&csmadata->sendqueue_lock);
		pthread_cond_signal(&csmadata->sendqueue_signal);
		pthread_mutex_unlock(&csmadata->sendqueue_lock);
		pthread_mutex_lock(&csmadata->send_lock);
		pthread_cond_signal(&csmadata->send_signal);
		pthread_mutex_unlock(&csmadata->send_lock);

		pthread_join(csmadata->sendthread, NULL);

		while (csmadata->sendqueue_head != NULL) {
			struct sendqueue_t *entry = sendqueue_pop_front();
			free(entry);
		}
		free(csmadata);
		break;
	case MACNONE:
		break;
	default:
		logprintf(LOG_ERR, "invalid mac: %d", mac);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static unsigned short gpio433Settings(JsonNode *json) {
	if(strcmp(json->key, "receiver") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_433_in = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	} else if(strcmp(json->key, "sender") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_433_out = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	} else if(strcmp(json->key, "media-access-control") == 0) {
		if(json->tag == JSON_STRING) {
			if(strcmp(json->string_, "none") == 0) {
				mac = MACNONE;
			} else if(strcmp(json->string_, "csma") == 0) {
				mac = MACCSMA;
			} else if(strcmp(json->string_, "csma/cd") == 0) {
				mac = MACCSMACD;
			} else {
				return EXIT_FAILURE;
			}
		} else {
			return EXIT_FAILURE;
		}
	}

	switch(mac) {
	case MACCSMACD:
		if(strcmp(json->key, "csmacd-wait-min") == 0) {
			if(json->tag == JSON_NUMBER) {
				collision_waitmin = (int)json->number_;
			} else {
				return EXIT_FAILURE;
			}
		} else if(strcmp(json->key, "csmacd-wait-unit") == 0) {
			if(json->tag == JSON_NUMBER) {
				collision_waitunit = (int)json->number_;
			} else {
				return EXIT_FAILURE;
			}
		} else if(strcmp(json->key, "csmacd-wait-max-units") == 0) {
			if(json->tag == JSON_NUMBER) {
				collision_waitmaxunits = (int)json->number_;
			} else {
				return EXIT_FAILURE;
			}
		}
		break;
	case MACCSMA:
		if(strcmp(json->key, "csma-wait-n-frames") == 0) {
			if(json->tag == JSON_NUMBER) {
				wait_n_frames = (int)json->number_;
			} else {
				return EXIT_FAILURE;
			}
		}
		break;
	case MACNONE:
		break;
	default:
		logprintf(LOG_ERR, "invalid mac: %d", mac);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void *gpio433SendHandler(void* arg) {
	struct sendqueue_t *entry = NULL;

	while(1) {
		pthread_mutex_lock(&csmadata->sendqueue_lock);
		entry = sendqueue_pop_front();
		if(entry == NULL) {
			pthread_cond_wait(&csmadata->sendqueue_signal, &csmadata->sendqueue_lock);
			if(csmadata->sendthread_stop) {
				pthread_mutex_unlock(&csmadata->sendqueue_lock);
				goto exit;
			}

			pthread_mutex_unlock(&csmadata->sendqueue_lock);
			continue;
		}
		pthread_mutex_unlock(&csmadata->sendqueue_lock);

		pthread_mutex_lock(&csmadata->send_lock);
		while(1) {
			struct timespec tmp;
			clock_gettime(CLOCK_MONOTONIC, &tmp);
			if(time_cmp(&csmadata->send_lockedts, &tmp) <= 0) {
				break;
			}

			pthread_cond_timedwait(&csmadata->send_signal, &csmadata->send_lock, &csmadata->send_lockedts);
			if(csmadata->sendthread_stop) {
				pthread_mutex_unlock(&csmadata->send_lock);
				goto exit;
			}
		}
		int result = csmadata->sendimpl(entry->code, entry->rawlen, entry->repeats);
		pthread_mutex_unlock(&csmadata->send_lock);

		if(result == EXIT_FAILURE) {
			logprintf(LOG_DEBUG, "send failed (retry %d/%d)", entry->retry, max_retries);
			entry->retry++;
			if(entry->retry < max_retries) {
				pthread_mutex_lock(&csmadata->sendqueue_lock);
				sendqueue_push_front(entry);
				pthread_mutex_unlock(&csmadata->sendqueue_lock);
			} else {
				logprintf(LOG_DEBUG, "skip code");
				free(entry);
				entry = NULL;
			}
		} else {
			free(entry);
			entry = NULL;
		}
	}

exit:
	if (entry != NULL) {
		free(entry);
	}
	return NULL;
}

static int gpio433SendQueue(int *code, int rawlen, int repeats) {
	pthread_mutex_lock(&csmadata->sendqueue_lock);
	bool notify = csmadata->sendqueue_head == NULL;
	struct sendqueue_t *entry = (struct sendqueue_t*) malloc(sizeof(struct sendqueue_t));
	if(entry == NULL) {
		OUT_OF_MEMORY;
	}
	entry->code = (int *) malloc(sizeof(int)*rawlen);
	if(entry->code == NULL) {
		OUT_OF_MEMORY;
	}
	memcpy(entry->code, code, sizeof(int)*rawlen);
	entry->rawlen = rawlen;
	entry->repeats = repeats;
	entry->retry = 0;
	sendqueue_push_back(entry);
	if(notify) {
		pthread_cond_signal(&csmadata->sendqueue_signal);
	}
	pthread_mutex_unlock(&csmadata->sendqueue_lock);
	return EXIT_SUCCESS;
}

static int gpio433Send(int *code, int rawlen, int repeats) {
	if(gpio_433_out < 0) {
		usleep(10);
		return EXIT_SUCCESS;
	}

	int r = 0, x = 0;
	for(r=0;r<repeats;r++) {
		for(x=0;x<rawlen;x+=2) {
			digitalWrite(gpio_433_out, 1);
			usleep((__useconds_t)code[x]);
			digitalWrite(gpio_433_out, 0);
			if(x+1 < rawlen) {
				usleep((__useconds_t)code[x+1]);
			}
		}
	}
	digitalWrite(gpio_433_out, 0);
	return EXIT_SUCCESS;
}

static int gpio433SendCSMACD(int *code, int rawlen, int repeats) {
	struct timespec pulse_sent;
	struct timespec now;
	int repeat;
	int transmit;
	int receive;
	int first;
	int last_value;

	for(repeat=0;repeat<repeats;repeat++) {
		transmit = 0;
		receive = 0;
		first = 1;
		last_value = digitalRead(gpio_433_in);

		for(transmit=0;transmit<rawlen;transmit++) {
			int c = !(transmit&1);
			digitalWrite(gpio_433_out, c);

			clock_gettime(CLOCK_MONOTONIC, &pulse_sent);
			time_add_us(&pulse_sent, code[transmit]);

			while(true) {
				clock_gettime(CLOCK_MONOTONIC, &now);
				if(digitalRead(gpio_433_in) != last_value) {
					if(first == 0) {
						if(receive > transmit) {
							goto collision;
						}
						receive++;
					} else {
						first = 0;
					}
					last_value = !last_value;
				}
				if(time_cmp(&now, &pulse_sent) > 0) {
					break;
				}
			}
		}

		digitalWrite(gpio_433_out, 0);
	}

	return EXIT_SUCCESS;

collision:
	digitalWrite(gpio_433_out, 0);

	unsigned long duration = collision_waitmin + collision_waitunit * (rand() % collision_waitmaxunits);
	send_lock(duration, NULL);
	logprintf(LOG_DEBUG, "csma/cd: lock - collision");

	return EXIT_FAILURE;
}

/*
 * FIXME
 */
static void *receiveStop(void *param) {
#ifdef _WIN32
	InterlockedExchangeAdd(&doPause, 1);
#else
	__sync_add_and_fetch(&doPause, 1);
#endif
	return NULL;
}

static void *receiveStart(void *param) {
#ifdef _WIN32
	InterlockedExchangeAdd(&doPause, 0);
#else
	__sync_add_and_fetch(&doPause, 0);
#endif
	return NULL;
}

static void gpio433ReportRawCode(int *code, int rawlen, unsigned long duration, struct timespec* endts) {
	duration = calc_duration(code, rawlen, duration);
	pthread_mutex_lock(&csmadata->send_lock);
	if (csmadata->last_rawlen > 0 && rawlen == csmadata->last_rawlen) {
		unsigned long lock_duration = duration * wait_n_frames;
		send_lock(lock_duration, endts);
		logprintf(LOG_DEBUG, "csma: raw code deteced (%d times, %lu us)", rawlen, duration);
	}
	csmadata->last_rawlen = rawlen;
	pthread_mutex_unlock(&csmadata->send_lock);
}

static void gpio433ReportCode(int *code, int rawlen, unsigned long duration, struct timespec* endts) {
	duration = calc_duration(code, rawlen, duration);
	pthread_mutex_lock(&csmadata->send_lock);
	unsigned long lock_duration = duration * wait_n_frames;
	send_lock(lock_duration, endts);
	logprintf(LOG_DEBUG, "csma: code deteced (%d times, %lu us)", rawlen, duration);
	pthread_mutex_unlock(&csmadata->send_lock);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void gpio433Init(void) {
	hardware_register(&gpio433);
	hardware_set_id(gpio433, "433gpio");

	options_add(&gpio433->options, 'r', "receiver", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpio433->options, 's', "sender", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpio433->options, 'm', "media-access-control", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	options_add(&gpio433->options, 'a', "csma-wait-n-frames", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpio433->options, 'b', "csmacd-wait-min", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpio433->options, 'c', "csmacd-wait-unit", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpio433->options, 'd', "csmacd-wait-max-units", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");

	gpio433->minrawlen = 1000;
	gpio433->maxrawlen = 0;
	gpio433->mingaplen = 5100;
	gpio433->maxgaplen = 10000;

	mac = MACNONE;
	max_retries = 5;
	wait_n_frames = 5;
	collision_waitmin = 200000;
	collision_waitunit = 50000;
	collision_waitmaxunits = 5;
	csmadata = NULL;

	gpio433->hwtype = RF433;
	gpio433->comtype = COMOOK;
	gpio433->init = &gpio433HwInit;
	gpio433->deinit = &gpio433HwDeinit;
	gpio433->sendOOK = &gpio433Send;
	// gpio433->receiveOOK = &gpio433Receive;
	gpio433->settings = &gpio433Settings;
	gpio433->reportCode = NULL;

	eventpool_callback(REASON_SEND_BEGIN, receiveStop);
	eventpool_callback(REASON_SEND_END, receiveStart);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "433gpio";
	module->version = "2.0";
	module->reqversion = "8.0";
	module->reqcommit = NULL;
}

void init(void) {
	gpio433Init();
}
#endif
