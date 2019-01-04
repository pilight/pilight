/*
	Copyright (C) 2018

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

/*
 *  this code is based on tfa protocol (see tfa.[ch]) and adjusted according to RTL_433 project code for HIDEKI
 * */

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
#include "hideki.h"

#define HIDEKI_MANCHESTER_ENCODING

// TODO: need to check/adjust these constants for HIDEKI
#define PULSE_MULTIPLIER	13
#define MIN_PULSE_LENGTH	220
#define MAX_PULSE_LENGTH	280	// FREETEC NC7104-675, Globaltronics GT-WT-01
#define AVG_PULSE_LENGTH	235
#define RAW_LENGTH			88
#define MIN_RAW_LENGTH		76	// SOENS, NC7104-675, GT-WT-01
#define MED_RAW_LENGTH		86	// TFA
#define MAX_RAW_LENGTH		88	// DOSTMAN 32.3200
//#define PULSE_HIDEKI_WEATHER_LOWER 	750
//#define PULSE_HIDEKI_WEATHER_UPPER	1250
#define PULSE_NINJA_WEATHER_SHORT		412
#define PULSE_NINJA_WEATHER_LOWER	309	// SHORT*0,75
#define PULSE_NINJA_WEATHER_UPPER		515	// SHORT * 1,25

#define HIDEKI_MAX_BYTES_PER_ROW 14

/*
 * The received bits are inverted.
 * Every 8 bits are stuffed with a (even) parity bit.
 * The payload (excluding the header) has an byte parity (XOR) check
 * The payload (excluding the header) has CRC-8, poly 0x07 init 0x00 check
 * The payload bytes are reflected (LSB first / LSB last) after the CRC check
 *
 *    11111001 0  11110101 0  01110011 1 01111010 1  11001100 0  01000011 1  01000110 1  00111111 0  00001001 0  00010111 0
 *    SYNC+HEAD P   RC cha P     LEN   P     Nr.? P   .1° 1°  P   10°  BV P   1%  10% P     ?     P     XOR   P     CRC   P
 *
 * TS04:
 *    00000000  11111111  22222222  33333333  44444444  55555555  66666666  77777777  88888888 99999999
 *    SYNC+HEAD cha   RC     LEN        Nr.?    1° .1°  VB   10°   10%  1%     ?         XOR      CRC
 *
 * Wind:
 *    00000000  11111111  22222222  33333333  44444444  55555555  66666666  77777777  88888888 99999999 AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD
 *    SYNC+HEAD cha   RC     LEN        Nr.?    1° .1°  VB   10°    1° .1°  VB   10°   1W .1W  .1G 10W   10G 1G    w°  AA    XOR      CRC
 *
 * Rain:
 *    00000000  11111111  22222222  33333333  44444444  55555555  66666666  77777777  88888888
 *    SYNC+HEAD cha   RC   B LEN        Nr.?   RAIN_L    RAIN_H     0x66       XOR       CRC
 *
 */

enum sensortypes { HIDEKI_UNKNOWN, HIDEKI_TEMP, HIDEKI_TS04, HIDEKI_WIND, HIDEKI_RAIN };

