CC = $(CROSS_COMPILE)gcc
CFLAGS = -Ofast -ffast-math -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -g -Wall -I. -I.. -Ilibs/ -Iprotocols/ -Ilirc/ -I/usr/include/ -L/usr/lib/arm-linux-gnueabihf/
SUBDIRS = libs protocols lirc
SRC = $(wildcard *.c)
INCLUDES = $(wildcard protocols/*.o) $(wildcard lirc/*.o) $(wildcard libs/*.h) $(wildcard libs/*.o)
PROGAMS = $(patsubst %.c,433-%,$(SRC))
LIBS = libs/libs.o protocols/protocols.o lirc/lirc.o -lpthread -lm

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS) all

$(SUBDIRS):
	$(MAKE) -C $@
 
all: $(LIBS) $(OBJS) $(PROGAMS) 
 
433-daemon: daemon.c $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) $(LIBS)

433-send: send.c $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) $(LIBS)

433-receive: receive.c $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) $(LIBS)

433-debug: debug.c $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) $(LIBS)	

433-learn: learn.c $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) $(LIBS)

433-control: control.c $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) $(LIBS)
	
clean:
	rm $(PROGRAMS) >/dev/null 2>&1 || true
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done