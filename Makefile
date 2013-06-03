SUBDIRS = daemons protocols

all: subdirs protocol.o binary.o options.o libs.o send receive debug
	
subdirs:
	make -C protocols/
	make -C daemons/
	
protocol.o: protocol.c protocol.h
	make trunc && gcc -Wall -I. -I.. -c protocol.c

options.o: options.c options.h
	make trunc && gcc -Wall -I. -I.. -c options.c	
	
binary.o: binary.c binary.h
	make trunc && gcc -Wall -I. -I.. -c binary.c

libs.o:
	ld -r binary.o protocol.o options.o protocols/protocols.o daemons/lirc.o -o libs.o

send: send.c lirc.h
	gcc -O2 -g -Wall -I. -I.. -static -o send send.c libs.o
	
receive: receive.c lirc.h
	gcc -O2 -g -Wall -I. -I.. -static -o receive receive.c libs.o

debug: debug.c lirc.h
	gcc -O2 -g -Wall -I. -I.. -static -o debug debug.c libs.o
	
clean:
	rm -f libs.o protocols/*.o daemons/*.o *.o send receive
	
trunc:
	test ! -s send || rm send
	test ! -s receive || rm receive
	test ! -s libs.o || rm libs.o