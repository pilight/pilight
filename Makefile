SUBDIRS = lirc protocols

all: subdirs protocol.o binary.o options.o gc.o config.o libs.o 433-send 433-receive 433-debug 433-learn 433-daemon
	
subdirs:
	make -C protocols/
	make -C lirc/
	
protocol.o: protocol.c protocol.h
	make trunc && gcc -Wall -I. -I.. -c protocol.c

options.o: options.c options.h
	make trunc && gcc -Wall -I. -I.. -c options.c	
	
binary.o: binary.c binary.h
	make trunc && gcc -Wall -I. -I.. -c binary.c
	
gc.o: gc.c gc.h
	make trunc && gcc -Wall -I. -I.. -c gc.c

config.o: config.c config.h
	make trunc && gcc -Wall -I. -I.. -c config.c -lconfig
	
libs.o:
	ld -r binary.o protocol.o options.o gc.o config.o protocols/protocols.o lirc/lirc.o -o libs.o

433-send: send.c lirc.h
	gcc -O2 -g -Wall -static -I. -I.. -o 433-send send.c libs.o -lconfig
	
433-receive: receive.c
	gcc -O2 -g -Wall -static -I. -I.. -o 433-receive receive.c

433-debug: debug.c lirc.h
	gcc -O2 -g -Wall -static -I. -I.. -o 433-debug debug.c libs.o -lm -lconfig
	
433-learn: learn.c lirc.h
	gcc -O2 -g -Wall -static -I. -I.. -o 433-learn learn.c libs.o -lm -lconfig
	
433-daemon: daemon.c lirc.h
	gcc -O2 -g -Wall -static -I. -I.. -o 433-daemon daemon.c libs.o -lpthread -lconfig
	
clean:
	rm -f libs.o protocols/*.o lirc/*.o *.o send receive
	
trunc:
	test ! -s 433-send || rm 433-send
	test ! -s 433-receive || rm 433-receive
	test ! -s 433-debug || rm 433-debug
	test ! -s 433-learn || rm 433-learn
	test ! -s 433-daemon || rm 433-daemon
	test ! -s libs.o || rm libs.o
