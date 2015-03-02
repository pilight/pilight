GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
OS := $(shell uname)

ifdef CROSS_COMPILE
	INCLUDE = -isystem/usr/include/ -isystem/usr/include/x86_64-linux-gnu/
endif
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS = -Ofast -fPIC -march=native -mtune=native -mfpmath=sse -Wconversion -Wunreachable-code -Wstrict-prototypes 
endif
ifneq (, $(findstring arm, $(SYS)))
	ifneq (, $(findstring gnueabihf, $(SYS)))
		OSFLAGS = -fno-omit-frame-pointer -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		ifeq (, $(findstring FreeBSD, $(SYS)))
			OSFLAGS = -fno-omit-frame-pointer -isystem../../../../x-tools/arm-unknown-linux-gnueabi/sysroot/usr/include/ -nostdinc  -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
		else
			OSFLAGS = -fno-omit-frame-pointer -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
		endif
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS = -fno-omit-frame-pointer -Ofast -mfloat-abi=soft -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
endif
ifneq (, $(findstring amd64, $(SYS)))
	OSFLAGS = -O3 -fPIC -march=native -mtune=native -mfpmath=sse -Wno-conversion
endif
CFLAGS = -ffast-math -rdynamic -funwind-tables $(OSFLAGS) -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wformat-nonliteral -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -isystem. -isystem.. -I/usr/local/include/ -I../avrdude/ -I../platform/ -I../config/ -I../actions/ -lunwind -DHASH=\"$(GIT_HASH)\"
SRC = $(wildcard *.c)
OBJS = $(filter-out wiringX.o mongoose.o http.o json.o firmware.o ping.o, $(SRC:.c=.o))

all: json.o wiringX.o http.o mongoose.o firmware.o ping.o $(OBJS) lpilight.o

json.o: json.c json.h
	$(GCC) -ffast-math $(OSFLAGS) -Wno-conversion -g -isystem. -isystem.. -c $(patsubst %.o,%.c,$@)

wiringX.o: wiringX.c wiringX.h
	$(GCC) -ffast-math $(OSFLAGS) -w -g -isystem. -isystem.. -pthread -lm -c $(patsubst %.o,%.c,$@) -I../platform/
	
mongoose.o: mongoose.c mongoose.h
	$(GCC) -ffast-math $(OSFLAGS) -w -g -isystem. -isystem.. -pthread -lm -c $(patsubst %.o,%.c,$@)

firmware.o: firmware.c firmware.h
	$(GCC) $(CFLAGS) -c -g $(patsubst %.o,%.c,$@) -isystem. -isystem.. $(INCLUDE)

ping.o: ping.c ping.h
	$(GCC) $(CFLAGS) -c -w -g $(patsubst %.o,%.c,$@) -isystem. -isystem.. $(INCLUDE)	
	
%.o: %.c %.h 
	$(GCC) $(CFLAGS) -pthread -c $(patsubst %.o,%.c,$@)

lpilight.o: $(OBJS) json.o http.o wiringX.o mongoose.o firmware.o ping.o
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -g json.o http.o wiringX.o mongoose.o firmware.o ping.o -o lpilight.o

dist-clean: clean

clean:
	rm *.o >/dev/null 2>&1 || true