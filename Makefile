GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS = -Ofast -march=native -mtune=native -mfpmath=sse -Wconversion -Wunreachable-code -Wstrict-prototypes 
endif
ifneq (, $(findstring arm, $(SYS)))
	ifneq (, $(findstring gnueabihf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=soft -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
endif
ifneq (, $(findstring amd64, $(SYS)))
	OSFLAGS = -O3 -march=native -mtune=native -mfpmath=sse -Wno-conversion
endif
CFLAGS = -ffast-math $(OSFLAGS) -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -I. -I.. -Ilibs/ -Iprotocols/ -Ilirc/ -I/usr/include/ -L/usr/lib/arm-linux-gnueabihf/
SUBDIRS = libs protocols lirc
SRC = $(wildcard *.c)
INCLUDES = $(wildcard protocols/*.o) $(wildcard lirc/*.o) $(wildcard libs/*.h) $(wildcard libs/*.o)
PROGAMS = $(patsubst %.c,qpido-%,$(SRC))
LIBS = libs/libs.o protocols/protocols.o lirc/lirc.o

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS) all

$(SUBDIRS):
	$(MAKE) -C $@

all: $(LIBS) libqpido.so.1 libqpido.a $(PROGAMS) 

libqpido.so.1:
	$(GCC) $(LIBS) -shared -o libqpido.so.1 -lpthread -lm
	cp libqpido.so.1 /usr/local/lib/
	ldconfig
	
libqpido.a:
	ar -rsc libqpido.a $(LIBS)
	cp libqpido.a /usr/local/lib/

qpido-daemon: daemon.c $(INCLUDES) libqpido.so.1
	$(GCC) $(CFLAGS) -lpthread -lm -o $@ $(patsubst qpido-%,%.c,$@) libqpido.so.1

qpido-send: send.c $(INCLUDES) libqpido.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst qpido-%,%.c,$@) libqpido.so.1

qpido-receive: receive.c $(INCLUDES) libqpido.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst qpido-%,%.c,$@) libqpido.so.1

qpido-debug: debug.c $(INCLUDES) libqpido.so.1
	$(GCC) $(CFLAGS) -lm -o $@ $(patsubst qpido-%,%.c,$@) libqpido.so.1

qpido-learn: learn.c $(INCLUDES) libqpido.so.1
	$(GCC) $(CFLAGS) -lm -o $@ $(patsubst qpido-%,%.c,$@) libqpido.so.1

qpido-control: control.c $(INCLUDES) libqpido.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst qpido-%,%.c,$@) libqpido.so.1

clean:
	rm qpido-* >/dev/null 2>&1 || true
	rm *.so* || true
	rm *.a* || true
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done