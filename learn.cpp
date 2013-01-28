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

//gcc -Wall -pedantic -o learn learn.cpp -L/usr/local/lib -lwiringPi -lstdc++ 

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include <ctype.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

int iPinIn = 2;
int iSpeed = 113;
int iZero = 0;
int iOne = 0;
int iTime = 0;

int iRemoteBit = 0;
//int iRemoteGap = 0;
int iRemoteEnd = 0;
int iCodeLength = 0;
int iHighValue = 0;
int iLowValue = 0;
int iSwitchBit = 0;

int aNoiseArrayZero[9999];
int aNoiseArrayOne[9999];
int aIntArray[9999];
//int iNoiseOne = 15;
//int iNoiseZero = 21;
//int iRemoteBit = 0;
//int iRemoteGap = 141;
//int iRemoteEnd = 88;
//int iCodeLength = 133;
int iDebug = 1;
double iMarge = 0.7;
int bValidRange = 1;
string sCode;
vector<string> aCode;

string itos(int i) {
    ostringstream s;
    s << i;
    return s.str();
}

vector<string> explode( const string &delimiter, const string &str) {
    vector<string> arr;

    int strleng = str.length();
    int delleng = delimiter.length();
    if (delleng==0)
        return arr;//no change

    int i=0;
    int k=0;
    while( i<strleng ) {
        int j=0;
        while (i+j<strleng && j<delleng && str[i+j]==delimiter[j])
            j++;
        if (j==delleng) {
            arr.push_back(  str.substr(k, i-k) );
            i+=delleng;
            k=i;
        }
        else {
            i++;
        }
    }
    arr.push_back(str.substr(k, i-k));
    return arr;
}

string createBinaryString(string code) {
	int a = 0;
	string binary;
	vector<string> acode;
	
	acode=explode(";",code);

	for(a=0;a<((int)acode.size());a+=4) {
		if((a+iSwitchBit) < (int)acode.size()) {
			if(atoi(acode[a+iSwitchBit].c_str()) > ((iLowValue+iHighValue)/2)) {
				binary.append("0");
			} else if(atoi(acode[a+iSwitchBit].c_str()) < ((iLowValue+iHighValue)/2)) {
				binary.append("1");
			}
		}
	}
	return binary;
}

void getNoiseChar() {
	cout << endl;
	cout << "The first step is to analyse the noise the receiver is recieving." << endl;
	cout << "Do not press any buttons on your remote controller(s)." << endl << endl;
	
	for(int i=0;i<9999;i++) {
		aNoiseArrayZero[i]=0;
	}
	
	for(int i=0;i<9999;i++) {
		aNoiseArrayOne[i]=0;
	}
	
	iTime = 0;
	while(iTime < 50000) {
		usleep(iSpeed);
		if(digitalRead(iPinIn) == 1) {
			iOne++;
			if(iZero > 0) {
				aNoiseArrayZero[iZero]++;
				if((iTime%100) == 1) {
					cout << ".";
					cout << flush;
				}
				//iTime=0;
			}
			iZero=0;
		}
		if(digitalRead(iPinIn) == 0) {
			iZero++;
			/*if(iOne > 0) {
				aNoiseArrayOne[iOne]++;
				if((iTime%100) == 1) {
					cout << ".";
					cout << flush;
				}
				//iTime=0;
			}*/
			iOne=0;
		}
	iTime++;
	}
	/*for(int i=0;i<9999;i++) {
		if(aNoiseArrayZero[i]>1) {
			cout << i << " " << aNoiseArrayZero[i] << endl;
		}
	}*/
	/*for(int i=0;i<9999;i++) {
		if(aNoiseArrayOne[i]>1) {
			cout << i << " " << aNoiseArrayOne[i] << endl;
		}
	}*/
	/*if(iNoiseZero > 50 || iNoiseOne > 50) {
		cout << endl << endl;
		cout << "There are devices interfering with the receiver," << endl;
		cout << "please turn off your tv, radio, mobile etc. near the receiver." << endl;
		cout << endl;
		cout << "Aborting..." << endl;
		cout << endl;
	}*/
	cout << endl << endl;
}

