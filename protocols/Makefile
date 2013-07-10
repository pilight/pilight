GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS=-march=native -mtune=native -mfpmath=sse
endif
ifneq (, $(findstring arm, $(SYS)))
	ifneq (, $(findstring gnueabihf, $(SYS)))
		OSFLAGS=-mfloat-abi=hard -mfpu=vfp -march=armv6
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		OSFLAGS=-mfloat-abi=hard -mfpu=vfp -march=armv6
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS=-mfloat-abi=soft -mfpu=vfp -march=armv6
	endif
endif
CFLAGS = -Ofast -ffast-math $(OSFLAGS) -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -g -Wall -static -I. -I.. -I../libs/
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)
INCLUDES = $(wildcard ../libs/*.h) ../libs/libs.o

all: $(OBJS) protocols.o

%.o: %.c %.h $(INCLUDES)
	$(GCC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

protocols.o: $(OBJS)
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -o protocols.o

clean:
	rm *.o >/dev/null 2>&1 || true