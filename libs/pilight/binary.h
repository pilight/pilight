/*
	Copyright (C) 2013 - 2014 CurlyMo

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

#ifndef _BINARY_H_
#define _BINARY_H_

int binToDecRev(int *binary, int s, int e);
int binToDec(int *binary, int s, int e);
int decToBinRev(int nr, int *binary);
int decToBin(int n, int binary[]);

unsigned long long binToDecRevUl(int *binary, unsigned int s, unsigned int e);
unsigned long long binToDecUl(int *binary, unsigned int s, unsigned int e);
int decToBinUl(unsigned long long n, int binary[]);
int decToBinRevUl(unsigned long long n, int binary[]);

#endif
