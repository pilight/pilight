GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS = -fPIC -Ofast -march=native -mtune=native -mfpmath=sse
endif
ifneq (, $(findstring arm, $(SYS)))
	ifneq (, $(findstring gnueabihf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=soft -mfpu=vfp -march=armv6
	endif
endif
ifneq (, $(findstring amd64, $(SYS)))
	OSFLAGS = -fPIC -O3 -march=native -mtune=native -mfpmath=sse
endif
CFLAGS = -ffast-math $(OSFLAGS) -g -Wall -static -isystem. -isystem.. -I../pilight/
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)

all: $(OBJS) websockets.o

%.o: %.c %.h
	$(GCC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

websockets.o: $(OBJS)
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -o websockets.o

dist-clean: clean

clean:
	rm *.o >/dev/null 2>&1 || true