/*void getRemoteGap() {
	cout << endl;
	cout << "The second step is to figure out the start and stop signal of the remote" << endl;
	cout << "Please keep pressing the same button until the right values has been captured" << endl << endl;
	
	iTime = 0;
	while(iTime < 10000) {
		usleep(iSpeed);
		bValidRange=1;
		if(digitalRead(iPinIn) == 1) {
			iOne++;
			for(int i=0;i<9999;i++) {
				if(aNoiseArrayZero[i] > 50 && iZero <= i && bValidRange==1) {
					bValidRange=0;
					break;
				}
			}
			if(bValidRange) {
				if(iZero > iRemoteGap) {
					iRemoteGap = iZero;
					iRemoteBit = 0;
					cout << ".";
					cout << flush;
					iTime=0;
				}
			}
			iZero=0;
		}
		if(digitalRead(iPinIn) == 0) {
			iZero++;
			for(int i=0;i<9999;i++) {
				if(aNoiseArrayOne[i] > 50 && iOne <= i && bValidRange==1) {
					bValidRange=0;
					break;
				}
			}
			if(bValidRange) {
				if(iOne > iRemoteGap) {
					iRemoteGap = iOne;
					iRemoteBit = 1;
					cout << ".";
					cout << flush;
					iTime=0;
				}
			}
			iOne=0;
		}
	iTime++;
	}
	if(iDebug == 1) {
		cout << endl << endl;
		cout << "Gap: " << iRemoteGap << endl;
		cout << "Bit: " << iRemoteBit << endl;
	}
	cout << endl << endl;
}*/

void getRemoteEnd() {
	cout << endl;
	cout << "The second step is to figure out the length of the last signal of the remote" << endl;
	cout << "Please keep pressing the same button until the right values has been captured" << endl << endl;
	
	for(int i=0;i<9999;i++) {
		aIntArray[i]=0;
	}
	
	iTime = 0;
	while(iTime < 100) {
		usleep(iSpeed);
		bValidRange=1;
		if(digitalRead(iPinIn) == 1) {
			iOne++;
			for(int i=0;i<9999;i++) {
				if(aNoiseArrayZero[i] > 1 && iZero <= i && bValidRange==1) {
					bValidRange=0;
					break;
				}
			}
			if(bValidRange) {
				//cout << iZero << endl;
				aIntArray[iZero]++;
				cout << ".";
				cout << flush;
				iTime++;
			}
			iZero=0;
		}
		if(digitalRead(iPinIn) == 0) {
			iZero++;
			/*for(int i=0;i<9999;i++) {
				if(aNoiseArrayOne[i] > 1 && iOne <= i && bValidRange==1) {
					bValidRange=0;
					break;
				}
			}
			if(bValidRange) {
				//cout << iOne << endl;
				aIntArray[iOne]++;
				if((iTime%10) == 1) {
					cout << ".";
					cout << flush;
					}
				iTime++;
			}*/
			iOne=0;
		}
	}
	cout << endl << endl;
	if(iDebug == 1) {
		iRemoteEnd=0;
		int x=0;
		for(int i=0;i<9999;i++) {
			if(aIntArray[i] > x) {
				iRemoteEnd = i;
				x=aIntArray[i];
			}
		}
		cout << endl;
		cout << "End: " << iRemoteEnd << endl;
	}
	cout << endl;
}

