#include "binary.h"
#include <stdio.h>
#include <stdlib.h>

int binToDecRev(int *binary, int s, int e) {
	int dec = 0, x = 1, i;
	for(i=s;i<=e;i++) {
		x*=2;
	}
	for(i=s;i<=e;i++) {
		x/=2;
		if(binary[i] == 1)
			dec += x;
	}
	return dec;
}

int binToDec(int *binary, int s, int e) {
	int dec = 0, x = 1, i;
	for(i=s;i<=e;i++) {
		if(binary[i] == 1)
			dec += x;
		x*=2;
	}
	return dec;
}