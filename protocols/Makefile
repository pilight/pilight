all: kaku_switch.o kaku_dimmer.o elro.o protocols.o

kaku_switch.o: kaku_switch.c kaku_switch.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -c kaku_switch.c

kaku_dimmer.o: kaku_dimmer.c kaku_dimmer.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -c kaku_dimmer.c && make trunc

elro.o: elro.c elro.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -c elro.c && make trunc
	
protocols.o:
	ld -r kaku_switch.o kaku_dimmer.o elro.o -o protocols.o
	
trunc:
	test ! -s protocols.o || rm protocols.o
	test ! -s ../libs.o || rm ../libs.o
	test ! -s ../send || rm ../send
	test ! -s ../receive || rm ../receive
