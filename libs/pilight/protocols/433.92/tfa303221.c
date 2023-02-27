/*
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

	- Ported to pilight 2023 by <jtheuer>
	- Changes done by Andrew Rivett <veggiefrog@gmail.com>. Copyright is
	  retained by Robert Fraczkiewicz.
	- Copyright (C) 2017 Robert Fraczkiewicz <aromring@gmail.com>

*/

#include "tfa303221.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/binary.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/gc.h"
#include "../../core/log.h"
#include "../../core/pilight.h"
#include "../protocol.h"

/*
Implementation based on previous work from
https://github.com/NorthernMan54/rtl_433_ESP/blob/master/contrib/lacrosse_tx141x.c

A single data packet looks as follows:
1) preamble - 833 us high followed by 833 us low, repeated 4 times:
	 ----      ----      ----      ----
	|    |    |    |    |    |    |    |
		  ----      ----      ----      ----
2) a train of 40 data pulses with fixed 625 us period follows immediately:
	 ---    --     --     ---    ---    --     ---
	|   |  |  |   |  |   |   |  |   |  |  |   |   |
		 --    ---    ---     --     --    ---     -- ....
A logical 1 is 417 us of high followed by 208 us of low.
A logical 0 is 208 us of high followed by 417 us of low.
Thus, in the example pictured above the bits are 1 0 0 1 1 0 1 ....

The data is grouped in 5 bytes / 10 nybbles
	[id] [id] [flags] [temp] [temp] [temp] [humi] [humi] [chk] [chk]
- id:    8 bit random integer generated at each powers up
- flags: 4 bit for battery low indicator, test button press, and channel
- temp: 12 bit unsigned temperature in degrees Celsius, scaled by 10, offset 500, range -40 C to 60
C
- humi:  8 bit integer indicating relative humidity in %.
- chk:   8 bit checksum is a digest, 0x31, 0xf4, reflected

*/

#define PULSE_MULTIPLIER 16
#define MIN_PULSE_LENGTH 208   // min zero signal after sequence (..x 34)us
#define AVG_PULSE_LENGTH 882   //
#define MAX_PULSE_LENGTH 1000  // max zero signal after sequence(..x 34)us
#define ZERO_PULSE 1300
#define ONE_PULSE 300
#define AVG_PULSE 800  // limit puls length for binary analyses
#define FINAL_PULSE_MIN_LENGTH 1000
#define MIN_RAW_LENGTH 106
#define MAX_RAW_LENGTH 116
#define RAW_LENGTH 106

#define HEADER_PULSE_GAP_MIN_LENGTH 800
#define HEADER_PULSE_COUNT 8  // 4 pairs of high-low pulses

typedef unsigned char uint8_t;

/*
 * CRC method
 */
static uint8_t lfsr_digest8_reflect(uint8_t const message[], int bytes, uint8_t gen, uint8_t key) {
	uint8_t sum = 0;
	// Process message from last byte to first byte (reflected)
	for (int k = bytes - 1; k >= 0; --k) {
		uint8_t data = message[k];
		// Process individual bits of each byte (reflected)
		for (int i = 0; i < 8; ++i) {
			// fprintf(stderr, "key is %02x\n", key);
			// XOR key into sum if data bit is set
			if ((data >> i) & 1) {
				sum ^= key;
			}

			// roll the key left (actually the lsb is dropped here)
			// and apply the gen (needs to include the dropped lsb as msb)
			if (key & 0x80)
				key = (key << 1) ^ gen;
			else
				key = (key << 1);
		}
	}
	return sum;
}

static int validate(void) {
	if (tfa303221->rawlen >= MIN_RAW_LENGTH && tfa303221->rawlen <= MAX_RAW_LENGTH &&
		tfa303221->raw[tfa303221->rawlen - 2] > FINAL_PULSE_MIN_LENGTH) {
		return 0;
	}
	return -1;
}

/*
 * Finds the index of the first payload bit (after the header of 4 pulse/gap pairs)
 * Returns -1 if no such header was found
 *
 * Example pulsetrain
 * 260 227 505 232 496 478 262 229 497 236 494 483 256 473 295 816 855 821 858 822 855 825 854 233
 * 500 232 497 481 258 471 259 228 503 475 261 226 503 234 497 477 257 234 499 475 263 224 504 233
 * 496 237 492 485 253 233 495 483 255 232 496 482 254 233 498 480 256 472 262 226 502 475 257 231
 * 501 235 502 472 256 476 262 226 501 478 260 227 500 473 259 233 502 472 264 227 502 231 498 479
 * 263 225 499 233 500 478 258 229 2056 19207
 */
static int findPayloadStartIndex(void) {
	if (tfa303221->rawlen >= MIN_RAW_LENGTH && tfa303221->rawlen <= MAX_RAW_LENGTH) {
		int headerFound = 0;
		for (int i = 0; i < tfa303221->rawlen; i++) {
			// both pulse & gap need to be > HEADER_PULSE_GAP_MIN_LENGTH
			if (tfa303221->raw[i] >= HEADER_PULSE_GAP_MIN_LENGTH) {
				headerFound++;
			} else if (headerFound > 0) {
				if (headerFound == HEADER_PULSE_COUNT) {
					return i;  // this is the first pulse/gap pair that didn't look like a header
				} else {
					return -headerFound;  // header not yet complete - broken transmission
				}
			}
		}
	}
	return -999;
}