void getCodeLength() {
	cout << endl;
	cout << "The third step is to figure out the code length of the buttons of your remote." << endl;
	cout << "Please press different buttons until the right value have been captured." << endl << endl;

	for(int i=0;i<9999;i++) {
		aIntArray[i]=0;
	}

	iTime = 0;
	while(iTime < 100) {
		usleep(iSpeed);
		if(digitalRead(iPinIn) == 1) {
			iOne++;
			if(iZero > 0) {
				sCode.append(itos(iZero));
				sCode.append(";");
			}
			iZero=0;
		}
		if(digitalRead(iPinIn) == 0) {
			iZero++;
			if(iOne > 0) {
				sCode.append(itos(iOne));
				sCode.append(";");
			}
			iOne=0;
		}
		//if(iRemoteBit == 0) {
			if(iZero >= (iRemoteEnd*iMarge)) {
				aCode = explode(";",sCode);
				if(aCode.size() > 1) {
					cout << ".";
					cout << flush;
					aIntArray[aCode.size()]++;
					iTime++;
					iZero=0;
					iOne=0;
				}
				sCode.clear();
			}
		//} else {
			/*if(iOne >= (iRemoteEnd*iMarge)) {
				aCode = explode(";",sCode);
				if(aCode.size() > 1) {
					cout << ".";
					cout << flush;
					aIntArray[aCode.size()]++;
					iTime++;
					iZero=0;
					iOne=0;
				}
				sCode.clear();
			}
		}*/
	}
	iCodeLength=0;
	int x=0;
	for(int i=0;i<9999;i++) {
		if(aIntArray[i] > x) {
			iCodeLength = i;
			x=aIntArray[i];
		}
	}
	cout << endl;
	if(iDebug==1) {
		cout << endl;
		cout << "Length: " << iCodeLength << endl;
		
	}
	cout << endl;
}

void getCodeChar() {
	cout << endl;
	cout << "The last step is figuring out what the high and the low values of the remote are." << endl;
	cout << "Again keep pressing some buttons." << endl << endl;

	iTime = 0;
	while(iTime < 50) {
		usleep(iSpeed);
		if(digitalRead(iPinIn) == 1) {
			iOne++;
			if(iZero > 0) {
				sCode.append(itos(iZero));
				sCode.append(";");
			}
			iZero=0;
		}
		if(digitalRead(iPinIn) == 0) {
			iZero++;
			if(iOne > 0) {
				sCode.append(itos(iOne));
				sCode.append(";");
			}
			iOne=0;
		}
		//if(iRemoteBit == 0) {
			if(iZero >= (iRemoteEnd*iMarge)) {
				aCode = explode(";",sCode);
				if((int)aCode.size() == iCodeLength) {
					cout << ".";
					cout << flush;
					for(int i=0;i<9999;i++) {
						aIntArray[i]=0;
					}
					for(int x=0;x<(int)aCode.size();x++) {
						aIntArray[atoi(aCode[x].c_str())]++;
					}
				iTime++;				
				}
				if(iTime < 50) {
					sCode.clear();
				}
			}
		/*} else {
			if(iOne >= (iRemoteEnd*iMarge)) {
				aCode = explode(";",sCode);
				if((int)aCode.size() == iCodeLength) {
					cout << sCode << endl;
				}
			}
		}*/
	}
	for(int i=0;i<9999;i++) {
		if(aIntArray[i] > 10) {
			if(iLowValue < aIntArray[i]) {
				if(iLowValue == 0 || (iLowValue > 0 && iLowValue >= (i-1) && iLowValue <= (i+1))) {
					iLowValue = i;
				}
			}
		}
	}
	for(int i=0;i<9999;i++) {
		if(aIntArray[i] > 10) {
			if(iHighValue < aIntArray[i] && i > iLowValue) {
				if(iHighValue == 0 || (iHighValue > 0 && iHighValue >= (i-1) && iHighValue <= (i+1))) {
					iHighValue = i;
				}
			}
		}
	}
	int iMatch=0;
	string sBinary;
	for(int i=1;i<4;i++) {
		iMatch=0;
		iSwitchBit=i;
		sBinary=createBinaryString(sCode);
		for(int x=0;x<(int)sBinary.length();x++) {
			if(sBinary[x] == '0') {
				iMatch++;
			}
		}
		if(iMatch > 3 && ((iMatch+1) < (int)sBinary.length())) {
			break;
		}
	}
	if(iDebug==1) {
		cout << endl;
		cout << "Low: " << iLowValue << endl;
		cout << "High: " << iHighValue << endl;
		cout << "SwitchBit: " << iSwitchBit << endl;		
	}
	
	cout << createBinaryString(sCode) << endl;
	
	cout << endl;		
}


int main() {
	if(wiringPiSetup() == -1)
		return 0;
	piHiPri(99);
	pinMode(iPinIn, INPUT);

	iTime = 0;
	getNoiseChar();
	//getRemoteGap();
	getRemoteEnd();
	getCodeLength();
	getCodeChar();
}