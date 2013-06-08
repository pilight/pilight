all: kaku_switch.o kaku_dimmer.o kaku_old.o elro.o raw.o protocols.o

kaku_switch.o: kaku_switch.c kaku_switch.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -DHAVE_CONFIG_H -c kaku_switch.c

kaku_dimmer.o: kaku_dimmer.c kaku_dimmer.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -DHAVE_CONFIG_H -c kaku_dimmer.c && make trunc
	
kaku_old.o: kaku_old.c kaku_old.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -DHAVE_CONFIG_H -c kaku_old.c && make trunc

elro.o: elro.c elro.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -DHAVE_CONFIG_H -c elro.c && make trunc

raw.o: raw.c raw.h ../protocol.h
	make trunc && gcc -Wall -I. -I.. -DHAVE_CONFIG_H -c raw.c && make trunc
	
protocols.o:
	ld -L/usr/local/lib -r kaku_switch.o kaku_dimmer.o kaku_old.o elro.o raw.o -o protocols.o
	
trunc:
	test ! -s protocols.o || rm protocols.o
	make trunc -C ../
