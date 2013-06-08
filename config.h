/*
	Copyright (C) 2013 CurlyMo

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

#ifndef _CONFIG_H_
#define _CONFIG_H_

//#define PULSE_LENGTH 	165 // wiringPi pulse length
#define PULSE_LENGTH	295

#define PORT 			5000
#define MAX_CLIENTS		30
#define BUFFER_SIZE		1025

#define SEND_REPEATS	10
#define PIDFILE			"/var/run/433-deamon.pid"

#define FREQ433			433920
#define FREQ38			38000

#endif