static void parseCode(void) {
	int payloadStartIndex = findPayloadStartIndex();
	if (payloadStartIndex > 0) {
		// convert pulse-train to array of 0/1 int values
		unsigned int buffer[40];
		for (int i = 0; i < 40; i++) {	// read 40 value-pairs from raw data
			int v0 = tfa303221->raw[payloadStartIndex + (i * 2)];
			int v1 = tfa303221->raw[payloadStartIndex + (i * 2) + 1];
			if (v0 < 357 && v1 > 357) {
				buffer[i] = 0;
			} else if (v0 > 357 && v1 < 357) {
				buffer[i] = 1;
			} else {
				return;	 // found invalid v0 / v1 pair, abort parsing
			}
		}

		// the sent message checksum
		int checksum = binToDecRev(buffer, 32, 39);

		uint8_t raw[4];  // 4 bytes checksum payload
		raw[0] = binToDecRev(buffer, 0, 7);
		raw[1] = binToDecRev(buffer, 8, 15);
		raw[2] = binToDecRev(buffer, 16, 23);
		raw[3] = binToDecRev(buffer, 24, 31);
		
		// compare message checksum with computed checksum...
		int crc = lfsr_digest8_reflect(raw, 4, 0x31, 0xf4);

		if (crc != checksum) {
			return;
		}

		// found valid header, parse payload
		tfa303221->message = json_mkobject();

		// add sensor data to json
		int sensorId = binToDecRev(buffer, 0, 7);
		int battery = binToDecRev(buffer, 8, 8);
		int manual = binToDecRev(buffer, 9, 9);
		int channel = binToDecRev(buffer, 10, 11) + 1;	// bits 0..2, description 1..3
		int tempRaw = binToDecRev(buffer, 12, 23);
		float tempCelsius = (tempRaw - 500) * 0.1f;
		int humidity = binToDecRev(buffer, 24, 31);

// add debug data if enabled
#ifdef DEBUG
		json_append_member(tfa303221->message, "rawlen", json_mknumber(tfa303221->rawlen, 0));
		json_append_member(tfa303221->message, "headerIndex", json_mknumber(payloadStartIndex, 0));

		char *result = malloc(tfa303221->rawlen * 5);
		char *where = result;
		for (size_t i = 0; i < tfa303221->rawlen; i++) {
			size_t printed = sprintf(where, "%d ", tfa303221->raw[i]);
			where += printed;
		}
		json_append_member(tfa303221->message, "raw", json_mkstring(result));
		free(result);

		char *result1 = malloc(40 * 3);
		char *where1 = result1;
		for (size_t i = 0; i < 40; i++) {
			size_t printed1 = sprintf(where1, "%d ", buffer[i]);
			where1 += printed1;
		}
		json_append_member(tfa303221->message, "bytes", json_mkstring(result1));
		free(result1);

		json_append_member(tfa303221->message, "checksum", json_mknumber(checksum, 0));
		json_append_member(tfa303221->message, "checksum_calculated", json_mknumber(crc, 0));
#endif

		json_append_member(tfa303221->message, "id", json_mknumber(sensorId, 0));
		json_append_member(tfa303221->message, "channel", json_mknumber(channel, 0));
		json_append_member(tfa303221->message, "manual", json_mknumber(manual, 0));
		json_append_member(tfa303221->message, "battery", json_mknumber(battery, 0));
		json_append_member(tfa303221->message, "temperature", json_mknumber(tempCelsius, 1));
		json_append_member(tfa303221->message, "humidity", json_mknumber(humidity, 0));
	}
}

static void gc(void) {
	struct settings_t *tmp = NULL;
	while (settings) {
		tmp = settings;
		settings = settings->next;
		FREE(tmp);
	}
	if (settings != NULL) {
		FREE(settings);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void tfa303221Init(void) {

	protocol_register(&tfa303221);
	protocol_set_id(tfa303221, "tfa303221");
	protocol_device_add(tfa303221, "tfa30", "TFA 30.3221 Temp Hum Sensor");
	tfa303221->devtype = WEATHER;
	tfa303221->hwtype = RF433;
	tfa303221->minrawlen = MIN_RAW_LENGTH;
	tfa303221->maxrawlen = MAX_RAW_LENGTH;
	tfa303221->maxgaplen = MAX_PULSE_LENGTH * PULSE_DIV;
	tfa303221->mingaplen = MIN_PULSE_LENGTH * PULSE_DIV;
	tfa303221->parseCode = &parseCode;
	tfa303221->validate = &validate;
	tfa303221->gc = &gc;
}

#ifdef MODULAR
void compatibility(const char **version, const char **commit) {
	module->name = "tfa30.3221";
	module->version = "0.1";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) { tfa30Init(); }
#endif
