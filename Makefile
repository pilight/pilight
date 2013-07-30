GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS = -Ofast -fPIC -march=native -mtune=native -mfpmath=sse -Wconversion -Wunreachable-code -Wstrict-prototypes 
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
	OSFLAGS = -O3 -fPIC -march=native -mtune=native -mfpmath=sse -Wno-conversion
endif
CFLAGS = -ffast-math $(OSFLAGS) -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -I. -I.. -Ilibs/ -Iprotocols/ -Ilirc/ -I/usr/include/ -L/usr/lib/arm-linux-gnueabihf/
SUBDIRS = libs protocols lirc
SRC = $(wildcard *.c)
INCLUDES = $(wildcard protocols/*.o) $(wildcard lirc/*.o) $(wildcard libs/*.h) $(wildcard libs/*.o)
PROGAMS = $(patsubst %.c,433-%,$(SRC))
LIBS = libs/libs.o protocols/protocols.o lirc/lirc.o

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS) all

$(SUBDIRS):
	$(MAKE) -C $@

all: $(LIBS) lib433.so.1 lib433.a $(PROGAMS) 

lib433.so.1:
	$(GCC) $(LIBS) -shared -o lib433.so.1 -lpthread -lm
	cp lib433.so.1 /usr/local/lib/
	ldconfig
	
lib433.a:
	ar -rsc lib433.a $(LIBS)
	cp lib433.a /usr/local/lib/

433-daemon: daemon.c $(INCLUDES) $(LIBS)
	$(GCC) $(CFLAGS) -lpthread -lm -o $@ $(patsubst 433-%,%.c,$@) lib433.so.1 

433-send: send.c $(INCLUDES) lib433.so.1 
	$(GCC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) lib433.so.1 

433-receive: receive.c $(INCLUDES) lib433.so.1 
	$(GCC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) lib433.so.1 

433-debug: debug.c $(INCLUDES) lib433.so.1 
	$(GCC) $(CFLAGS) -lm -o $@ $(patsubst 433-%,%.c,$@) lib433.so.1 

433-learn: learn.c $(INCLUDES) lib433.so.1 
	$(GCC) $(CFLAGS) -lm -o $@ $(patsubst 433-%,%.c,$@) lib433.so.1 

433-control: control.c $(INCLUDES) lib433.so.1 
	$(GCC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) lib433.so.1 

clean:
	rm 433-* >/dev/null 2>&1 || true
	rm *.so* || true
	rm *.a* || true
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done