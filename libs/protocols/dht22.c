/*
	Copyright (C) 2013 CurlyMo

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

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "dht22.h"

	
#define MAXTIMINGS 85

unsigned short dht22_loop = 1;

void *dht22Parse(void *param) {
	struct dirent *dir;
	struct dirent *file;

	DIR *d = NULL;
	DIR *f = NULL;
	FILE *fp;
	char *content;
	size_t bytes;
	struct stat st;
	
	int interval = 5;
	int dht_pin = 7;

	protocol_setting_get_number(dht22, "interval", &interval);	
	protocol_setting_get_number(dht22, "pin", &dht_pin);	

	
	while(dht22_loop) {


		int tries = 5;
		unsigned short got_correct_date = 0;

		while(tries > 0 && !got_correct_date) {

			uint8_t laststate = HIGH;
			uint8_t counter = 0;
			uint8_t j = 0, i;

			dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

			// pull pin down for 18 milliseconds
			pinMode(dht_pin, OUTPUT);
			digitalWrite(dht_pin, LOW);
			delay(18);
			// then pull it up for 40 microseconds
			digitalWrite(dht_pin, HIGH);
			delayMicroseconds(40);
			// prepare to read the pin
			pinMode(dht_pin, INPUT);

			// detect change and read data
			for ( i=0; i< MAXTIMINGS; i++) {
				counter = 0;
				while (sizecvt(digitalRead(dht_pin)) == laststate) {
					counter++;
					delayMicroseconds(1);
					if (counter == 255) {
						break;
					}
				}
				laststate = sizecvt(digitalRead(dht_pin));

				if (counter == 255) break;

				// ignore first 3 transitions
				if ((i >= 4) && (i%2 == 0)) {
					// shove each bit into the storage bytes
					dht22_dat[j/8] <<= 1;
					if (counter > 16)
						dht22_dat[j/8] |= 1;
						j++;
				  	}
				}

				// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
				// print it out if data is good
				if ((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {

					got_correct_date = 1;

					float t, h;
					h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
					h /= 10;
					t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
					t /= 10.0;
					if ((dht22_dat[2] & 0x80) != 0) t *= -1;

					dht22->message = json_mkobject();
					
					JsonNode *code = json_mkobject();
					
					json_append_member(code, "temperature", json_mknumber(t));
					
					json_append_member(dht22->message, "code", code);
					json_append_member(dht22->message, "origin", json_mkstring("receiver"));
					json_append_member(dht22->message, "protocol", json_mkstring(dht22->id));
					
					pilight.broadcast(dht22->id, dht22->message);
					json_delete(dht22->message);
					dht22->message = NULL;

					printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		
	  			}
				else
				{
					tries--;
				}

		}
		

		sleep((unsigned int)interval);
	}

	return (void *)NULL;
}

int dht22GC(void) {
	dht22_loop = 0;
	return 1;
}

void dht22Init(void) {
	
	gc_attach(dht22GC);

	protocol_register(&dht22);
	protocol_set_id(dht22, "dht22");
	protocol_device_add(dht22, "DHT22", "DHT22/AM2302 temperature-humidity sensor");
	dht22->devtype = WEATHER;
	dht22->hwtype = SENSOR;

	options_add(&dht22->options, 't', "temperature", has_value, config_value, "^[0-9]{1,5}$");

	protocol_setting_add_number(dht22, "decimals", 3);
	protocol_setting_add_number(dht22, "humidity", 1);
	protocol_setting_add_number(dht22, "temperature", 1);
	protocol_setting_add_number(dht22, "battery", 0);
	protocol_setting_add_number(dht22, "interval", 5);

	threads_register(&dht22Parse, (void *)NULL);

}
