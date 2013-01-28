/*	
	Copyright 2012 CurlyMo
	
	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute 
	it and/or modify it under the terms of the GNU General Public License as 
	published by the Free Software Foundation, either version 3 of the License, 
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will 
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see 
	<http://www.gnu.org/licenses/>
*/

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <ctype.h>
#include <iostream>

using namespace std;

int speed = 113;
int pin_out = 0;
int start = 25;
int _short = 2;
int _long = 12; 
int end = 100;
int repeat = 10;

string decToBin(int number) {
    if(number == 0) 
		return "0";
    if(number == 1) 
		return "1";

    if(number % 2 == 0)
        return decToBin(number / 2) + "0";
    else
        return decToBin(number / 2) + "1";
}

void sendStart() {
	digitalWrite(pin_out,1);
	usleep((speed*_short));
	digitalWrite(pin_out,0);
	usleep((speed*start));
}

void sendPulse0() {
	digitalWrite(pin_out,1);
	usleep((speed*_short));
	digitalWrite(pin_out,0);
	usleep((speed*_short));
}

void sendPulse1() {
	digitalWrite(pin_out,1);
	usleep((speed*_short));
	digitalWrite(pin_out,0);
	usleep((speed*_long));
}

void sendPulseHigh() {
	sendPulse0();
	sendPulse1();
}

void sendPulseLow() {
	sendPulse1();
	sendPulse0();
}

void sendPulseOn() {
	sendPulseLow();
}

void sendPulseDim() {
	sendPulse0();
	sendPulse0();
}

void sendPulseOff() {
	sendPulseHigh();
}

void sendPulseAll() {
	sendPulseLow();
}

void sendPulseSingle() {
	sendPulseHigh();
}

void sendUniqueId(int id) {
	int x = 0;
	int l = 0;
	string c;
	
	c = decToBin(id);
	l = (int)c.length();
	for(x=0;x<(26-l);x++) {
		sendPulseHigh();
	}
	for(x=0;x<l;x++) {
		if(c[x] == '1')
			sendPulseLow();
		else
			sendPulseHigh();
	}
}

void sendUnitCode(int code) {
	int x = 0;
	int l = 0;
	string c;
	
	c = decToBin(code);
	l = (int)c.length();
	for(x=0;x<(4-l);x++) {
		sendPulseHigh();
	}
	for(x=0;x<l;x++) {
		if(c[x] == '1')
			sendPulseLow();
		else
			sendPulseHigh();
	}
}

void sendDim(int code) {
	int x = 0;
	int l = 0;
	string c;
	
	c = decToBin(code);
	l = (int)c.length();
	for(x=0;x<(4-l);x++) {
		sendPulseHigh();
	}
	for(x=0;x<l;x++) {
		if(c[x] == '1')
			sendPulseLow();
		else
			sendPulseHigh();
	}
	digitalWrite(pin_out,1);
	usleep((speed*_short));
	digitalWrite(pin_out,1);
	usleep((speed*_short));
}

void sendEnd() {
	digitalWrite(pin_out,1);
	usleep((speed*_short));
	digitalWrite(pin_out,0);
	usleep((speed*end));
}

int main(int argc, char **argv) { 

	int i = 0;
	int b = 0;
	int c = 0;
	int d = 0;
	int e = 0;
	int dim = 0;
	int opt = 0;
	int id = 0;
	int unit = 1;
	int percentage = 0;
	
	while((opt = getopt(argc, argv, "tfu:i:s:ad:r:")) != -1) {
		switch(opt) {
			case 't':
				b=0;
				d=1;
			break;
			case 'f':
				b=1;
			break;
			case 'u':
				unit = atoi(optarg);
			break;
			case 'i':
				id = atoi(optarg);
			break;
			case 's':
				speed = atoi(optarg);
			break;
			case 'a':
				c = 1;
			break;
			case 'd':
				dim = atoi(optarg);
				if(dim > 15)
					dim = 15;
				if(dim < 0)
					dim = 0;
				e =  1;
			break;
			case 'r':
				repeat = atoi(optarg);
			break;
			default:
				exit(EXIT_FAILURE);
		}
	}
		
	if(wiringPiSetup() == -1)
		return 0;
	piHiPri(99);
        pinMode(pin_out, OUTPUT);
	
	percentage = (((dim+1)*100)/16);
	if(e == 1) {
		printf("Dimmer %d%%\n",percentage);
	} else {
		if(c == 1) {
			if(d==0) {
				b=1;	
			}	
			printf("Alles ");
			if(b == 0) {
				printf("On\n");
			} else {
				printf("Off\n");
			}
		} else {
			if(b == 0) {
				printf("On\n");
			} else {
				printf("Off\n");
			}
		}
	}
	
	for(i=0;i<repeat;i++) {
		sendStart();
		sendUniqueId(id);
		if(c == 1 && e == 0) {
			if(d==0) {
				b=1;	
			}
			sendPulseAll();
			unit = 0;
		} else {
			sendPulseSingle();
		}
		if(e == 1) {
			sendPulseDim();
		} else {
			if(b==1) {
				sendPulseOff();
			} else {
				sendPulseOn();
			}
		}

		sendUnitCode(unit);
		if(e == 1) {
			sendDim(dim);
		}
		sendEnd();
	}
	//usleep(500000);
}
