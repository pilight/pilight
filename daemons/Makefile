includes = $(wildcard *.h)

all: hw_default.o hw-types.o ir_remote.o receive.o transmit.o lirc.o

hw_default.o: hw_default.c $(includes)
	make trunc && gcc -Wall -I. -I.. -c hw_default.c

hw-types.o: hw-types.c $(includes)
	make trunc && gcc -Wall -I. -I.. -c hw-types.c

ir_remote.o: ir_remote.c $(includes)
	make trunc && gcc -Wall -I. -I.. -c ir_remote.c	

receive.o: receive.c $(includes)
	make trunc && gcc -Wall -I. -I.. -c receive.c	

transmit.o: transmit.c $(includes)
	make trunc && gcc -Wall -I. -I.. -c transmit.c	
	
lirc.o: hardware.h $(includes)
	ld -r hw_default.o hw-types.o ir_remote.o receive.o transmit.o -o lirc.o
	
trunc:
	test ! -s lirc.o || rm lirc.o
	test ! -s ../libs.o || rm ../libs.o	
	test ! -s ../send || rm ../send
	test ! -s ../receive || rm ../receive