typedef struct settings_t {
	double id;
	double channel;
	double temp;
	double humi;
	double wind_dir;
	double wind_factor;
	double rain;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static uint8_t reverse8(uint8_t x)
{
    x = (x & 0xF0) >> 4 | (x & 0x0F) << 4;
    x = (x & 0xCC) >> 2 | (x & 0x33) << 2;
    x = (x & 0xAA) >> 1 | (x & 0x55) << 1;
    return x;
}

static int byteParity(uint8_t inByte)
{
    inByte ^= inByte >> 4;
    inByte &= 0xf;
    return (0x6996 >> inByte) & 1;
}

static void set_bit(uint8_t *array, int position) {
	uint8_t mask = 1 << (position % 8);
	array[position / 8] |= mask;
}

#if 0
static void reset_bit(uint8_t *array, int position) {
	uint8_t mask = (1 << (position % 8)) ^ 0xff;
	array[position / 8] &= mask;
}
#endif

static int validate(void) {
	int x = 0, i = 0;
	logprintf(LOG_DEBUG, "HIDEKI validate(): rawlen=%d", hideki->rawlen);
	if(hideki->rawlen == MIN_RAW_LENGTH || hideki->rawlen == MED_RAW_LENGTH || hideki->rawlen == MAX_RAW_LENGTH) {
		if(hideki->raw[hideki->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   hideki->raw[hideki->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
#if 0
#ifdef HIDEKI_MANCHESTER_ENCODING
		uint8_t header = 0;
		for(x=0; x<8; x++) {
			if(hideki->raw[i] > PULSE_HIDEKI_WEATHER_LOWER &&
					hideki->raw[i] < PULSE_HIDEKI_WEATHER_UPPER) {
				set_bit(&header, x);
				i++;
			} else {
				 // leave value unchanged to 0
			}
			i++;
		}
#else
			// extract first byte of raw data...
			uint8_t header = 0;
			for(x=0; x<16; x+=2) {
				if(hideki->raw[x] > AVG_PULSE_LENGTH*PULSE_MULTIPLIER) {
					set_bit(&header, i++);
				} else {
					i++; // leave value unchanged to 0
				}
			}
			header = reverse8(header) ^ 0xFF;
			logprintf(LOG_DEBUG, "HIDEKI validate(): header=0x%X", header);
			if (header == 0x9F)
#endif
#endif
				return 0;
		}
	}
	return -1;
}

static void parseCode(void) {
	uint8_t binary[RAW_LENGTH/2];
	int id = 0;
	int i = 0, x;
	double humi_offset = 0.0, temp_offset = 0.0, wind_dir_offset = 0.0, wind_factor = 1.0, rain_factor = 1.0;
	uint8_t packet[HIDEKI_MAX_BYTES_PER_ROW];
	int sensortype = HIDEKI_WIND; // default for 14 valid bytes
    uint8_t channel, rc, battery_ok;
    int temp, rain_units;
    double temperature, humidity, wind_strength, wind_direction, rain;

#ifdef HIDEKI_MANCHESTER_ENCODING
	// Decode Biphase Mark Coded Differential Manchester (BMCDM) pulse stream into binary
	memset(&binary[0], sizeof(binary));
	for(x=0; x<=(MAX_RAW_LENGTH/2); x++) {
		if(hideki->raw[i] > PULSE_HIDEKI_WEATHER_LOWER &&
				hideki->raw[i] < PULSE_HIDEKI_WEATHER_UPPER) {
			set_bit(binary, x);
			i++;
		} else {
			 // leave value unchanged to 0
		}
		i++;
	}
#else
    memset(&binary[0], sizeof(binary));
	for(x=0; x<hideki->rawlen-2; x+=2) {
		if(hideki->raw[x] > AVG_PULSE_LENGTH*PULSE_MULTIPLIER) {
			set_bit(binary, i++);
		} else {
			i++; // leave value unchanged to 0
		}
	}
#endif

	// Transform incoming data:
	//  * reverse MSB/LSB
	//  * invert all bits
	//  * Remove (and check) parity bit
	for (int i = 0; i < HIDEKI_MAX_BYTES_PER_ROW; i++) {
		unsigned int offset = i/8;
	    packet[i] = (binary[i+offset] << (i%8)) | (binary[i+offset+1] >> (8 - i%8)); // skip/remove parity bit
	    packet[i] = reverse8(packet[i]); // reverse LSB first to LSB last
	    packet[i] ^= 0xFF; // invert bits
	    // check parity
	    uint8_t parity = ((binary[i+offset+1] >> (7 - i%8)) ^ 0xFF) & 0x01;
	    if (parity != byteParity(packet[i]))
	    {
	    	if (i == 10) {
	    		sensortype = HIDEKI_TS04;
	            break;
	         }
	    	if (i == 9) {
	    		sensortype = HIDEKI_RAIN;
	            break;
	        }
	    	if (i == 8) {
	    		sensortype = HIDEKI_TEMP;
	            break;
	        }
			logprintf(LOG_DEBUG, "HIDEKI parseCode(): unsupported sensor type");
	        return; // no success
	    }
	}

	logprintf(LOG_DEBUG, "HIDEKI parseCode(): %02X %02X %02X %02X %02X %02X %02X %02X",
			packet[0], packet[1], packet[2], packet[3], packet[4], packet[5], packet[6], packet[7]);

	// TODO: move this check into the validate() function
    if (packet[0] != 0x9f) // NOTE: other valid ids might exist
        return;

    // decode the sensor values
    channel = (packet[1] >> 5) & 0x0F;
     if (channel >= 5) channel -= 1;
     rc = packet[1] & 0x0F; // rolling code
     temp = (packet[5] & 0x0F) * 100 + ((packet[4] & 0xF0) >> 4) * 10 + (packet[4] & 0x0F);
     if (((packet[5]>>7) & 0x01) == 0) {
         temp = -temp;
     }
     battery_ok = (packet[5]>>6) & 0x01;
     id = packet[3]; // probably some ID

     // read some settings
 	 struct settings_t *tmp = settings;
 	 while(tmp) {
 		if(fabs(tmp->channel-channel) < EPSILON) {
 			humi_offset = tmp->humi;
 			temp_offset = tmp->temp;
 			wind_dir_offset = tmp->wind_dir;
 			wind_factor = tmp->wind_factor;
 			rain_factor = tmp->rain;
 			break;
 		}
 		tmp = tmp->next;
 	 }

     if (sensortype == HIDEKI_TS04) {
    	  temperature = (double)temp/10 + temp_offset;
          humidity = ((packet[6] & 0xF0) >> 4) * 10 + (packet[6] & 0x0F) + humi_offset;

          hideki->message = json_mkobject();
          json_append_member(hideki->message, "temperature", json_mknumber(temperature, 2));
          json_append_member(hideki->message, "humidity", json_mknumber(humidity, 2));

          json_append_member(hideki->message, "battery", json_mknumber(battery_ok, 0));
          json_append_member(hideki->message, "channel", json_mknumber(channel, 0));
          json_append_member(hideki->message, "id", json_mknumber(id, 0));
          json_append_member(hideki->message, "rc", json_mknumber(rc, 0));

      } else  if (sensortype == HIDEKI_WIND) {
          const uint8_t wd[] = { 0, 15, 13, 14, 9, 10, 12, 11, 1, 2, 4, 3, 8, 7, 5, 6 };
    	  temperature = (double)temp/10 + temp_offset;
    	  wind_strength = (double)(packet[9] & 0x0F) * 100 + ((packet[8] & 0xF0) >> 4) * 10 + (packet[8] & 0x0F); // unit still unclear
    	  wind_strength *= wind_factor; // user defined adjustment
    	  // wind strength in km/h = wind_strength*0.160934f
          wind_direction = (double)(wd[((packet[11] & 0xF0) >> 4)] * 225);
          wind_direction /= 10;
          wind_direction += wind_dir_offset; // user defined adjustment
#if 0
          if (wind_direction > 360.0)
        	  wind_direction -= 360.0;
#endif
          hideki->message = json_mkobject();
          json_append_member(hideki->message, "temperature", json_mknumber(temperature, 2));
          json_append_member(hideki->message, "wind strength", json_mknumber(wind_strength, 2));
          json_append_member(hideki->message, "wind direction", json_mknumber(wind_direction, 2));

          json_append_member(hideki->message, "battery", json_mknumber(battery_ok, 0));
          json_append_member(hideki->message, "channel", json_mknumber(channel, 0));
          json_append_member(hideki->message, "id", json_mknumber(id, 0));
          json_append_member(hideki->message, "rc", json_mknumber(rc, 0));

      } else if (sensortype == HIDEKI_TEMP) {
    	 temperature = (double)temp/10 + temp_offset;

         hideki->message = json_mkobject();
         json_append_member(hideki->message, "temperature", json_mknumber(temperature, 2));

         json_append_member(hideki->message, "battery", json_mknumber(battery_ok, 0));
         json_append_member(hideki->message, "channel", json_mknumber(channel, 0));
         json_append_member(hideki->message, "id", json_mknumber(id, 0));
         json_append_member(hideki->message, "rc", json_mknumber(rc, 0));

     } else if (sensortype == HIDEKI_RAIN) {
         rain_units = (packet[5] << 8) + packet[4];
         rain = (double)rain_units * 0.7; // in mm
         rain *= rain_factor;	// user defined adjustment
         battery_ok = (packet[2]>>6) & 0x01;

         hideki->message = json_mkobject();
         json_append_member(hideki->message, "rain", json_mknumber(rain, 2));

         json_append_member(hideki->message, "battery", json_mknumber(battery_ok, 0));
         json_append_member(hideki->message, "channel", json_mknumber(channel, 0));
         json_append_member(hideki->message, "id", json_mknumber(id, 0));
         json_append_member(hideki->message, "rc", json_mknumber(rc, 0));
     }
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
			if(fabs(tmp->id-id) < EPSILON && fabs(tmp->channel-channel) < EPSILON) {
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
			snode->wind_dir = 0;
			snode->wind_factor = 1.0;;
			snode->rain = 1.0;

			json_find_number(jvalues, "temperature-offset", &snode->temp);
			json_find_number(jvalues, "humidity-offset", &snode->humi);
			json_find_number(jvalues, "wind-direction-offset", &snode->wind_dir);
			json_find_number(jvalues, "wind-factor", &snode->wind_factor);
			json_find_number(jvalues, "rain-factor", &snode->rain);

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
void hidekiInit(void) {
	protocol_register(&hideki);
	protocol_set_id(hideki, "hideki");
	protocol_device_add(hideki, "TS04", "HIDEKI TS04 sensor");
	protocol_device_add(hideki, "Hideki-Wind", "HIDEKI Wind sensor");
	protocol_device_add(hideki, "Hideki-Temp", "HIDEKI Temperature sensor");
	protocol_device_add(hideki, "Hideki-Rain", "HIDEKI Rain sensor");
	hideki->devtype = WEATHER;
	hideki->hwtype = RF433;
	hideki->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	hideki->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;
	hideki->minrawlen = MIN_RAW_LENGTH;
	hideki->maxrawlen = MAX_RAW_LENGTH;

	// device values
	options_add(&hideki->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&hideki->options, "h", "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&hideki->options, "w", "wind", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&hideki->options, "d", "wind_direction", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&hideki->options, "r", "rain", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&hideki->options, "b", "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[01]$");
	options_add(&hideki->options, "o", "rc", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]"); // rolling code
	// device IDs
	options_add(&hideki->options, "c", "channel", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&hideki->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	// device settings:
	options_add(&hideki->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&hideki->options, "0", "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&hideki->options, "0", "wind-direction-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&hideki->options, "0", "wind-factor", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "^[0-9]{1,5}$");
	options_add(&hideki->options, "0", "rain-factor", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "^[0-9]{1,5}$");
	// GUI  settings:
	options_add(&hideki->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&hideki->options, "0", "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&hideki->options, "0", "wind-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&hideki->options, "0", "wind_direction-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&hideki->options, "0", "rain-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&hideki->options, "0", "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&hideki->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&hideki->options, "0", "show-battery", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&hideki->options, "0", "show-wind", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&hideki->options, "0", "show-wind_direction", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&hideki->options, "0", "show-rain", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&hideki->options, "0", "show-rc", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	hideki->parseCode=&parseCode;
	hideki->checkValues=&checkValues;
	hideki->validate=&validate;
	hideki->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "hideki";
	module->version = "0.1";
	module->reqversion = "1.0";
	module->reqcommit = "85";
}

void init(void) {
	hidekiInit();
}
#endif
