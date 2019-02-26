/*
  Copyright (C) 2019 by saak2820
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "bresser_3ch.h"

#define MIN_PULSE_LENGTH    14 //488
#define MAX_PULSE_LENGTH    57 //1950
#define AVG_PULSE    		42 //(1950+965)/2

#define RAW_LENGTH        	74
#define MIN_RAW_LENGTH		130
#define MAX_RAW_LENGTH      255
#define MESSAGE_LENGTH      36
#define SYNC_LENGTH  		3890
 
#define SEP_LENGTH   		488
#define BIT1_LENGTH  		1950
#define BIT0_LENGTH  		965
#define RING_BUFFER_SIZE  	256
/*
    bresser sensor protocol     
    found on httpx://github.com/giulio93/Backeng_433mhz_Arduino    
*/

typedef struct settings_t {
    double id;
    double channel;
    double temp;
    double humi;
    struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(bre->rawlen >= MIN_RAW_LENGTH && bre->rawlen <= MAX_RAW_LENGTH) {
		return 0;        
    }
    return -1;
}

static void createMessage(int *binary, int id, int channel, double temperature, double humidity, int battery) {
    int i = 0;
    char binaryCh[MESSAGE_LENGTH];
    bre->message = json_mkobject();
    json_append_member(bre->message, "id", json_mknumber(id, 0));
    json_append_member(bre->message, "channel", json_mknumber(channel +1, 0));
    json_append_member(bre->message, "temperature", json_mknumber(temperature/10, 1));
    json_append_member(bre->message, "humidity", json_mknumber(humidity, 1));
    json_append_member(bre->message, "battery", json_mknumber(battery, 0));
}

static void parseCode(void) {
    int msg[MESSAGE_LENGTH];
    int id = 0, battery = 0, channel = 0;
	int x = 0, start = 0, c = 0;
    double temperature = 0.0, humidity = 0.0;
    double humi_offset = 0.0, temp_offset = 0.0;
    
    if (bre->rawlen > MAX_RAW_LENGTH) {
        logprintf(LOG_ERR, "bre: parsecode - invalid parameter passed %d", bre->rawlen);
        return;
    }
    
	logprintf(LOG_INFO,"bre: validate: len %d", bre->rawlen);
	
	/*
	#define SEP_LENGTH   488
	#define BIT1_LENGTH  1950
	#define BIT0_LENGTH  950
	*/
    for(x=0;x<bre->rawlen;x++) {		
        unsigned long t0 = bre->raw[x], t1 = bre->raw[(x+1)%bre->rawlen];			
        if (t0>(SEP_LENGTH-50) && t0<(SEP_LENGTH+50)) {
			if (t1>(SYNC_LENGTH-50) && t1<(SYNC_LENGTH+50)) {
				if (bre->rawlen - x + 1  >= RAW_LENGTH ) {
					start = x+1;					   
				} else {
					return;
				}
			} 
        }
    }
	
	if (start < 1) {
		return;
	}
	
	for(x=start+1;x<(start+RAW_LENGTH-1);x=(x+2)) {
		unsigned long t0 = bre->raw[x], t1 = bre->raw[x+1];
		if (t0>(SEP_LENGTH-50) && t0<(SEP_LENGTH+50)) {
         if (t1>(BIT1_LENGTH-50) && t1<(BIT1_LENGTH+50)) {
			 msg[c++] = 1;
         } else if (t1>(BIT0_LENGTH-50) && t1<(BIT0_LENGTH+50)) {
			 msg[c++] = 0;
         } else {
			return;
         }
       } else {
		   return;
       }
       
	}
	
    //01110000 10100000 01110110 11110011 0010SYNC  
    //                           1111 <== Celsius/ Fahrenheit Flag

    id = binToDecRev(msg, 0, 7);
    battery = msg[8];
    channel = binToDecRev(msg, 10, 11);
    temperature = binToSignedRev(msg, 12, 23);
    //01110000 10100000 01110110 11110011 0010SYNC  
    //                               0011 0010<== Humidity 
    humidity = binToDecRev(msg, 28, 35);

    struct settings_t *tmp = settings;
    while(tmp) {
        if(fabs(tmp->id-id) < EPSILON) {
            humi_offset = tmp->humi;
            temp_offset = tmp->temp;
            break;
        }
        tmp = tmp->next;
    }

    temperature += temp_offset;
    humidity += humi_offset;
    createMessage(msg, id, channel, temperature, humidity, battery);
}



static int checkValues(struct JsonNode *jvalues) {
    struct JsonNode *jid = NULL;

    if((jid = json_find_member(jvalues, "id"))) {
        struct settings_t *snode = NULL;
        struct JsonNode *jchild = NULL;
        struct JsonNode *jchild1 = NULL;
        double channel = -1, id = -1;
        int match = 0;

        jchild = json_first_child(jid);
        while(jchild) {
            jchild1 = json_first_child(jchild);
            while(jchild1) {
                    if(strcmp(jchild1->key, "channel") == 0) {
                    channel = jchild1->number_;
                }
                if(strcmp(jchild1->key, "id") == 0) {
                    id = jchild1->number_;
                }
                jchild1 = jchild1->next;
            }
            jchild = jchild->next;
        }

        struct settings_t *tmp = settings;
        while(tmp) {
            if(fabs(tmp->id-id) < EPSILON) {
                match = 1;
                break;
            }
            tmp = tmp->next;
        }

        if(match == 0) {
            if((snode = MALLOC(sizeof(struct settings_t))) == NULL) {
                fprintf(stderr, "out of memory\n");
                exit(EXIT_FAILURE);
            }
            snode->id = id;
            snode->channel = channel;
            snode->temp = 0;
            snode->humi = 0;

            json_find_number(jvalues, "temperature-offset", &snode->temp);
            json_find_number(jvalues, "humidity-offset", &snode->humi);

            snode->next = settings;
            settings = snode;
        }
    }
    return 0;
}



static void gc(void) {
    struct settings_t *tmp = NULL;
    while(settings) {
        tmp = settings;
        settings = settings->next;
        FREE(tmp);
    }
    if(settings != NULL) {
        FREE(settings);
    }
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void bresser3chInit(void) {
    protocol_register(&bre);
    protocol_set_id(bre, "bre");
    protocol_device_add(bre, "bre", "bresser Thermo-/Hygro-Sensor 3CH");
    bre->devtype = WEATHER;
    bre->hwtype = RF433;
    bre->maxgaplen = AVG_PULSE*PULSE_DIV;
    bre->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;
    bre->minrawlen = MIN_RAW_LENGTH;
    bre->maxrawlen = MAX_RAW_LENGTH;

    options_add(&bre->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
    options_add(&bre->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
    options_add(&bre->options, 'c', "channel", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
    options_add(&bre->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
    options_add(&bre->options, 'b', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");

    options_add(&bre->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
    options_add(&bre->options, 0, "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
    options_add(&bre->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
    options_add(&bre->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
    options_add(&bre->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
    options_add(&bre->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
    options_add(&bre->options, 0, "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

    bre->parseCode=&parseCode;
    bre->checkValues=&checkValues;
    bre->validate=&validate;
    bre->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "bre";
    module->version = "1.0";
    module->reqversion = "6.0";
    module->reqcommit = "84";
}

void init(void) {
    bresser3chInit();
}
#endif
