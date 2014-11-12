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
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		ifeq (, $(findstring FreeBSD, $(SYS)))
			OSFLAGS = -isystem../../../../x-tools/arm-unknown-linux-gnueabi/sysroot/usr/include/ -nostdinc  -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
		else
			OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
		endif
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=soft -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
endif
ifneq (, $(findstring amd64, $(SYS)))
	OSFLAGS = -O3 -fPIC -march=native -mtune=native -mfpmath=sse -Wno-conversion
endif
CFLAGS = -ffast-math -rdynamic -funwind-tables $(OSFLAGS) -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wformat-nonliteral -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -isystem. -isystem.. -I../avrdude/ -I../platform/ -I../pilight/ -DHASH=\"$(GIT_HASH)\"
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)

all: $(OBJS) pilight.o

%.o: %.c %.h 
	$(GCC) $(CFLAGS) -pthread -c $(patsubst %.o,%.c,$@)

pilight.o: $(OBJS)
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -o config.o

dist-clean: clean

clean:
	rm *.o >/dev/null 2>&1 || true